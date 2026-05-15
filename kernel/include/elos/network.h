/*
    @TODO Who owns the packet buffer memory?

    Is it the caller who should free it? Does caller not own it and NET just handles it?

    To be efficient we do not want to malloc, copy around and free a bunch of buffers.
    This introduces latency.
    
    NET copies data from network controller into a buffer from a buffer pool.
    It is sent to OS space where it is processed and distributed to
    appropriate process. The user space now has access to this packet buffer.

    If the user wants to send a packet they ask OS -> Kernel -> NET for a packet
    buffer of some size (allowing resizing, comes from buffer pool).
    They fill it up and send it to NET. NET sends data to controller
    and collects the buffer into the pool where it can be reused again.

*/


#pragma once

#include "elos/common/types.h"


//###############################
//          TYPES
//###############################


typedef void* NetDevice;

typedef struct NET_Packet {
    char* buffer;
    int   size;
} NET_Packet;

typedef void(*FN_NET_recv_packet)(NetDevice device, NET_Packet* packet, void* user_data);

typedef struct NET_DeviceInfo {
    u8 mac[6];
} NET_DeviceInfo;

//######################################
//     NETWORK CONTROLLER FUNCTIONS
//######################################


/*
    Scans the computer for network controllers (devices) and initializes them
    for sending and receiving packets.
    
    Function will do nothing if you already scanned and retrieved devices.
    Any devices detected but not returned (either because first time, earlier call to cleanup,
    or to small device count) will be returned.

    If 'devices' argument is NULL then count will be set to devices found.

    @note Thread-safe

    @param devices Array of devices to fetch, initialize and return.
    @param max_count Max number of devices to initialize. Returns number of devices found.
*/
void NET_scan_devices(NetDevice devices[], int* count);

void NET_device_info(NetDevice device, NET_DeviceInfo* info);

/*
    Reset/turn off network controller.
    Remove/reset interrupts.
    Free all internal memory.

    This will block if a packet is currently being sent or received by a NET function.
    Packets in internal buffers (internal to controller or NET module) will be lost.
    
    @note Thread-safe
*/
void NET_cleanup();

/*
    Polls the network controller for packets. The returned packets are raw ethernet frames.
    
    @note Thread-safe

    @param packet Packet with pointer and size. (Ethernet frame + IP/ICMP + UDP/TCP for example).
    @return False if no packet available. True if packet was available.
*/
bool NET_poll_packet(NetDevice device, NET_Packet* packet);
void NET_free_packet(NetDevice device, NET_Packet* packet);

/*
    Sends packet to network controller.
    
    @note Thread-safe

    @param buffer Pointer to packet data (Ethernet frame + IP/ICMP + UDP/TCP for example).
    @param size Size of packet data.
    @return False if no packet available. True if packet was available.
*/
void NET_send_packet(NetDevice device, void* buffer, int size);

/*
    Sets callback for network controller. When packets are received the callback will be
    called asynchronously. The callback is called asynhronously possible in an interrupt.
    You may want to do little computation in the callback and copy it to a separate buffer.

    Pass NULL to remove the callback. This will block if the callback is currently
    receiving a packet.

    @note Thread-safe, the call itself is thread safe. Be a little careful what you do in
    the callback. NET_send_packet() is fine. NET_poll_packet() is not!

    @param callback Will be called asynchronously when packet was received.
*/
void NET_set_receive_callback(NetDevice device, FN_NET_recv_packet callback, void* user_data);


//######################################
//     PROTOCOL HANDLING FUNCTIONS
//######################################


bool NET_handle_packet(NetDevice device, NET_Packet* packet);


//######################################
//     EXTRA WILL MOVE ELSEWHERE
//######################################


void NET_send_arp(NetDevice device, uint32_t address);

void NET_send_dhcp_discover(NetDevice device);
void NET_send_dhcp_request(NetDevice device, u32 request_address, u32 dhcp_server);

bool NET_send_udp(NetDevice device, u8 dst_mac[6], u32 address, u16 src_port, u16 dst_port, void* data, u32 size);
