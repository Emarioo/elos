
#include "elos/kernel/net/i8254x.h"

#include "elos/kernel/driver/pci.h"

#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/common/string.h"

#include "elos/common/string.h"

#include "elos/kernel/net/net_internal.h"

#include "elos/network.h"




typedef struct NetworkController {
    bool found;
    PCI_ConfigSpace config;
    u64 ioaddr;

    u8 mac_address[6];
} NetworkController;

static NetworkController controller;

#define VENDOR_ID__INTEL 0x8086

#define DEVICE_ID__82540EM_A 0x100E

#define printf(...) KCON_printf(__VA_ARGS__)

void setup_transmit_ring();
void setup_receive_ring();
void enable_interrupts();




bool find_device(PCI_Scanner* scanner, PCI_ConfigSpace* config) {
    if (config->classCode == PCI_CLASSCODE__NETWORK_CONTROLLER) {
        if (config->vendorID == VENDOR_ID__INTEL && config->deviceID == DEVICE_ID__82540EM_A) {
            controller.found = true;
            controller.config = *config;
            return true;
        } else {
            printf("NET_init: Searching PCI for network card, found device id 0x%x (vendor 0x%x), but card is not supported.\n", config->deviceID, config->vendorID);
        }
    }

    return false;
}

void write_register(uint16_t reg_offset, uint32_t value){
    // @TODO Memory needs to be marked non cacheable in the page tables?
    *(uint32_t *)(controller.ioaddr + reg_offset) = value;
}

uint32_t read_register(uint16_t reg_offset){
    // @TODO Memory needs to be marked non cacheable in the page tables?
    return *(uint32_t *)(controller.ioaddr + reg_offset);
}

void reset_nic();


u16 eeprom_read(u8 addr);

void card_init() {

    PCI_Scanner scanner = {};
    scanner.func = find_device;

    pci_scan_buses(&scanner);

    if (!controller.found) {
        printf("NET_init: Could not find a supported Network Controller on the PCI bus.\n");
        return;
    }

    if ((controller.config.header0.bar0 & 0x1) == 1) {
        printf("NET_init: Network Controller uses I/O space not memory mapped!?\n");
        return;
    }

    int prefetchable = controller.config.header0.bar0 & 0x8;

    // There text at https://wiki.osdev.org/PCI "Address and size of the BAR" which says:
    // "Before attempting to read the information about the BAR, make sure to disable both I/O and memory decode in the command byte"
    // I am not doiung this which might be a problem.

    // Disable memory + IO decode
    u16 cmd = pciConfig_readw(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x6);
    pciConfig_writew(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x6, cmd & ~0x3);

    int prev_bar = pciConfig_readl(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x10);
    pciConfig_writel(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x10, 0xFFFFFFFF);
    int bar_size = pciConfig_readl(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x10);
    bar_size = ~bar_size + 1; // decode to actual size
    pciConfig_writel(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x10, prev_bar);

    cmd = pciConfig_readw(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x6);
    pciConfig_writew(controller.config.pci_bus, controller.config.pci_device, controller.config.pci_function, 0x6, cmd | 0x3);


    printf("NET_init: Network Controller prefetchable: %d\n", prefetchable);
    printf("NET_init: Network Controller BAR0 size: %dK\n", bar_size/1024);

    switch ((controller.config.header0.bar0 & 0x6) >> 1) {
        case 0:
            // 32 bit
            controller.ioaddr = controller.config.header0.bar0 & 0xFFFFFFF0;
            break;
        case 2:
            // 64 bit
            controller.ioaddr = (u64)(controller.config.header0.bar0 & 0xFFFFFFF0) |
                           (u64)controller.config.header0.bar1 << 32;
            break;
        default:
            printf("NET_init: The heck? bar type\n");
            return;
    }

    // @TODO Do i need to memory map (page tables) the base address space?

    /*
        Reset the Network Controller
    */
    reset_nic();

    memcpy(current_mac, controller.mac_address, 6);

    printf("NET_init: MAC Address: %x%x:%x%x:%x%x:%x%x:%x%x:%x%x\n",
        controller.mac_address[0] >> 4, controller.mac_address[0] & 0xF,
        controller.mac_address[1] >> 4, controller.mac_address[1] & 0xF,
        controller.mac_address[2] >> 4, controller.mac_address[2] & 0xF,
        controller.mac_address[3] >> 4, controller.mac_address[3] & 0xF,
        controller.mac_address[4] >> 4, controller.mac_address[4] & 0xF,
        controller.mac_address[5] >> 4, controller.mac_address[5] & 0xF
        );

    /*
        Setup ring buffers
    */

    setup_transmit_ring();
    setup_receive_ring();

    enable_interrupts();

}

