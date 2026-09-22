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


    @TODO How does user app iterate network controllers.
       A network debug app might want to display all controllers, networks, gateways, ipv4 addresses.
       Maybe artificially make network controller like tap on linux.
*/


#pragma once

#include "elos/common/types.h"
#include "elos/elos.h"

#include "elos/kernel/driver/pci.h"


//###############################
//          TYPES
//###############################


typedef struct NET_Packet {
    char* buffer;
    int   size;
} NET_Packet;

typedef struct NET_PortMessage NET_PortMessage;

struct NET_PortMessage {
    NET_Packet       rawPacket;
    NET_PortMessage* nextMessage;
    
    u32 sourceAddress;
    u32 destinationAddress;
    u16 sourcePort;
    u16 destinationPort;
    u8* data;
    u8  data_len;
};


typedef struct NET_DeviceInfo {
    char name[64];
    u8   mac[6];
    u32  ipv4_address;
    u32  ipv4_subnet_mask;
    u32  ipv4_gateway;
} NET_DeviceInfo;

typedef struct NET_Device NET_Device;

struct NET_Device {
    bool present;
    NET_DeviceInfo info;

    // Some network controllers may use USB and not PCI.
    PCI_ConfigSpace config;
};

typedef struct {
    bool             used;
    ELOS_Net_Address address;
    NET_PortMessage* volatile nextMessage;
} NET_Handle;

// typedef bool(*FN_NET_recv_packet)(NET_Device* device, NET_Packet* packet, void* user_data);


// extern FN_NET_recv_packet g_recv_packet_callback;
// extern void* g_recv_packet_callback_userData;

extern bool initialize_network_with_interrupts;
extern bool enable_network_interrupts;
extern bool skip_network_dhcp;


//######################################
//      KERNEL NETWORK FUNCTIONS
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
void NET_scan_devices(NET_Device* devices[], int* count);

void NET_device_info(NET_Device* device, NET_DeviceInfo* info);

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
    Sends raw packet to network controller.
    
    @note Thread-safe

    @param buffer Pointer to packet data (Ethernet frame + IP/ICMP + UDP/TCP for example).
    @param size Size of packet data.
    @return False if no packet available. True if packet was available.
*/
bool NET_send_packet(NET_Device* device, const void* buffer, int size);

// void NET_enable_interrupt(NET_Device* device);
// void NET_disable_interrupt(NET_Device* device);

/*
    Polls the network controller for packets. The returned packets are raw ethernet frames.
    
    @note Thread-safe

    @param packet Packet with pointer and size. (Ethernet frame + IP/ICMP + UDP/TCP for example).
    @return False if no packet available. True if packet was available.
*/
bool NET_poll_packet(NET_Device* device, NET_Packet* packet);
void NET_free_packet(NET_Packet* packet);

// bool NET_handle_packet(NET_Device* device, NET_Packet* packet);

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
// void NET_set_receive_callback(NET_Device* device, FN_NET_recv_packet callback, void* user_data);



//######################################
//      USER NETWORK ASYNC API
//######################################




/*
    Opens a connection on the this computer at the address.
*/
NET_Handle* NET_open(const ELOS_Net_Address* address);

/*
    Closes a connection.
*/
void NET_close(NET_Handle* handle);

/*
    Sends bytes from a connection on this computer to another
    address/connection which may or may not be this computer.
*/
ELOS_Error NET_write(NET_Handle* handle, const ELOS_Net_Address* address, const void* data, u32 size);

/*
    Reads bytes from a connection. The sender address is provided.

    @param address Which address/connection the bytes came from.
*/
ELOS_Error NET_read(NET_Handle* handle, ELOS_Net_Address* address, void* buffer, u32* bufferSize, u64 timeout_ns);




//######################################
//     EXTRA WILL MOVE ELSEWHERE
//######################################


