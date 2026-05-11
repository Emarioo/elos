#include "elos/kernel/net/rtl8169.h"

#include "elos/kernel/net/net_internal.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"
#include "elos/kernel_console.h"
#include "elos/physical_memory.h"
#include "elos/kernel/pmem/paging.h"


typedef struct Descriptor {
    u32 command;
    u32 vlan;
    u32 low_buf;
    u32 high_buf;
} Descriptor;



#define DESCRIPTOR_COMMAND_OWN (1 << 31)
#define DESCRIPTOR_COMMAND_EOR (1 << 30)
#define DESCRIPTOR_COMMAND_FS (1 << 29)
#define DESCRIPTOR_COMMAND_LS (1 << 28)


#define printf(...) KCON_printf(__VA_ARGS__)

static bool reset_nic();

bool rtl8169_init() {
    
    decode_bar(&controller.config, &controller.ioaddr, &controller.ioaddr_size, &controller.maddr, &controller.maddr_size);

    if(!controller.ioaddr) {
        printf("[ERROR] rtl8169 BARS does not have IO bar, might have memory bar: %x\n", controller.ioaddr, controller.maddr);
        return false;
    }

    // If we use maddr don't forget to map the physical pages to virtual.

    bool yes = reset_nic();
    if (!yes) {
        return false;
    }

    printf("Reset success?\n");

    return true;
}


#define rx_buffer_len 2048
#define rx_descriptors_len 128
#define tx_buffer_len 2048
#define tx_descriptors_len 128


Descriptor* rx_descriptors;
u8* rx_packet_buffer;

Descriptor* tx_descriptors;
u8* tx_packet_buffer;

bool prepare_buffers() {
    #define RAW_BUFFER_SIZE (2*256 + 2*8 + rx_buffer_len * rx_descriptors_len + tx_buffer_len * tx_descriptors_len)
    // static u8 _raw_buffer[2*256 + 2*8 + 2 * rx_buffer_len * rx_descriptors_len];

    u8* _raw_buffer = PMEM_alloc_phys(RAW_BUFFER_SIZE, PMEM_FLAG_IDENTITY_MAPPED|PMEM_FLAG_NOT_CACHED);
    if (!_raw_buffer) {
        printf("rtl8169: Could not allocate physical pages for buffers. %d KB\n", RAW_BUFFER_SIZE/1024);
        return false;
    }
    // PMEM_alloc currently identitiy maps memory, will it in the future?
    // we manually allcoate phys pages and map them because of this.

    // Descriptors should be 256-byte aligned.
    // Buffers should be 8-byte aligned.
    u64 next_address = ((u64)_raw_buffer + 255) & ~0xFF;
    rx_descriptors = (void*)next_address;
    next_address = ((u64)next_address + sizeof(Descriptor) * rx_descriptors_len + 255) & ~0xFF;
    tx_descriptors = (void*)next_address;
    next_address = ((u64)next_address + sizeof(Descriptor) * tx_descriptors_len + 255) & ~0xFF;
    rx_packet_buffer = (void*)next_address;
    next_address = ((u64)next_address + rx_buffer_len * rx_descriptors_len + 8) & ~7;
    tx_packet_buffer = (void*)next_address;
    next_address = ((u64)next_address + tx_buffer_len * tx_descriptors_len + 8) & ~7;

    // printf("Mapping out addresses, rawaddr=%x\n", _raw_buffer);

    memset(rx_descriptors, 0, sizeof(Descriptor) * rx_descriptors_len);
    memset(tx_descriptors, 0, sizeof(Descriptor) * tx_descriptors_len);

    // printf("Prepared buffers\n");

    return true;
}

