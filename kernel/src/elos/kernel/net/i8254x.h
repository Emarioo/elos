#pragma once

#include "elos/common/types.h"

// CARD refers to i8254x. Poor naming.

#define CARD_REG_CTRL   0x0
#define CARD_REG_STATUS 0x8
#define CARD_REG_EECD   0x10
#define CARD_REG_EERD   0x14
#define CARD_REG_ICR    0xC0
#define CARD_REG_IMS    0xD0
#define CARD_REG_RCTL   0x100
#define CARD_REG_RDBAL  0x2800
#define CARD_REG_RDBAH  0x2804
#define CARD_REG_RDLEN  0x2808
#define CARD_REG_RDH    0x2810
#define CARD_REG_RDT    0x2818
#define CARD_REG_TCTL   0x400
#define CARD_REG_TDBAL  0x3800
#define CARD_REG_TDBAH  0x3804
#define CARD_REG_TDLEN  0x3808
#define CARD_REG_TDH    0x3810
#define CARD_REG_TDT    0x3818

#define CARD_REG_RAL(N) (0x5400 + (N) * 8)
#define CARD_REG_RAH(N) (0x5404 + (N) * 8)

#define CARD_BIT_CTRL_FD              0x1
#define CARD_BIT_CTRL_LRST            0x8
#define CARD_BIT_CTRL_ASDE            0x20
#define CARD_BIT_CTRL_SLU             0x40
#define CARD_BIT_CTRL_ILOS            0x80
#define CARD_MASK_CTRL_SPEED          0x300
#define CARD_BIT_CTRL_FRCSPD          0x800
#define CARD_BIT_CTRL_FRCDPLX         0x1000
#define CARD_BIT_CTRL_SDP0_DATA       0x40000
#define CARD_BIT_CTRL_SDP1_DATA       0x80000
#define CARD_BIT_CTRL_ADVD3WUC        0x100000
#define CARD_BIT_CTRL_EN_PHY_PWR_MGMT 0x200000
#define CARD_BIT_CTRL_SPD0_IODIR      0x400000
#define CARD_BIT_CTRL_SPD1_IODIR      0x800000
#define CARD_BIT_CTRL_RST             0x4000000
#define CARD_BIT_CTRL_RFCE            0x8000000
#define CARD_BIT_CTRL_TFCE            0x10000000
#define CARD_BIT_CTRL_VME             0x40000000
#define CARD_BIT_CTRL_PHY_RST         0x80000000

#define CARD_BIT_STATUS_FD           0x1
#define CARD_BIT_STATUS_LU           0x2
#define CARD_MASK_STATUS_FUNCTION_ID 0xC
#define CARD_BIT_STATUS_TXOFF        0x10
#define CARD_BIT_STATUS_TBIMODE      0x20
#define CARD_MASK_STATUS_SPEED       0xC0
#define CARD_MASK_STATUS_ASDV        0x300
#define CARD_BIT_STATUS_PCI66        0x800
#define CARD_BIT_STATUS_BUS64        0x1000
#define CARD_BIT_STATUS_PCIX_MODE    0x2000
#define CARD_MASK_STATUS_PCIXSPD     0xC000

#define CARD_BIT_EECD_SK        0x1
#define CARD_BIT_EECD_CS        0x2
#define CARD_BIT_EECD_DI        0x4
#define CARD_BIT_EECD_DO        0x8
#define CARD_BIT_EECD_FWE       0x10
#define CARD_BIT_EECD_EE_REQ    0x20
#define CARD_BIT_EECD_EE_GNT    0x40
#define CARD_BIT_EECD_EE_PRES   0x80
#define CARD_BIT_EECD_EE_SIZE_1 0x100
#define CARD_BIT_EECD_EE_SIZE_2 0x200
#define CARD_BIT_EECD_EE_TYPE   0x1000


#define CARD_BIT_EERD_START  0x1
#define CARD_BIT_EERD_DONE   0x10


#define CARD_BIT_TCTL_EN  (1 << 1)
#define CARD_BIT_TCTL_PSP (1 << 3)


#define CARD_BIT_RCTL_EN    (1 << 1)
#define CARD_BIT_RCTL_SBP   (1 << 2)
#define CARD_BIT_RCTL_UPE   (1 << 3)
#define CARD_BIT_RCTL_MPE   (1 << 3)
#define CARD_BIT_RCTL_LPE   (1 << 3)
#define CARD_BIT_RCTL_BAM   (1 << 15)
#define CARD_VAL_RCTL_BSIZE(N) ((N) << 16)
#define CARD_BIT_RCTL_BSEX  (1 << 25)