u16 eeprom_read(u8 addr) {
    /* Tell the EEPROM to start reading */
    // @TODO If device type is 82541x then we should do a 2-bit shift. See 8254x manual.
    u32 tmp = (((u32)addr & 0xff) << 8) | CARD_BIT_EERD_START;
    write_register(CARD_REG_EERD, tmp);

    u32 data;
    while (1) {
        data = read_register(CARD_REG_EERD);
        if (data & CARD_BIT_EERD_DONE)
            break;
        // @TODO Add retry limit?
        // @TODO Add some tiny sleep?
    }

    // Clear start bit
    write_register(CARD_REG_EERD, data & ~CARD_BIT_EERD_START);

    return data >> 16;
}


void reset_nic() {
    // Just interesting to see this value when debugging
    uint32_t status = read_register(CARD_REG_STATUS);
    
    // Reset controller
    uint32_t device_control = read_register(CARD_REG_CTRL);
    
    device_control |= CARD_BIT_CTRL_RST; // Set the reset bit
    write_register(CARD_REG_CTRL, device_control);
    
    while(read_register(CARD_REG_CTRL) & CARD_BIT_CTRL_RST) __asm__ ("hlt"); // wait for it to reset
    
    device_control = read_register(CARD_REG_CTRL);
    device_control |= CARD_BIT_CTRL_ASDE | CARD_BIT_CTRL_SLU; // Enable Auto Speed Detection.
    
    write_register(CARD_REG_CTRL, device_control);

    // Check if eeprom present bit is set. That's how we get the mac address.
    if ((read_register(CARD_REG_EECD) & CARD_BIT_EECD_EE_PRES) == 0) {
        printf("NET_init: EEPROM present bit is not set for i8254x\n");
        return;
    }

    // I misunderstood documentation?
    // No need to enable or lock anything just read eeprom?
    // Enable eeprom
    // u32 base_value = CARD_BIT_EECD_SK | CARD_BIT_EECD_CS | CARD_BIT_EECD_DI;
    // write_register(CARD_REG_EECD, base_value);

    // Lock eeprom
    // write_register(CARD_REG_EECD, base_value | CARD_BIT_EECD_EE_REQ);

    // while (1) {
    //     u32 val = read_register(CARD_REG_EECD);
    //     if (val & CARD_BIT_EECD_EE_GNT)
    //         break;
    //     // @TODO Put a retry limit?
    //     // @TODO Put some tiny sleep here?
    // }

    // Read the MAC address from the EEPROM
    u16 b0 = eeprom_read(0);
    u16 b1 = eeprom_read(1);
    u16 b2 = eeprom_read(2);
    
    controller.mac_address[0] = b0 & 0xFF;
    controller.mac_address[1] = b0 >> 8;
    controller.mac_address[2] = b1 & 0xFF;
    controller.mac_address[3] = b1 >> 8;
    controller.mac_address[4] = b2 & 0xFF;
    controller.mac_address[5] = b2 >> 8;

    // Unlock eeprom (clear EE_REQ bit)
    // write_register(CARD_REG_EECD, base_value);

    
    // Write the MAC address to RAL/RAH 0.
    uint32_t writeL = ((uint32_t)b1 << 16) | b0;
    uint32_t writeH = b2;
    
    write_register(CARD_REG_RAL(0), writeL);
    write_register(CARD_REG_RAH(0), writeH);

}

#define NUM_OF_TX_DESCRIPTORS 8
#define SIZE_OF_TX_DESCRIPTOR_BUFFER 4096
#define NUM_OF_RX_DESCRIPTORS 32
#define SIZE_OF_RX_DESCRIPTOR_BUFFER 4096

TransmitDescriptor* transmit_ring;
ReceiveDescriptor* receive_ring;

void setup_transmit_ring() {
    // @TODO memory should not be cached.
    int transmit_ring_size = NUM_OF_TX_DESCRIPTORS * 16;
    transmit_ring = PMEM_alloc(transmit_ring_size);
    memset(transmit_ring, 0, transmit_ring_size);
    
    for (int i = 0; i < NUM_OF_TX_DESCRIPTORS; i++){
        TransmitDescriptor* descriptor = transmit_ring + i;
        descriptor->buffer_address = PMEM_alloc(SIZE_OF_TX_DESCRIPTOR_BUFFER);
    }
    
    write_register(CARD_REG_TDBAL, ((u64)transmit_ring) & 0xFFFFFFFF);
    write_register(CARD_REG_TDBAH, ((u64)transmit_ring) >> 32);
    write_register(CARD_REG_TDLEN, transmit_ring_size);
    write_register(CARD_REG_TDH, 0);
    write_register(CARD_REG_TDT, 0);
    
    // Set the Enable (EN) and Pad Short Packets (PSP) bits
    u32 tctl = CARD_BIT_TCTL_EN | CARD_BIT_TCTL_PSP;
    write_register(CARD_REG_TCTL, tctl);
}


