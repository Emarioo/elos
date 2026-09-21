

#include "elos/kernel/net/i82574.h"

#include "elos/kernel/driver/pci.h"


#include "elos/kernel_console.h"

#include "elos/physical_memory.h"

#include "elos/common/string.h"

#include "elos/common/intrinsics.h"

#include "elos/kernel/net/net_internal.h"

#include "elos/kernel/pmem/paging.h"

#include "elos/cpu.h"


#define printf(...) KCON_printf(__VA_ARGS__)
#define debug(...)  KCON_printf(__VA_ARGS__)

void i82574_interrupt_handler(u32 vector, InterruptFrame* frame);

bool i82574_reset(PCI_ConfigSpace* config);

static void setup_transmit_ring();
static void setup_receive_ring();

u16 i82574_eeprom_read(u8 addr);
u16 i82574_flash_read(u8 addr);

static uint8_t* g_memory_bar;
static uint8_t* g_flash_bar;
static uint8_t  g_mac[6];

static NET_Device* g_device;


static void write_register(uint16_t reg_offset, uint32_t value){
    // @TODO Memory needs to be marked non cacheable in the page tables?
    // @TODO Also need to map them? EFI might have done it but bad to assume so.
    *(uint32_t *)(g_memory_bar + reg_offset) = value;
}

static uint32_t read_register(uint16_t reg_offset){
    // @TODO Memory needs to be marked non cacheable in the page tables?
    return *(uint32_t *)(g_memory_bar + reg_offset);
}


void setup_interrupt(PCI_ConfigSpace* config) {

    u32 coreIndex = CPU_get_core_index();
    u32 localIRQ = 6; // @TODO Don't hardcode.

    u32 cap_ptr = config->header0.capabilities_pointer & ~0x3;

    while (cap_ptr) {
        u32 cap_data = pci_config_readl(config, cap_ptr);
        u32 cap_id   = (cap_data & 0xFF);
        u32 cap_next = (cap_data >> 8) & 0xFC;

        // debug("Cap %d\n", cap_id);

        if (cap_id == 0x5) {
            u16 messageControl = (cap_data >> 16);

            int is_64bit = messageControl & (1 << 7);

            u64 messageAddress;
            u16 messageData;
            CPU_set_msi_irq(coreIndex, localIRQ, i82574_interrupt_handler, &messageAddress, &messageData);

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

            // debug(" control 0x%x\n", messageControl);
            // debug(" address %p\n",   messageAddress);
            // debug(" data    0x%x\n", messageData);

        } else if (cap_id == 0x11) {
            // HDA on QEMU does not support MSI-X
            // @TODO Check if my laptop HDA supports MSI or only MSI-X, probably does right?
        }

        cap_ptr = cap_next;
    }


    uint32_t ims = CARD_BIT_IMS_RXT0 | CARD_BIT_IMS_RXO | CARD_BIT_IMS_LSC;
    write_register(CARD_REG_IMS, ims);

}

bool i82574_init(NET_Device* device) {
    static bool initialized;

    if (initialized) {
        // @TODO Handle multiple network controllers.
        printf("i82574_init: Missing support for multiple network controllers\n");
        return false;
    }
    initialized = true;
    g_device = device;
    PCI_ConfigSpace* config = &device->config;

    // @TODO Investigate power management. Network controller can draw a lot of energy while idle?

    u64 barSize = 0;
    decode_bar_size(config, 0, &barSize);

    void* memory_bar = (void*)((size_t)config->header0.bar0 & ~(size_t)0xF);
    g_memory_bar = memory_bar;
    
    void* flash_bar = (void*)((size_t)config->header0.bar1 & ~(size_t)0xF);
    g_flash_bar = flash_bar;

    bool mapped = PMEM_map_memory(g_kernelPageTable, memory_bar, memory_bar, barSize, PMEM_FLAG_NOT_CACHED);
    KERNEL_PANIC(mapped, "Could not map memory for i82574 (network)");

    mapped = PMEM_map_memory(g_kernelPageTable, flash_bar, flash_bar, 0x40, PMEM_FLAG_NOT_CACHED);
    KERNEL_PANIC(mapped, "Could not map memory for i82574 (network)");

    /*
        Reset the Network Controller
    */
    bool res = i82574_reset(config);
    if (!res) {
        printf("Reset failed\n");
        return false;
    }

    memcpy(device->info.mac, g_mac, sizeof(device->info.mac));


    printf("i82574: MAC Address: %x%x:%x%x:%x%x:%x%x:%x%x:%x%x\n",
        g_mac[0] >> 4, g_mac[0] & 0xF,
        g_mac[1] >> 4, g_mac[1] & 0xF,
        g_mac[2] >> 4, g_mac[2] & 0xF,
        g_mac[3] >> 4, g_mac[3] & 0xF,
        g_mac[4] >> 4, g_mac[4] & 0xF,
        g_mac[5] >> 4, g_mac[5] & 0xF
        );

    if (initialize_network_with_interrupts) {
        setup_interrupt(config);
    }

    setup_transmit_ring();
    setup_receive_ring();

    return true;
}



