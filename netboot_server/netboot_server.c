/*
    Validate requests and paths.
    Think about security.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "netboot/netboot.h"

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    typedef SOCKET socket_t;
    #define close_socket(s) closesocket(s)
    
    WSADATA wsaData;
    #define socket_error() WSAGetLastError()
#else
    #include <unistd.h>
    #include <time.h>
    #include <errno.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    // #include <netdb.h>
    #include <pthread.h>
    typedef int socket_t;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define close_socket(s) close(s)

    #define socket_error() errno
#endif


const char* base_directory = "releases/elos-0.1.0-x86_64/fs";

int PORT = NETBOOT_DEFAULT_PORT;


char g_recv_buffer[0x10000];
char g_send_buffer[0x10000];
char g_thread_send_buffer[0x10000];

#define TIME_RESOLUTION 1000000LLU

// Through testing 8 seems to work best for QEMU
// I will try on real hardware.
#define MAX_TRANSFER_INFOS 8
#define MAX_SESSIONS 10
#define SESSION_IDLE_TIME (5 * TIME_RESOLUTION)
#define RESEND_IDLE_TIME (TIME_RESOLUTION / 3) // if server is in australia and client in sweden then you will need to increase this value
#define MAX_RESEND_ATTEMPTS 5

// #define debug(...) printf(__VA_ARGS__)
#define debug(...)

typedef struct TransferInfo {
    bool active;
    uint16_t size;
    uint64_t offset;
    uint64_t sent_time;
    int attempts;
} TransferInfo;

typedef struct Session {
    uint32_t address;
    uint16_t port;
    char path[256];
    // bool active;
    TransferInfo transferInfos[MAX_TRANSFER_INFOS];

    uint64_t fileTotalSize;
    uint64_t file_offset;
    uint64_t file_size;

    uint64_t lastAccessTime;
    uint64_t startedTime;
    int resends;

    FILE* file;
    struct sockaddr_in client;
} Session;


uint64_t access_time_now();
Session* create_session(uint32_t address, uint16_t port);
Session* find_session(uint32_t address, uint16_t port);

typedef void*(*FN_Thread)(void*);

void thread_loss_detection(void*);
void refresh_transfers(Session* session);
void send_file_packet(Session* session, TransferInfo* info, void* send_buffer);

void work();

socket_t listenSocket;


const char* ipv4_str(char address[4], char* buffer) {
    snprintf(buffer, 20, "%u.%u.%u.%u", 
        address[0], address[1], address[2], address[3]
    );
    return buffer;
}

int main(int argc, char** argv) {
    int res;

#ifdef _WIN32
    res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (res != 0) {
        printf("WSAStartup failed: %d\n", res);
        return 1;
    }
#endif

    listenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listenSocket == INVALID_SOCKET) {
        printf("Could not make listen socket: %d\n", socket_error());
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    // addr.sin_addr.s_addr = inet_addr("192.168.100.50");
    addr.sin_addr.s_addr = INADDR_ANY;

    res = bind(listenSocket, (struct sockaddr*)&addr, sizeof(addr));
    if (res == SOCKET_ERROR) {
        printf("bind failed: %d\n", socket_error());
        return 1;
    }


    // @TODO NetBOOT file send protcol needs to communicate over a transfer rate.
    //   server may send stuff too fast or slow and client may receive too slow or fast.
    //   packets may be dropped if we're too fast. we need to resend.

    #ifdef _WIN32
        HANDLE handle = CreateThread(NULL, 0x10000, (DWORD(*)(void*))thread_loss_detection, NULL, 0, NULL);
    #else
        pthread_t thread;
        res = pthread_create(&thread, NULL, (FN_Thread) thread_loss_detection, NULL);
    #endif

    char ip_buffer[30];
    printf("NetBoot server listening on %s:%d\n", ipv4_str((char*)&addr.sin_addr, ip_buffer), PORT);

    work();
}


Session sessions[MAX_SESSIONS];

#define SESSION_IS_ACTIVE(SES) (now - SES->lastAccessTime < SESSION_IDLE_TIME)

// An ethernet frame can have about 1500 bytes without being fragmented.
// We use 1400 to make room for Ethernet, IPv4, UDP, and NetBoot headers.
#define PAYLOAD_LIMIT 1400


uint64_t access_time_now() {
    #ifdef _WIN32
        // performance counter
        LARGE_INTEGER freq;
        LARGE_INTEGER perf;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&perf);
        return perf.QuadPart * TIME_RESOLUTION / freq.QuadPart;
    #else
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        return (uint64_t)tp.tv_sec * TIME_RESOLUTION + (uint64_t)tp.tv_nsec * TIME_RESOLUTION / 1000000000LLU;
    #endif
}

Session* create_session(uint32_t address, uint16_t port) {
    uint64_t now = access_time_now();
    Session* new_session = NULL;
    for (int i=0;i<MAX_SESSIONS;i++) {
        Session* session = &sessions[i];
        if (!SESSION_IS_ACTIVE(session) || (session->address == address && session->port == port)) {
            if (session->file) {
                fclose(session->file);
            }
            memset(session, 0, sizeof(*session));
            if (!new_session) {
                new_session = session;
            }
        }
    }
    if (!new_session)
        return NULL;

    memset(new_session, 0, sizeof(*new_session));
    new_session->address = address;
    new_session->port = port;
    new_session->lastAccessTime = now;
    new_session->startedTime = now;
    return new_session;
}

Session* find_session(uint32_t address, uint16_t port) {
    Session* new_session = NULL;
    for (int i=0;i<MAX_SESSIONS;i++) {
        Session* session = &sessions[i];
        if (session->address == address && session->port == port) {
            return session;
        }
    }
    return NULL;
}

void stop_session(Session* session) {
    session->lastAccessTime = 0;
}


void work() {
    int res;
    struct sockaddr_in client;
    int client_len = sizeof(client);

    while (1) {

        int bytes = recvfrom(listenSocket, g_recv_buffer, sizeof(g_recv_buffer), 0, (struct sockaddr*)&client, (void*)&client_len);
        if (bytes == SOCKET_ERROR) {
            printf("recvfrom failed: %d\n", socket_error());
            continue;
        }

        // debug("Received %d bytes\n", bytes);

        NetBoot_Header* header = (NetBoot_Header*)g_recv_buffer;
        if (memcmp(header->magic, NETBOOT_MAGIC, 4)) {
            // Drop non-netboot messages.
            printf("Not boot message '%*.s'\n", 4, header->magic);
            continue;
        }
        if (header->version != 1) {
            // Drop non-netboot messages.
            printf("Incompatible netboot version: %d\n", header->version);
            continue;
        }

        // printf("Port %d\n", __builtin_bswap16(client.sin_port));

        if (header->type == NETBOOT_REQUEST_FILE) {
            NetBoot_Request_File* req = (NetBoot_Request_File*)header;
            const char* path = req->filePath;

            char fullpath[256];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", base_directory, path);
            
            FILE* file = fopen(fullpath, "rb");
            if (!file) {
                printf("Cannot request %s, does not exist.\n", fullpath);
                // @TODO Tell the boot client. For now they'll figure it out because of no response.
                break;
            }

            fseek(file, 0, SEEK_END);
            int totalFileSize = ftell(file);
            fseek(file, 0, SEEK_SET);


            Session* session = create_session(*(uint32_t*)&client.sin_addr, client.sin_port);
            snprintf(session->path, sizeof(session->path), "%s", path);
            session->file        = file;
            session->file_offset = req->offset;
            session->file_size   = req->size;
            session->fileTotalSize = totalFileSize;
            session->client      = client;
            // Clamp size to total file size
            if (req->offset + req->size > totalFileSize) {
                session->file_size = totalFileSize - req->offset;
            }

            printf("Request %s off=%d bytes=%d\n", fullpath, (int)session->file_offset, (int)session->file_size);

            refresh_transfers(session);

        } else if (header->type == NETBOOT_SEND_FILE_ACK) {
            Session* session = find_session(*(uint32_t*)&client.sin_addr, client.sin_port);

            NetBoot_Send_File_Ack* ack = (NetBoot_Send_File_Ack*)header;

            int completed_any = false;
            for (int i=0;i<MAX_TRANSFER_INFOS;i++) {
                TransferInfo* info = &session->transferInfos[i];
                if (info->active && info->offset == ack->offset && info->size == ack->size) {
                    info->active = false;
                    completed_any = true;
                    break;
                }
            }
            if (completed_any) {
                debug("ACK off=%d size=%d\n", ack->offset, ack->size);

                if (session->file_size == 0) {
                    
                    bool finished = true;
                    int locked_transfers = 0;
                    for (int i=0;i<MAX_TRANSFER_INFOS;i++) {
                        TransferInfo* info = &session->transferInfos[i];
                        if (info->active) {
                            finished = false;
                            if (info->attempts >= MAX_RESEND_ATTEMPTS) {
                                locked_transfers++;
                            }
                            break;
                        }
                    }
                    if (finished) {
                        uint64_t diff = access_time_now() - session->startedTime;
                        float seconds = (float)diff / (float)TIME_RESOLUTION;
                        printf("Finished %s in %.3f s (resends=%d, lockedTransfers=%d)\n", session->path, seconds, session->resends, locked_transfers);
                        stop_session(session);
                    }
                } else {
                    refresh_transfers(session);
                }
            } else {
                printf("warning: ACK off=%d size=%d did not complete any Transfer (duplicates?).\n", (int)ack->offset, ack->size);
                for (int i=0;i<MAX_TRANSFER_INFOS;i++) {
                    TransferInfo* info = &session->transferInfos[i];
                    debug("  Transfer[%d] off=%d size=%d\n", i, (int)info->offset, info->size);
                }
            }
        }
    }
}

void thread_loss_detection(void* arg) {
    while (1) {
        uint64_t now = access_time_now();
        
        for (int si=0;si<MAX_SESSIONS;si++) {
            Session* session = &sessions[si];
            
            // printf("Sesh %d now=%d idle=%d\n", si, now, session->lastAccessTime, SESSION_IDLE_TIME);
            if (!SESSION_IS_ACTIVE(session))
                continue;
            
            // printf("Sesh %d now=%d idle=%d\n", si, now, RESEND_IDLE_TIME);
            for (int ti=0;ti<MAX_TRANSFER_INFOS;ti++) {
                TransferInfo* info = &session->transferInfos[ti];

                debug("Inf[%d] act=%d off=%d size=%d tim=%d\n", ti, info->active, info->offset, info->size, now-info->sent_time);
                if (info->active && now - info->sent_time > RESEND_IDLE_TIME) {
                    if (info->attempts > MAX_RESEND_ATTEMPTS) {
                        // Block/lock this transfer info.
                        continue;
                    }
                    info->attempts++;

                    // RESEND!
                    // debug("Resend off=%d size=%d\n", info->offset, info->size);
                    // printf("Resend off=%d size=%d\n", info->offset, info->size);
                    session->resends++;
                    info->sent_time = access_time_now();
                    send_file_packet(session, info, g_thread_send_buffer);
                }
            }
        }
        // printf("Refresh %lf\n", (double)access_time_now()/(double)TIME_RESOLUTION);
        #ifdef _WIN32
            Sleep(45);
        #else
            usleep(50*1000);
        #endif
    }
}

void refresh_transfers(Session* session) {
    for (int i=0;i<MAX_TRANSFER_INFOS;i++) {
        TransferInfo* info = &session->transferInfos[i];

        if (info->active) {
            continue;
        }
        // if (info->size <= 0) {
        //     // Done or we are waiting for the last ACKs
        //     continue;
        // }
        memset(info, 0, sizeof(*info));
        info->sent_time = access_time_now();
        info->offset = session->file_offset;
        if (session->file_size > PAYLOAD_LIMIT) {
            info->size = PAYLOAD_LIMIT;
        } else {
            info->size = session->file_size;
        }

        session->file_size -= info->size;
        session->file_offset += info->size;
        // debug("DECR %d %d\n", session->file_offset, session->file_size);
        // printf("Completion %d (%d)\n", session->file_size, session->file_offset);

        session->lastAccessTime = access_time_now();
        send_file_packet(session, info, g_send_buffer);

        info->active = true;
    }
}

void send_file_packet(Session* session, TransferInfo* info, void* send_buffer) {
    NetBoot_Send_File* sendf = (NetBoot_Send_File*)send_buffer;
    memcpy(sendf->header.magic, NETBOOT_MAGIC, 4);
    sendf->header.version = 1;
    sendf->header.type = NETBOOT_SEND_FILE;

    sendf->totalFileSize = session->fileTotalSize;
    sendf->offset = info->offset;
    sendf->size = info->size;

    fseek(session->file, sendf->offset, SEEK_SET);
    int read_bytes = fread(sendf->payload, 1, sendf->size, session->file);
    if (read_bytes != sendf->size) {
        printf(" bytes were not read! %d\n", (int)read_bytes);
    }
    
    int packet_size = sizeof(NetBoot_Send_File) + sendf->size;

    debug("Send off=%d size=%d\n", sendf->offset, sendf->size);

    size_t sent_bytes = sendto(listenSocket, send_buffer, packet_size,
        0, (struct sockaddr*)&session->client, sizeof(session->client));

    if (sent_bytes != packet_size) {
        printf(" bytes were not sent! %d\n", (int)sent_bytes);
    }
}