void setup_receive_ring() {
    // @TODO memory should not be cached. uncacheable pages
    int receive_ring_size = NUM_OF_RX_DESCRIPTORS * 16; // you can substitute 16 with sizeof(receive_descriptor_t)
    receive_ring = PMEM_alloc(receive_ring_size);
    
    for (int i = 0; i < NUM_OF_RX_DESCRIPTORS; i++){
        ReceiveDescriptor* descriptor = receive_ring + i;
        descriptor->buffer_address = PMEM_alloc(SIZE_OF_RX_DESCRIPTOR_BUFFER);
    }
    
    write_register(CARD_REG_RDBAL, ((u64)receive_ring) & 0xFFFFFFFF); // Base Address Low
    write_register(CARD_REG_RDBAH, ((u64)receive_ring) >> 32); // Base Address High
    write_register(CARD_REG_RDLEN, receive_ring_size); // Ring Size
    write_register(CARD_REG_RDH, 0); // Set it to the first descriptor
    write_register(CARD_REG_RDT, NUM_OF_RX_DESCRIPTORS - 1); // Set it to the last descriptor
    
    // Set the Enable, Long Packet Reception, Broadcast Accept Mode and Size Extenstion bits
    // Also set the buffer size. This configuration (BSIZE = 0b11 and BSEX = 1) means 4096 (4kB) buffers
    u32 rctl = CARD_BIT_RCTL_EN | CARD_BIT_RCTL_LPE | CARD_BIT_RCTL_BAM | CARD_BIT_RCTL_BSEX | CARD_VAL_RCTL_BSIZE(0b11);
    write_register(CARD_REG_RCTL, rctl); 
}


void enable_interrupts(){
    uint32_t ims = CARD_BIT_IMS_RXT0 | CARD_BIT_IMS_RXO | CARD_BIT_IMS_LSC;
    write_register(CARD_REG_IMS, ims);
}

void _handle_interrupt() {
    uint32_t cause = read_register(CARD_REG_ICR); // Cleared uppon read

    if (cause & CARD_BIT_ICR_RXT0) { // Packets received
        if (g_recv_packet_callback) {
            // Call the function responsible for receiving
            // packets and sending them to the network stack
            static int fake_device;
            NET_Packet packet;
            NET_poll_packet(&fake_device, &packet);
            NET_handle_packet(&fake_device, &packet);
            NET_free_packet(&fake_device, &packet);
        }
    }

    if (cause & CARD_BIT_ICR_LSC){ // link status change
        // Read the status register and check the LU bit to get the link status
        if (read_register(CARD_REG_STATUS) & CARD_BIT_STATUS_LU) {
            printf("Link change detected: Link up!\n");
        }else{
            printf("Link change detected: Link down!\n");
        }
    }
}

u8 rx_next = 0;


void receive_packet(void** out_buffer, int* out_size) {
    // printf("RECV packets!\n");

    // @TODO make this thread safe

    u32 idx = rx_next;

    void* buffer = NULL; // use this to store the buffer.
    size_t buffer_len = 0; 

    while (receive_ring[idx].status & CARD_BIT_RD_STATUS_DD) {
        // This descriptor has been filled
        
        bool eop = receive_ring[idx].status & CARD_BIT_RD_STATUS_EOP;
        u16 len = receive_ring[idx].length;
        void* data = receive_ring[idx].buffer_address;
        
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
    
        // Set status to 0 (To give ownership back to the controller)
        receive_ring[idx].status = 0; 

        idx = (idx + 1) % NUM_OF_RX_DESCRIPTORS;

        if (eop) {
            break;
        }
    }

    // Give the controller more free descriptors by updating RDT
    u32 tail = (idx == 0) ? NUM_OF_RX_DESCRIPTORS - 1 : idx - 1;
    write_register(CARD_REG_RDT, tail);

    rx_next = idx;

    *out_buffer = buffer;
    *out_size = buffer_len;
}

void send_data(void* data, u32 size, bool EOP){
    u32 tail = read_register(CARD_REG_TDT);
    TransmitDescriptor* tx = transmit_ring + tail; // Get the descriptor the tail is pointing at (next available descriptor)

    memcpy(tx->buffer_address, data, size); // Copy the data to the previously allocated buffer

    tx->length = size; // Set the length of the descriptor

    if (EOP) tx->cmd |= CARD_BIT_TD_CMD_EOP | CARD_BIT_TD_CMD_IFCS; // If its the last one, set EOP
    tail = (tail + 1) % NUM_OF_TX_DESCRIPTORS;
    write_register(CARD_REG_TDT, tail); // Increment and write the tail
}

int send_packet(void* data, int length){
    int sent = 0;
    // split the data into chunks and send them
    while (sent < length){
        int to_send = min(length - sent, SIZE_OF_TX_DESCRIPTOR_BUFFER);
        send_data((void*)((u64)data + sent), to_send, to_send == (length - sent));
        sent += to_send;
    }
    return sent;
}


