#pragma once

#define FDC_PRIMARY_BASE        0x03F0u
#define FDC_SECONDARY_BASE      0x0370u

#define FDC_REG_STATUS_A        0u     /* read, PS/2 only */
#define FDC_REG_STATUS_B        1u     /* read, PS/2 only */
#define FDC_REG_DOR             2u     /* write */
#define FDC_REG_TDR             3u     /* tape drive register */
#define FDC_REG_MSR             4u     /* read */
#define FDC_REG_DSR             4u     /* write */
#define FDC_REG_FIFO            5u     /* read/write */
#define FDC_REG_DIR             7u     /* read */
#define FDC_REG_CCR             7u     /* write */

#define FDC_MSR_D0B             0x01u
#define FDC_MSR_D1B             0x02u
#define FDC_MSR_D2B             0x04u
#define FDC_MSR_D3B             0x08u
#define FDC_MSR_CB              0x10u
#define FDC_MSR_NDM             0x20u
#define FDC_MSR_DIO             0x40u
#define FDC_MSR_RQM             0x80u

#define FDC_DOR_DRIVE_MASK      0x03u
#define FDC_DOR_RESET           0x04u
#define FDC_DOR_DMA_IRQ         0x08u
#define FDC_DOR_MOTOR0          0x10u
#define FDC_DOR_MOTOR1          0x20u
#define FDC_DOR_MOTOR2          0x40u
#define FDC_DOR_MOTOR3          0x80u

#define FDC_CMD_SPECIFY         0x03u
#define FDC_CMD_SENSE_INTERRUPT 0x08u
#define FDC_CMD_RECALIBRATE     0x07u
#define FDC_CMD_SEEK            0x0Fu
#define FDC_CMD_READ_ID         0x4Au   /* MFM + READ ID */
#define FDC_CMD_READ_DATA       0xC6u   /* MT + MFM + READ DATA */
#define FDC_CMD_WRITE_DATA      0xC5u   /* MT + MFM + WRITE DATA */

#define FDC_SECTOR_SIZE_CODE_512 2u

#define FDC_ST0_INTERRUPT_CODE_MASK 0xC0u
#define FDC_ST0_SEEK_END            0x20u
#define FDC_ST0_EQUIPMENT_CHECK     0x10u
#define FDC_ST0_NOT_READY           0x08u
#define FDC_ST0_HEAD_ADDRESS        0x04u
#define FDC_ST0_DRIVE_MASK          0x03u