#define CARD_BIT_IMS_TXDW    (1 << 0)
#define CARD_BIT_IMS_TXQE    (1 << 1)
#define CARD_BIT_IMS_LSC     (1 << 2)
#define CARD_BIT_IMS_RXSEQ   (1 << 3)
#define CARD_BIT_IMS_RXDMT0  (1 << 4)
#define CARD_BIT_IMS_RXO     (1 << 6)
#define CARD_BIT_IMS_RXT0    (1 << 7)
#define CARD_BIT_IMS_MDAC    (1 << 9)
#define CARD_BIT_IMS_RXCFG   (1 << 10)
#define CARD_BIT_IMS_PHYINT  (1 << 12)
#define CARD_BIT_IMS_TXD_LOW (1 << 15)

#define CARD_BIT_ICR_TXDW    (1 << 0)
#define CARD_BIT_ICR_TXQE    (1 << 1)
#define CARD_BIT_ICR_LSC     (1 << 2)
#define CARD_BIT_ICR_RXSEQ   (1 << 3)
#define CARD_BIT_ICR_RXDMT0  (1 << 4)
#define CARD_BIT_ICR_RXO     (1 << 6)
#define CARD_BIT_ICR_RXT0    (1 << 7)
#define CARD_BIT_ICR_MDAC    (1 << 9)
#define CARD_BIT_ICR_RXCFG   (1 << 10)
#define CARD_BIT_ICR_PHYINT  (1 << 12)
#define CARD_BIT_ICR_TXD_LOW (1 << 15)



#pragma pack(push, 1)
typedef struct TransmitDescriptor {
    void* buffer_address;
    u16 length;
    u8  cso;
    u8  cmd;
    u8  sta : 4;
    u8  rsv : 4;
    u8  css;
    u16 special;
} TransmitDescriptor;
#pragma pack(pop)


#define CARD_BIT_TD_CMD_EOP  (1 << 0)
#define CARD_BIT_TD_CMD_IFCS (1 << 1)
#define CARD_BIT_TD_CMD_IC   (1 << 2)
#define CARD_BIT_TD_CMD_RS   (1 << 3)
#define CARD_BIT_TD_CMD_RPS  (1 << 4)
#define CARD_BIT_TD_CMD_DEXT (1 << 5)
#define CARD_BIT_TD_CMD_VLE  (1 << 6)
#define CARD_BIT_TD_CMD_IDE  (1 << 7)


#define CARD_BIT_TD_STA_DD   (1 << 0)
#define CARD_BIT_TD_STA_EC   (1 << 1)
#define CARD_BIT_TD_STA_LC   (1 << 2)
#define CARD_BIT_TD_STA_TU   (1 << 3)


#pragma pack(push, 1)
typedef struct ReceiveDescriptor {
    void* buffer_address;
    u16 length;
    u16 checksum;
    u8  status;
    u8  errors;
    u16 special;
} ReceiveDescriptor;
#pragma pack(pop)


#define CARD_BIT_RD_STATUS_DD    (1 << 0)
#define CARD_BIT_RD_STATUS_EOP   (1 << 1)
#define CARD_BIT_RD_STATUS_IXSM  (1 << 2)
#define CARD_BIT_RD_STATUS_VP    (1 << 3)
#define CARD_BIT_RD_STATUS_RSV   (1 << 4)
#define CARD_BIT_RD_STATUS_TCPCS (1 << 5)
#define CARD_BIT_RD_STATUS_IPCF  (1 << 6)
#define CARD_BIT_RD_STATUS_PIF   (1 << 7)


#define CARD_BIT_RD_ERRORS_CE   (1 << 0)
#define CARD_BIT_RD_ERRORS_SE   (1 << 1)
#define CARD_BIT_RD_ERRORS_SEQ  (1 << 2)
#define CARD_BIT_RD_ERRORS_RSV  (1 << 3)
#define CARD_BIT_RD_ERRORS_CXE  (1 << 4)
#define CARD_BIT_RD_ERRORS_TCPE (1 << 5)
#define CARD_BIT_RD_ERRORS_IPE  (1 << 6)
#define CARD_BIT_RD_ERRORS_RXE  (1 << 7)

bool card_init();

void receive_packet(void** out_buffer, int* out_size);

int send_packet(void* data, int size);

