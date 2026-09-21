#include "elos/kernel/net/rtl8169.h"

#include "elos/network.h"
#include "elos/kernel/net/net_internal.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"
#include "elos/kernel_console.h"
#include "elos/physical_memory.h"
#include "elos/kernel/pmem/paging.h"
#include "elos/cpu.h"


static uint8_t* g_memory_bar;
static uint32_t g_io_bar;
static uint8_t  g_mac[6];


static u8 read_byte(u32 reg) {
    if (g_memory_bar) {
        return *(uint8_t*)(g_memory_bar + reg);
    } else {
        return inb(g_io_bar + reg);
    }
}
static u16 read_short(u32 reg) {
    if (g_memory_bar) {
        return *(uint16_t*)(g_memory_bar + reg);
    } else {
        return inw(g_io_bar + reg);
    }
}
static u32 read_long(u32 reg) {
    if (g_memory_bar) {
        return *(uint32_t*)(g_memory_bar + reg);
    } else {
        return inl(g_io_bar + reg);
    }
}

static void write_byte(u32 reg, u8 value) {
    if (g_memory_bar) {
        *(uint8_t*)(g_memory_bar + reg) = value;
    } else {
        outb(g_io_bar + reg, value);
    }
}
static void write_short(u32 reg, u16 value) {
    if (g_memory_bar) {
        *(uint16_t*)(g_memory_bar + reg) = value;
    } else {
        outw(g_io_bar + reg, value);
    }
}
static void write_long(u32 reg, u32 value) {
    if (g_memory_bar) {
        *(uint32_t*)(g_memory_bar + reg) = value;
    } else {
        outl(g_io_bar + reg, value);
    }
}


static NET_Device* g_device;

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


void rtl8169_interrupt_handler(u32 vector, InterruptFrame* frame);
void rtl8169_setup_interrupt(PCI_ConfigSpace* config);

bool rtl8169_init(NET_Device* device) {

    PCI_ConfigSpace* config = &device->config;

    // printf("bar0 0x%x\n", config->header0.bar0);
    // printf("bar1 0x%x\n", config->header0.bar1);
    // printf("bar2 0x%x\n", config->header0.bar2);
    // printf("bar3 0x%x\n", config->header0.bar3);

    if (config->header0.bar0 & 1) {
        g_io_bar = (config->header0.bar0 & ~0x3);
    } else {
        // no IO bar is wierd, quit before funny things happen.
        printf("rtl8169: BARS does not have IO bar, might have memory bar\n");
        return false;
    }
    
    if ((config->header0.bar1 & 1) == 0) {
        if (config->header0.bar1 != 0) {
            // 32-bit
            g_memory_bar = (void*)(size_t)(config->header0.bar1 & ~0x3);
        } else if ((config->header0.bar2 & 7) == 4) {
            // 64-bit
            g_memory_bar = (void*)(((u64)config->header0.bar2 & ~0xfLLU) | ((u64)config->header0.bar3 << 32));
        }
    }
    // no memory bar is fine i guess? fall back to io bar?
    // printf("iobar  0x%x\n", g_io_bar);
    // printf("membar 0x%zx\n", g_memory_bar);

    if (g_memory_bar) {
        bool mapped = PMEM_map_memory(g_kernelPageTable, g_memory_bar, g_memory_bar, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);
        if (!mapped) {
            // fall back to iobar
            printf("rtl8169: could not page map membar. using iobar instead\n");
            g_memory_bar = NULL;
        }
    }

    
    // decode_bar(&controller.config, &controller.ioaddr, &controller.ioaddr_size, &controller.maddr, &controller.maddr_size);

    // if(!controller.ioaddr) {
    //     return false;
    // }

    // If we use maddr don't forget to map the physical pages to virtual.

    bool yes = reset_nic();
    if (!yes) {
        printf("rtl8169: could not reset/init\n");
        return false;
    }
    
    printf("rtl8169: MAC Address: %x%x:%x%x:%x%x:%x%x:%x%x:%x%x\n",
        g_mac[0] >> 4, g_mac[0] & 0xF,
        g_mac[1] >> 4, g_mac[1] & 0xF,
        g_mac[2] >> 4, g_mac[2] & 0xF,
        g_mac[3] >> 4, g_mac[3] & 0xF,
        g_mac[4] >> 4, g_mac[4] & 0xF,
        g_mac[5] >> 4, g_mac[5] & 0xF
        );


    memcpy(device->info.mac, g_mac, sizeof(device->info.mac));

    if (initialize_network_with_interrupts) {
        rtl8169_setup_interrupt(config);
    }

    // printf("Reset success?\n");

    return true;
}

