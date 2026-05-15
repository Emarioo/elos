
// typedef struct {
//     u32 last_access;
//     u32 address;
//     u8  mac[6];
// } ARP_Entry;

// #define ARP_TABLE_MAX 50
// int arp_table_len;
// u64 rdtsc_base;
// ARP_Entry arp_table[ARP_TABLE_MAX];

// bool fetch_mac_from_address(NetDevice device, u32 address,u8 mac[6]) {
//     ARP_Entry* entry = NULL;
//     ARP_Entry* oldest_entry = NULL;
//     u32        oldest_access = 0xFFFFFFFF;
//     for (int i=0;i<arp_table_len;i++) {
//         if (arp_table[i].address == address) {
//             entry = &arp_table[i];
//             break;
//         }
//         if (arp_table[i].last_access < oldest_access) {
//             oldest_access = arp_table[i].last_access;
//             oldest_entry = &arp_table[i];
//         }
//     }

//     if (rdtsc_base == 0) {
//         rdtsc_base = rdtsc();
//     }
//     u32 timestamp = (u32)((rdtsc() - rdtsc_base) / 1000000);

//     NET_send_arp(device, address);

//     while (true) {
//         NET_Packet packet;
//         bool found = NET_poll_packet(device, &packet);
//         if (found) {
//             u8 received_mac[6];
//             bool unpacked = NET_unpack_arp(packet, received_mac);
//             if (!unpacked) {
                
//             }
//         }
//         pause();
//     }

//     if (arp_table_len < ARP_TABLE_MAX) {
//         entry = &arp_table[arp_table_len];
//         arp_table_len++;
//     } else {
        
//     }

//     entry->last_access = timestamp;
//     memcpy(mac, entry->mac, 6);
//     return true;
// }