/*
 * udp_server.c
 *
 * Linux:
 *     gcc udp_server.c -o udp_server
 *     ./udp_server 5002
 *
 * Windows with MinGW:
 *     gcc udp_server.c -o udp_server.exe -lws2_32
 *     udp_server.exe 5002
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32

    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>

    typedef SOCKET socket_t;

    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_CODE WSAGetLastError()

#else

    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int socket_t;

    #define INVALID_SOCKET (-1)
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_CODE errno

#endif


#define BUFFER_SIZE 2048


static void print_address(const struct sockaddr_in *address)
{
    char ip[INET_ADDRSTRLEN];

    if (inet_ntop(
            AF_INET,
            &address->sin_addr,
            ip,
            sizeof(ip)) == NULL) {

        strcpy(ip, "?");
    }

    printf("%s:%u",
           ip,
           (unsigned)ntohs(address->sin_port));
}


int main(int argc, char **argv)
{
    uint16_t port = 5002;

    if (argc >= 2) {
        port = (uint16_t)atoi(argv[1]);
    }

#ifdef _WIN32

    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

#endif


    socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sock == INVALID_SOCKET) {
        printf("socket() failed: %d\n", SOCKET_ERROR_CODE);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    /*
     * Allow the server to reuse the port after restarting.
     */
    int reuse = 1;

    if (setsockopt(
            sock,
            SOL_SOCKET,
            SO_REUSEADDR,
            (const char *)&reuse,
            sizeof(reuse)) < 0) {

        printf("setsockopt(SO_REUSEADDR) failed: %d\n",
               SOCKET_ERROR_CODE);
    }


    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);


    if (bind(
            sock,
            (struct sockaddr *)&server_address,
            sizeof(server_address)) < 0) {

        printf("bind() failed: %d\n", SOCKET_ERROR_CODE);

        CLOSE_SOCKET(sock);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }


    printf("UDP server listening on port %u\n", port);
    printf("Waiting for packets...\n");


    uint8_t buffer[BUFFER_SIZE];

    while (1) {

        struct sockaddr_in client_address;
#ifdef _WIN32
        int client_address_size = sizeof(client_address);
#else
        socklen_t client_address_size = sizeof(client_address);
#endif

        int received = recvfrom(
            sock,
            (char *)buffer,
            sizeof(buffer) - 1,
            0,
            (struct sockaddr *)&client_address,
            &client_address_size);

        if (received < 0) {
            printf("recvfrom() failed: %d\n",
                   SOCKET_ERROR_CODE);
            break;
        }


        /*
         * Make the received data printable as a string.
         */
        buffer[received] = 0;


        printf("\nReceived %d bytes from ", received);
        print_address(&client_address);
        printf("\n");

        printf("Data: %s\n", buffer);


        /*
         * Send a response back to the exact source
         * address and source port.
         */
        char response[BUFFER_SIZE];

        int response_length = snprintf(
            response,
            sizeof(response),
            "server received: %s",
            buffer);

        if (response_length < 0) {
            continue;
        }

        if (response_length >= (int)sizeof(response)) {
            response_length = sizeof(response) - 1;
        }


        int sent = sendto(
            sock,
            response,
            response_length,
            0,
            (struct sockaddr *)&client_address,
            client_address_size);

        if (sent < 0) {
            printf("sendto() failed: %d\n",
                   SOCKET_ERROR_CODE);
            break;
        }


        printf("Sent %d bytes back to ", sent);
        print_address(&client_address);
        printf("\n");
    }


    CLOSE_SOCKET(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}