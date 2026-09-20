
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stdnet.h"

#include "elos/elos.h"
#include "elos/common/intrinsics.h"
#include "elos/common/string.h"



u64  g_ticks_per_second;
ELOS_Net_Handle g_net_handle;


void recv_packet();
void send_packet(int packedId);
int main();


void _start() {

    // @NOCHECKIN Temporary
    // dumpdir("/", 0);

    SYS_ticks_per_second(&g_ticks_per_second);

    int exitCode = main();

    SYS_exit(exitCode);
}

int main() {

    ELOS_Net_Address address = {0};
    address.protocol = ELOS_NET_PROTO_UDP_IPV4;
    address.udp_tcp4.address = net_ipv4_from_str("127.0.0.1");
    address.udp_tcp4.port    = 5001;

    ELOS_Error error = net_open(&address, &g_net_handle);
    
    if (error != ELOS_OK) {
        printf("NET_OPEN was not OK\n");
        return 1;
    }

    printf("NET_OPEN success, handle=%p\n", g_net_handle);

    int packetId = 0;

    while (1) {
        // recv_packet();
        send_packet(packetId++);

        SYS_sleep_ns(1000 * 1000000);
    }


    net_close(g_net_handle);

    return 0;
}


void recv_packet() {
    ELOS_Error error;
    
    ELOS_Net_Address address = {0};

    char messageBuffer[512];
    u32 bufferSize = sizeof(messageBuffer);

    error = net_read(g_net_handle, &address, messageBuffer, &bufferSize);
    if (error != ELOS_OK) {
        printf("recv_packet: Error %s\n", elos_error(error));
    } else {
        printf("recv_packet: Read message\n");
    }

    printf("%s\n", messageBuffer);

}

void send_packet(int packedId) {
    ELOS_Net_Address address = {0};
    address.protocol = ELOS_NET_PROTO_UDP_IPV4;
    //  10.255.255.254
    // address.udp_tcp4.address = net_ipv4_from_str("10.255.254");
    address.udp_tcp4.address = net_ipv4_from_str("192.168.100.50");
    address.udp_tcp4.port    = 5002;

    char messageBuffer[512];
    int len = snprintf(messageBuffer, sizeof(messageBuffer), "packetid %d", packedId);
    
    ELOS_Error error;
    
    error = net_write(g_net_handle, &address, messageBuffer, len);
    if (error != ELOS_OK) {
        printf("send_packet: Error %s\n", elos_error(error));
    } else {
        printf("send_packet: Sent message\n");
    }
}
