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

// #pragma pack(push, 1)
// typedef struct TransmitDescriptor {
//     u64 buffer_address;
//     u16 length;

// } TransmitDescriptor;
// #pragma pack(pop)


void card_init();