static bool reset_nic() {

    u64 ioaddr = controller.ioaddr;

    outb(ioaddr + 0x37, 0x10); /*set the Reset bit (0x10) to the Command Register (0x37)*/
    
    // Prepare buffers while card is resetting
    bool res = prepare_buffers();
    if (!res) {
        return false;
    }

    while(inb(ioaddr + 0x37) & 0x10) ;
    /*setting a timeout could be useful if the card is problematic*/

    
    for (int i=0;i<6;i++) {
        controller.mac_address[i] = inb(ioaddr + i);
    }
    memcpy(current_mac, controller.mac_address, 6);


    printf("NET_init: MAC Address: %x%x:%x%x:%x%x:%x%x:%x%x:%x%x\n",
        controller.mac_address[0] >> 4, controller.mac_address[0] & 0xF,
        controller.mac_address[1] >> 4, controller.mac_address[1] & 0xF,
        controller.mac_address[2] >> 4, controller.mac_address[2] & 0xF,
        controller.mac_address[3] >> 4, controller.mac_address[3] & 0xF,
        controller.mac_address[4] >> 4, controller.mac_address[4] & 0xF,
        controller.mac_address[5] >> 4, controller.mac_address[5] & 0xF
        );


    // Prepare RX descriptors and buffers
    for (int i=0;i<rx_descriptors_len;i++) {
        u64 packet_buffer = (u64)rx_packet_buffer + i * rx_buffer_len;
        rx_descriptors[i].low_buf = packet_buffer & 0xFFFFFFFF;
        rx_descriptors[i].high_buf = packet_buffer >> 32;
        rx_descriptors[i].vlan = 0;
        // The bitwise and for buffer length is problematic if rx_buffer_len is 0x2000 (8 KB)
        rx_descriptors[i].command = DESCRIPTOR_COMMAND_OWN | ((rx_buffer_len-1) & 0x1FF8);
        if (i == rx_descriptors_len-1) {
            rx_descriptors[i].command |= DESCRIPTOR_COMMAND_EOR;
        }
    }

    // Fill in descriptors as we transmit. We set EOR at the very least.
    tx_descriptors[tx_descriptors_len-1].command = DESCRIPTOR_COMMAND_EOR;

    // We create tx descriptors when we transmit packets.

    outb(ioaddr + 0x50, 0xC0); /* Unlock config registers */
    outl(ioaddr + 0x44, 0x0000E70F); /* RxConfig = RXFTH: unlimited, MXDMA: unlimited, AAP: set (promisc. mode set) */
    outb(ioaddr + 0x37, 0x04); /* Enable Tx in the Command register, required before setting TxConfig, NOTE: osdev did this so we do as well just in case. It works if we enable first. May work if we don't on my laptop? */
    outl(ioaddr + 0x40, 0x03000700); /* TxConfig = IFG: normal, MXDMA: unlimited */
    outw(ioaddr + 0xDA, rx_buffer_len-1); /* Max rx packet size */
    #define TX_UNIT_SIZE 32
    outb(ioaddr + 0xEC, (tx_buffer_len + TX_UNIT_SIZE-1)/TX_UNIT_SIZE); /* max tx packet size */


    outl(ioaddr + 0x20, (u64)tx_descriptors & 0xFFFFFFFF); // Tell the NIC where the first Tx descriptor is.
    outl(ioaddr + 0x24, (u64)tx_descriptors >> 32);
    outl(ioaddr + 0xE4, (u64)rx_descriptors & 0xFFFFFFFF); // Tell the NIC where the first Rx descriptor is.
    outl(ioaddr + 0xE8, (u64)rx_descriptors >> 32);

    outb(ioaddr + 0x37, 0x0c); /* Enable Rx/Tx in the Command register */
    outb(ioaddr + 0x50, 0);    /* Lock config registers */

    return true;
}

int next_rx_descriptor = 0;

