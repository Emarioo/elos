
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main() {
    int res;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        printf("Badsocket %d\n", sock);
        return 1;

    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 5000;
    addr.sin_addr.s_addr = 0x3664a8c0; // 192.168.100.54

    res = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (res == -1) {
        printf("Badconnect %d\n", res);
        return 1;
    }

    int value = 100;

    while (1) {
        char buffer[1024];

        *(int*)buffer = value;

        printf("Send %d\n", value);
        res = write(sock, buffer, 4);
        if (res == -1) {
            printf("Badwrite %d\n", res);
            return 1;
        }
        res = read(sock, buffer, 4);
        if (res == -1) {
            printf("Badread %d\n", res);
            return 1;
        }
        value = *(int*)buffer;
        printf("Read %d\n", value);
    }

    return 0;
}
