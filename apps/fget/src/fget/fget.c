
#include "fget/fget.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


// Rough regex for hostname: [0-9a-z-](.[0-9a-z-])*
int extractPartsFromURL(const char* url, char* hostName, char* filePath, uint16_t* port) {
    int head = strlen("http://");
    int textlen = strlen(url);
    int start = head;

    while (head < textlen) {
        int chr = url[head];
        head++;

        if ((chr >= 'a' && chr <= 'z') || (chr >= '0' && chr <= '9') || chr == '-') {
            continue;
        } else {
            if (head == start) {
                return -1;
            } else {
                head--;
                break;
            }
        }
    }

    // @TODO Is head-start larger than hostName buffer?
    memcpy(hostName, url + start, head - start);
    hostName[head - start] = '\0';

    if (url[head] == ':') {
        uint16_t acc_port = 0;
        start = head;
        while (head < textlen) {
            int chr = url[head];
            head++;

            if ((chr >= '0' && chr <= '9')) {
                acc_port = acc_port * 10 + chr - '0';
                continue;
            } else {
                head--;
                break;
            }
        }

        if (head - start < 0) {
            *port = acc_port;
        }
    }

    if (url[head] == '/') {
        start = head;
        while (head < textlen) {
            int chr = url[head];
            head++;

            if (chr != '?') {
                // @TODO Sanitize path for bad characters.
                continue;
            } else {
                if (head == start) {
                    return -1;
                } else {
                    head--;
                    break;
                }
            }
        }


        if (head - start > 0) {
            // @TODO Is head-start larger than filePath buffer?
            memcpy(filePath, url + start, head - start);
            filePath[head - start] = '\0';
        }
    }

    return 0;
}

int resolveHostName(const char* hostName, uint32_t* address) {
    // We resolve by sending DNS packets.
    // You need special capability to send arbitrary network packets.
    // OS provides function SYS_NET_RESOLVE_HOSTNAME or similar which sends the packets
    // and caches the responses.

    *address = 0;
    return -1;
}

int download_blob(const char* url, const char* path) {
    int result;

    if (!strncmp(url, "http://", 7)) {

        char     hostName[256];
        char     filePath[256];
        uint16_t port;
        uint32_t address;

        result = extractPartsFromUrl(url, &hostName, &filePath, &port);
        if (result < 0)  return -1;
        result = resolveHostName(hostName, &address);
        if (result < 0)  return -1;


        int socket = net_connect(address, port);

        char message[512];

        int messageLength = snprintf(
            message, sizeof(message),
            "GET /%s HTTP/1.1\r\n"
            "\r\n",
            filePath
        );

        net_send(socket, message, messageLength);

        
        net_recv(socket, message, messageLength);
    }

    return 0;
}

int download_blob_memory(const char* url, void* buffer, size_t* maxSize) {


}