void rtl8169_receive_packet(void** out_buffer, int* out_size) {
    
    if (rx_descriptors[next_rx_descriptor].command & DESCRIPTOR_COMMAND_OWN) {
        // No packets to read
        *out_buffer = NULL;
        *out_size = 0;
        return;
    }


    // Some handling of corrupt RX descriptor state when first descriptor isn't FS.
    // Shouldn't happen so no need to handle it?
    // while (1) {
    //     u32 cmd = rx_descriptors[next_rx_descriptor].command;
    //     if ((cmd & DESCRIPTOR_COMMAND_FS) == 0) {
    //         // We expect descriptor FS to be set to indicate that this descriptor
    //         // is first segment of descriptor.
    //         if ((cmd & DESCRIPTOR_COMMAND_OWN) == 0) {
    //             cmd |= DESCRIPTOR_COMMAND_OWN;
    //             rx_descriptors[next_rx_descriptor].command = cmd;
    //         }
    //         next_rx_descriptor = (next_rx_descriptor + 1) % rx_descriptors_len;
    //     } else {
    //         break;
    //     }
    // }

    int index = next_rx_descriptor;
    void* buffer = NULL; // use this to store the buffer.
    size_t buffer_len = 0; 

    while (1) {
        // This descriptor has been filled
        
        bool last_segment = rx_descriptors[index].command & DESCRIPTOR_COMMAND_LS;
        u16 len = rx_descriptors[index].command & 0x3FFF;
        void* data = (void*)((u64)rx_descriptors[index].low_buf | ((u64)rx_descriptors[index].high_buf << 32));
        
        printf("GOT SOMETHING! cmd=%x len=%d ptr=%x\n", rx_descriptors[index].command, len, data);


        // Handle multiple-descriptor packets
        if (buffer == NULL){ // This is the first descriptor of the packet
            buffer = PMEM_alloc(len); // use your kernel's heap allocator
            buffer_len = len;
            memcpy(buffer, data, len);
        }else{
            // Its the next part of the packet, add it to the packet
            void* new_buffer = PMEM_alloc(buffer_len + len); // allocate a bigger buffer
            memcpy(new_buffer, buffer, buffer_len); // copy the previous data
            PMEM_free(buffer); // free the old buffer
    
            // copy the new data
            memcpy((void*)((uint64_t)new_buffer + buffer_len), data, len);
            
            // Set the new buffer into the variables
            buffer_len += len;
            buffer = new_buffer;
        }
    
        // Set OWN bit (To give ownership back to the controller)
        rx_descriptors[index].command |= DESCRIPTOR_COMMAND_OWN;

        index = (index + 1) % rx_descriptors_len;

        if (last_segment) {
            break;
        }
    }
    next_rx_descriptor = index;
    
    *out_buffer = buffer;
    *out_size = buffer_len;
}

int next_tx_descriptor = 0;


int rtl8169_send_packet(void* data, int size) {
    u64 ioaddr = controller.ioaddr;

    if (size > tx_buffer_len)
        return 0;

    while (tx_descriptors[next_tx_descriptor].command & DESCRIPTOR_COMMAND_OWN) pause();

    void* packet_buffer = tx_packet_buffer + next_tx_descriptor * tx_buffer_len;
    memcpy(packet_buffer, data, size);

    tx_descriptors[next_tx_descriptor].low_buf = (u64)packet_buffer & 0xFFFFFFFF;
    tx_descriptors[next_tx_descriptor].high_buf = (u64)packet_buffer >> 32;
    tx_descriptors[next_tx_descriptor].vlan = 0;

    u32 command = DESCRIPTOR_COMMAND_OWN | DESCRIPTOR_COMMAND_FS | DESCRIPTOR_COMMAND_LS | (size & 0x3FFF);
    if (next_tx_descriptor == tx_descriptors_len-1) {
        command |= DESCRIPTOR_COMMAND_EOR;
    }
    tx_descriptors[next_tx_descriptor].command = command;

    outb(ioaddr + 0x38, 0x40); // Normal priority poll

    printf("rtl: sent %d\n", size);

    next_tx_descriptor = (next_tx_descriptor + 1) % tx_descriptors_len;
    return size;
}