bool i82574_reset(PCI_ConfigSpace* config) {

    i82574_Registers* regs = (void*)g_memory_bar;
    
    // printf("CTRL      0x%x\n", regs->CTRL);
    // printf("STATUS    0x%x\n", regs->STATUS);
    // printf("EEC       0x%x\n", regs->EEC);
    // printf("EERD      0x%x\n", regs->EERD);
    // printf("CTRL_EXT  0x%x\n", regs->CTRL_EXT);
    // printf("FLA       0x%x\n", regs->FLA);
    // printf("ICR       0x%x\n", regs->ICR);


    uint32_t status = read_register(CARD_REG_STATUS);

    uint32_t control = read_register(CARD_REG_CTRL);
    
    control |= CARD_BIT_CTRL_RST;

    write_register(CARD_REG_CTRL, control);
    
    while(read_register(CARD_REG_CTRL) & CARD_BIT_CTRL_RST) pause(); // wait for it to reset
    
    control = read_register(CARD_REG_CTRL);
    control |= CARD_BIT_CTRL_ASDE | CARD_BIT_CTRL_SLU; // Enable Auto Speed Detection.
    
    write_register(CARD_REG_CTRL, control);
    
    // Check if eeprom present bit is set. That's how we get the mac address.
    if ((read_register(CARD_REG_EECD) & CARD_BIT_EECD_EE_PRES) == 0) {
        printf("%s: Why is NVM not present?\n", __func__);
        return false;
    }

    u32 eec = read_register(CARD_REG_EECD);

    u16 b0;
    u16 b1;
    u16 b2;
    
    if (CARD_IS_NVM_FLASH(eec)) {
        // printf("Flash\n");
        // Read the MAC address from the EEPROM
        b0 = i82574_flash_read(0);
        b1 = i82574_flash_read(1);
        b2 = i82574_flash_read(2);
    } else {
        // printf("EEPROM\n");
        // Read the MAC address from the EEPROM
        b0 = i82574_eeprom_read(0);
        b1 = i82574_eeprom_read(1);
        b2 = i82574_eeprom_read(2);
    }

    g_mac[0] = b0 & 0xFF;
    g_mac[1] = b0 >> 8;
    g_mac[2] = b1 & 0xFF;
    g_mac[3] = b1 >> 8;
    g_mac[4] = b2 & 0xFF;
    g_mac[5] = b2 >> 8;
    
    // Write the MAC address to RAL/RAH 0.
    uint32_t writeL = ((uint32_t)b1 << 16) | b0;
    uint32_t writeH = b2;
    
    write_register(CARD_REG_RAL(0), writeL);
    write_register(CARD_REG_RAH(0), writeH);

    return true;
}


u16 i82574_flash_read(u8 addr) {
    // @TODO Not tested, did i understand this correctly?
    return *((u16*)g_flash_bar + addr);
}