// @IMPORTANT Buffer sizes can't be increased without reading the specification.
//   I believe max size is 0x1FF8 2^13-8
#define rx_buffer_len 2048
#define rx_descriptors_len 128
#define tx_buffer_len 2048
#define tx_descriptors_len 128


// Bit 0,1,2,13 should be zero (since max size is 2**13-8 = 8184)
#define RX_BUFFER_SIZE_MASK 0x1ff8
#define TX_BUFFER_SIZE_MASK 0x3FFF

volatile Descriptor* rx_descriptors;
u8* rx_packet_buffer;

volatile Descriptor* tx_descriptors;
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

    memset((void*)rx_descriptors, 0, sizeof(Descriptor) * rx_descriptors_len);
    memset((void*)tx_descriptors, 0, sizeof(Descriptor) * tx_descriptors_len);

    // printf("Prepared buffers\n");

    return true;
}

static bool reset_nic() {


    write_byte(0x37, 0x10); /*set the Reset bit (0x10) to the Command Register (0x37)*/
    
    // Prepare buffers while card is resetting
    bool res = prepare_buffers();
    if (!res) {
        return false;
    }

    while(read_byte(0x37) & 0x10) ;
    /*setting a timeout could be useful if the card is problematic*/

    
    for (int i=0;i<6;i++) {
        g_mac[i] = read_byte(i);
    }
    // memcpy(current_mac, controller.mac_address, 6);


    // printf("NET_init: MAC Address: %x%x:%x%x:%x%x:%x%x:%x%x:%x%x\n",
    //     controller.mac_address[0] >> 4, controller.mac_address[0] & 0xF,
    //     controller.mac_address[1] >> 4, controller.mac_address[1] & 0xF,
    //     controller.mac_address[2] >> 4, controller.mac_address[2] & 0xF,
    //     controller.mac_address[3] >> 4, controller.mac_address[3] & 0xF,
    //     controller.mac_address[4] >> 4, controller.mac_address[4] & 0xF,
    //     controller.mac_address[5] >> 4, controller.mac_address[5] & 0xF
    //     );


    // Prepare RX descriptors and buffers
    for (int i=0;i<rx_descriptors_len;i++) {
        u64 packet_buffer = (u64)rx_packet_buffer + i * rx_buffer_len;
        rx_descriptors[i].low_buf = packet_buffer & 0xFFFFFFFF;
        rx_descriptors[i].high_buf = packet_buffer >> 32;
        rx_descriptors[i].vlan = 0;
        // The bitwise and for buffer length is problematic if rx_buffer_len is 0x2000 (8 KB)
        u32 cmd = DESCRIPTOR_COMMAND_OWN | ((rx_buffer_len-1) & RX_BUFFER_SIZE_MASK);
        if (i == rx_descriptors_len-1) {
            cmd |= DESCRIPTOR_COMMAND_EOR;
        }
        rx_descriptors[i].command = cmd;
    }

    // Fill in descriptors as we transmit. We set EOR at the very least.
    tx_descriptors[tx_descriptors_len-1].command = DESCRIPTOR_COMMAND_EOR;

    // We create tx descriptors when we transmit packets.

    write_byte(0x50, 0xC0); /* Unlock config registers */
    write_long(0x44, 0x0000E70F); /* RxConfig = RXFTH: unlimited, MXDMA: unlimited, AAP: set (promisc. mode set) */
    write_byte(0x37, 0x04); /* Enable Tx in the Command register, required before setting TxConfig, NOTE: osdev did this so we do as well just in case. It works if we enable first. May work if we don't on my laptop? */
    write_long(0x40, 0x03000700); /* TxConfig = IFG: normal, MXDMA: unlimited */
    write_short(0xDA, rx_buffer_len-1); /* Max rx packet size */
    #define TX_UNIT_SIZE 32
    write_byte(0xEC, (tx_buffer_len + TX_UNIT_SIZE-1)/TX_UNIT_SIZE); /* max tx packet size */


    write_long(0x20, (u64)tx_descriptors & 0xFFFFFFFF); // Tell the NIC where the first Tx descriptor is.
    write_long(0x24, (u64)tx_descriptors >> 32);
    write_long(0xE4, (u64)rx_descriptors & 0xFFFFFFFF); // Tell the NIC where the first Rx descriptor is.
    write_long(0xE8, (u64)rx_descriptors >> 32);

    write_byte(0x37, 0x0c); /* Enable Rx/Tx in the Command register */
    write_byte(0x50, 0);    /* Lock config registers */

    return true;
}

