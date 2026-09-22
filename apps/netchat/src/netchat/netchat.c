
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
    address.udp_tcp4.address = 0; // any address
    address.udp_tcp4.port    = 5001;

    ELOS_Error error = net_open(&address, &g_net_handle);
    
    if (error != ELOS_OK) {
        printf("NET_OPEN was not OK\n");
        return 1;
    }

    printf("netchat: Started %s:%u\n", net_ipv4_str(address.udp_tcp4.address), address.udp_tcp4.port);

    int packetId = 0;

    while (1) {
        recv_packet();
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
    u32 bufferSize = sizeof(messageBuffer)-1;
    messageBuffer[0] = 0;

    error = net_read(g_net_handle, &address, messageBuffer, &bufferSize, 0);
    if (error == ELOS_ERR_TIMEOUT) {
        // no packets
    } else if (error != ELOS_OK) {
        printf("recv_packet: Error %s\n", elos_error(error));
    } else {
        messageBuffer[bufferSize] = 0;
        printf("netchat server: %s\n", messageBuffer);
    }
}

void send_packet(int packedId) {
    ELOS_Net_Address address = {0};
    address.protocol = ELOS_NET_PROTO_UDP_IPV4;
    address.udp_tcp4.port    = 5002;
    
    char messageBuffer[512];
    int len;
    ELOS_Error error;

    // address.udp_tcp4.address = net_ipv4_from_str("169.254.0.2");

    // len = snprintf(messageBuffer, sizeof(messageBuffer), "packetid %d", packedId);
    
    
    // error = net_write(g_net_handle, &address, messageBuffer, len);
    // if (error != ELOS_OK) {
    //     printf("Could not send packet, %s\n", elos_error(error));
    // } else {
    //     printf("sent: %s\n", messageBuffer);
    // }
    
    address.udp_tcp4.address = net_ipv4_from_str("192.168.0.60");

    len = snprintf(messageBuffer, sizeof(messageBuffer), "packetid %d", packedId);
    
    error = net_write(g_net_handle, &address, messageBuffer, len);
    if (error != ELOS_OK) {
        printf("Could not send packet, %s\n", elos_error(error));
    } else {
        printf("sent: %s\n", messageBuffer);
    }
}