u16 i82574_eeprom_read(u8 addr) {
    /* Tell the EEPROM to start reading */
    write_register(CARD_REG_EERD, CARD_MAKE_EERD_START(addr));

    u32 data;
    while (1) {
        data = read_register(CARD_REG_EERD);
        if (CARD_IS_EERD_DONE(data)) {
            return CARD_FIELD_EERD_DATA(data);
        }
    }

    // @TODO Timeout
    return 0;
}



#define NUM_OF_TX_DESCRIPTORS 8
#define SIZE_OF_TX_DESCRIPTOR_BUFFER 4096
#define NUM_OF_RX_DESCRIPTORS 32
#define SIZE_OF_RX_DESCRIPTOR_BUFFER 4096

TransmitDescriptor* g_transmit_ring;
ReceiveDescriptor* g_receive_ring;
volatile u32 g_ring_lock;

static void setup_transmit_ring() {
    // @TODO Tweak the paraameters.
    int g_transmit_ring_size = NUM_OF_TX_DESCRIPTORS * 16;
    g_transmit_ring = PMEM_alloc_phys(g_transmit_ring_size, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    memset(g_transmit_ring, 0, g_transmit_ring_size);
    
    if ((u64)g_transmit_ring_size & 127) {
        KERNEL_PANIC(false, "transmit ring alignment");
        return;
    }
    // Ring must be 16-byte aligned
    if ((u64)g_transmit_ring & 15) {
        KERNEL_PANIC(false, "transmit ring alignment");
        return;
    }

    for (int i = 0; i < NUM_OF_TX_DESCRIPTORS; i++){
        TransmitDescriptor* descriptor = g_transmit_ring + i;
        descriptor->buffer_address = PMEM_alloc_phys(SIZE_OF_TX_DESCRIPTOR_BUFFER, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    }
    
    write_register(CARD_REG_TDBAL, ((u64)g_transmit_ring) & 0xFFFFFFFF);
    write_register(CARD_REG_TDBAH, ((u64)g_transmit_ring) >> 32);
    write_register(CARD_REG_TDLEN, g_transmit_ring_size);
    write_register(CARD_REG_TDH, 0);
    write_register(CARD_REG_TDT, 0);
    
    // Set the Enable (EN) and Pad Short Packets (PSP) bits
    u32 tctl = CARD_BIT_TCTL_EN | CARD_BIT_TCTL_PSP;
    write_register(CARD_REG_TCTL, tctl);
}


static void setup_receive_ring() {
    int g_receive_ring_size = NUM_OF_RX_DESCRIPTORS * 16; // you can substitute 16 with sizeof(receive_descriptor_t)
    g_receive_ring = PMEM_alloc_phys(g_receive_ring_size, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    memset(g_receive_ring, 0, g_receive_ring_size);
    // Ring size must be 128-byte aligned
    if ((u64)g_receive_ring_size & 127) {
        KERNEL_PANIC(false, "receive ring alignment");
        return;
    }
    // Ring must be 16-byte aligned
    if ((u64)g_receive_ring & 15) {
        KERNEL_PANIC(false, "receive ring alignment");
        return;
    }
    
    for (int i = 0; i < NUM_OF_RX_DESCRIPTORS; i++){
        ReceiveDescriptor* descriptor = g_receive_ring + i;
        descriptor->buffer_address = PMEM_alloc_phys(SIZE_OF_RX_DESCRIPTOR_BUFFER, PMEM_FLAG_IDENTITY_MAPPED | PMEM_FLAG_NOT_CACHED);
    }
    
    write_register(CARD_REG_RDBAL, ((u64)g_receive_ring) & 0xFFFFFFFF); // Base Address Low
    write_register(CARD_REG_RDBAH, ((u64)g_receive_ring) >> 32); // Base Address High
    write_register(CARD_REG_RDLEN, g_receive_ring_size); // Ring Size
    write_register(CARD_REG_RDH, 0); // Set it to the first descriptor
    write_register(CARD_REG_RDT, NUM_OF_RX_DESCRIPTORS - 1); // Set it to the last descriptor
    
    // Set the Enable, Long Packet Reception, Broadcast Accept Mode and Size Extenstion bits
    // Also set the buffer size. This configuration (BSIZE = 0b11 and BSEX = 1) means 4096 (4kB) buffers
    u32 rctl = CARD_BIT_RCTL_EN | CARD_BIT_RCTL_LPE | CARD_BIT_RCTL_BAM | CARD_BIT_RCTL_BSEX | CARD_VAL_RCTL_BSIZE(0b11);
    write_register(CARD_REG_RCTL, rctl); 
}


static void send_data(const void* data, u32 size, bool EOP){
    // @TODO We need to check if buffer is full!

    LOCK_INT(&g_ring_lock);

    u32 tail = read_register(CARD_REG_TDT);
    TransmitDescriptor* tx = g_transmit_ring + tail; // Get the descriptor the tail is pointing at (next available descriptor)


    memcpy(tx->buffer_address, data, size); // Copy the data to the previously allocated buffer

    tx->length = size; // Set the length of the descriptor

    if (EOP) {
        tx->cmd |= CARD_BIT_TD_CMD_EOP | CARD_BIT_TD_CMD_IFCS; // If its the last one, set EOP
    }
    tail = (tail + 1) % NUM_OF_TX_DESCRIPTORS;
    write_register(CARD_REG_TDT, tail); // Increment and write the tail

    UNLOCK_INT(&g_ring_lock);
}

u32 i82574_send_packet(const void* data, u32 length){
    int sent = 0;
    // split the data into chunks and send them
    while (sent < length){
        int to_send = min(length - sent, SIZE_OF_TX_DESCRIPTOR_BUFFER);
        send_data((void*)((u64)data + sent), to_send, to_send == (length - sent));
        sent += to_send;
    }
    return sent;
}


static u8 rx_next = 0;

void i82574_receive_packet(void** out_buffer, u32* out_size) {
    // printf("RECV packets!\n");

    LOCK_INT(&g_ring_lock);

    u32 idx = rx_next;

    void* buffer = NULL; // use this to store the buffer.
    size_t buffer_len = 0; 

    while (g_receive_ring[idx].status & CARD_BIT_RD_STATUS_DD) {
        // This descriptor has been filled
        
        bool eop = g_receive_ring[idx].status & CARD_BIT_RD_STATUS_EOP;
        u16 len = g_receive_ring[idx].length;
        void* data = g_receive_ring[idx].buffer_address;
        
        // Handle multiple-descriptor packets
        if (buffer == NULL){ // This is the first descriptor of the packet
            // @TODO Keep a pre-allocated list of allocations to use.
            // @TODO Can we send buffer in ring directly and swap it with a fresh one
            //    to avoid memcpy?
            buffer = PMEM_alloc(len); // use your kernel's heap allocator
            buffer_len = len;
            memcpy(buffer, data, len);
        } else {
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
        g_receive_ring[idx].status = 0; 

        idx = (idx + 1) % NUM_OF_RX_DESCRIPTORS;

        if (eop) {
            break;
        }
    }

    if (rx_next != idx) {
        // printf("recv\n");
        // Give the controller more free descriptors by updating RDT
        u32 tail = (idx == 0) ? NUM_OF_RX_DESCRIPTORS - 1 : idx - 1;
        write_register(CARD_REG_RDT, tail);
        // printf("doneskies\n");
    }
    rx_next = idx;

    *out_buffer = buffer;
    *out_size = buffer_len;

    UNLOCK_INT(&g_ring_lock);
}


void i82574_interrupt_handler(u32 vector, InterruptFrame* frame) {
    // @TODO THIS IS SLOW. Implement high kernel mapping.
    u64 prev_cr3 = read_cr3();
    write_cr3((size_t)g_kernelPageTable);
    

    // @TODO Check which network controller triggered.


    u32 cause = read_register(CARD_REG_ICR);
    // @TODO Don't assume interrupt is because we got a packet.
    //    It's fine because we can still check if we have packet and do nothing if we don't.
    // @TODO How to handle overrun.
    // printf("i8254x interrupt 0x%x\n", cause);

    void* buffer = NULL;
    u32 bufferSize = 0;
    i82574_receive_packet(&buffer, &bufferSize);

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