int next_rx_descriptor = 0;

void rtl8169_receive_packet(void** out_buffer, u32* out_size) {
    
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
        
        u32 rx_cmd = rx_descriptors[index].command;
        bool last_segment = rx_cmd & DESCRIPTOR_COMMAND_LS;
        u16 len = rx_cmd & RX_BUFFER_SIZE_MASK;
        void* data = (void*)((u64)rx_descriptors[index].low_buf | ((u64)rx_descriptors[index].high_buf << 32));
        
        // printf("GOT SOMETHING! cmd=%x len=%d ptr=%x\n", rx_descriptors[index].command, len, data);

        
        // Good for debugging. Fragmented packets have not been tested.
        // if (!(rx_cmd & DESCRIPTOR_COMMAND_FS)) {
        //     printf("RX descriptor without FS\n");
        // }
        // if (!(rx_cmd & DESCRIPTOR_COMMAND_LS)) {
        //     printf("Packet spans descriptors\n");
        // }


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
        u32 cmd = DESCRIPTOR_COMMAND_OWN | ((rx_buffer_len-1) & RX_BUFFER_SIZE_MASK);
        if (index == rx_descriptors_len-1) {
            cmd |= DESCRIPTOR_COMMAND_EOR;
        }
        rx_descriptors[index].command = cmd;

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


u32 rtl8169_send_packet(const void* data, u32 size) {

    if (size > tx_buffer_len)
        return 0;

    while (tx_descriptors[next_tx_descriptor].command & DESCRIPTOR_COMMAND_OWN) pause();

    void* packet_buffer = tx_packet_buffer + next_tx_descriptor * tx_buffer_len;
    memcpy(packet_buffer, data, size);

    tx_descriptors[next_tx_descriptor].low_buf = (u64)packet_buffer & 0xFFFFFFFF;
    tx_descriptors[next_tx_descriptor].high_buf = (u64)packet_buffer >> 32;
    tx_descriptors[next_tx_descriptor].vlan = 0;

    u32 command = DESCRIPTOR_COMMAND_OWN | DESCRIPTOR_COMMAND_FS | DESCRIPTOR_COMMAND_LS | (size & TX_BUFFER_SIZE_MASK);
    if (next_tx_descriptor == tx_descriptors_len-1) {
        command |= DESCRIPTOR_COMMAND_EOR;
    }
    tx_descriptors[next_tx_descriptor].command = command;

    write_byte(0x38, 0x40); // Normal priority poll

    // printf("rtl: sent %d\n", size);

    next_tx_descriptor = (next_tx_descriptor + 1) % tx_descriptors_len;
    return size;
}

void rtl8169_setup_interrupt(PCI_ConfigSpace* config) {
    
    u32 coreIndex = CPU_get_core_index();
    u32 localIRQ = 6; // @TODO Don't hardcode. all network controllers use same IRQ which is terrible. will break stuff.

    u32 cap_ptr = config->header0.capabilities_pointer & ~0x3;

    while (cap_ptr) {
        u32 cap_data = pci_config_readl(config, cap_ptr);
        u32 cap_id   = (cap_data & 0xFF);
        u32 cap_next = (cap_data >> 8) & 0xFC;

        // printf("Cap %d\n", cap_id);

        if (cap_id == 0x5) {
            u16 messageControl = (cap_data >> 16);

            int is_64bit = messageControl & (1 << 7);

            u64 messageAddress;
            u16 messageData;
            CPU_set_msi_irq(coreIndex, localIRQ, rtl8169_interrupt_handler, &messageAddress, &messageData);

            pci_config_writel(config, cap_ptr + 0x4, messageAddress & 0xFFFFFFFF);
            if (is_64bit) {
                pci_config_writel(config, cap_ptr + 0x8, messageAddress >> 32);
                pci_config_writew(config, cap_ptr + 0xC, messageData);
            } else {
                pci_config_writew(config, cap_ptr + 0x8, messageData);
            }

            // Enable MSI
            messageControl |= 1;
            pci_config_writew(config, cap_ptr + 0x2, messageControl);

            
            cap_data = pci_config_readl(config, cap_ptr);
            cap_id   = (cap_data & 0xFF);
            cap_next = (cap_data >> 8) & 0xFC;
            messageControl = (cap_data >> 16);

            // printf(" control 0x%x\n", messageControl);
            // printf(" address %p\n",   messageAddress);
            // printf(" data    0x%x\n", messageData);

        } else if (cap_id == 0x11) {
            // HDA on QEMU does not support MSI-X
            // @TODO Check if my laptop HDA supports MSI or only MSI-X, probably does right?
        }

        cap_ptr = cap_next;
    }

    // receive fifo overflow
    #define CARD_BIT_FOVW    (1 << 7)
    // link change
    #define CARD_BIT_LINKCHG (1 << 5)
    // receive ok
    #define CARD_BIT_ROK     (1 << 0)

    uint16_t ims = CARD_BIT_LINKCHG | CARD_BIT_ROK | CARD_BIT_FOVW;
    write_short(0x3C, ims); // interrupt mask
}

void rtl8169_interrupt_handler(u32 vector, InterruptFrame* frame) {
    printf("rtl8169: received interrupt!\n");

    // @TODO THIS IS SLOW. Implement high kernel mapping.
    u64 prev_cr3 = read_cr3();
    write_cr3((size_t)g_kernelPageTable);
    

    // @TODO Check which network controller triggered.


    // u32 cause = read_register(CARD_REG_ICR);
    // @TODO Don't assume interrupt is because we got a packet.
    //    It's fine because we can still check if we have packet and do nothing if we don't.
    // @TODO How to handle overrun.
    // printf("i8254x interrupt 0x%x\n", cause);

    void* buffer = NULL;
    u32 bufferSize = 0;
    rtl8169_receive_packet(&buffer, &bufferSize);

    if (!buffer || !bufferSize) {
        goto exit;
    }

    uint8_t* bytes = buffer;

    // printf("RECV ");
    // printf("%02x%02x%02x%02x ", bytes[0], bytes[1], bytes[2], bytes[3]);
    // bytes += 4;
    // printf("%02x%02x%02x%02x ", bytes[0], bytes[1], bytes[2], bytes[3]);
    // bytes += 4;
    // printf("\n");
    // printf("%02x%02x%02x%02x ", bytes[0], bytes[1], bytes[2], bytes[3]);
    // bytes += 4;
    // printf("%02x%02x%02x%02x ", bytes[0], bytes[1], bytes[2], bytes[3]);
    // bytes += 4;
    // printf("\n");

    NET_Packet packet = {
        .buffer = buffer,
        .size   = bufferSize,
    };

    // @TODO How do we know which device?
    //   IRQ number per device?
    NET_Device* device = &g_devices[0];

    bool handled = net_handle_packet(device, &packet);
    if (!handled) {
        NET_free_packet(&packet);
    }

exit:
    write_cr3(prev_cr3);
}
