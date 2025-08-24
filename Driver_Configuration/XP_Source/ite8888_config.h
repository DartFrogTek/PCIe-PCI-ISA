/*
 * ite8888_config.h
 * 
 * ITE8888 Bridge Configuration - Modify these defines for your card
 */

#ifndef _ITE8888_CONFIG_H_
#define _ITE8888_CONFIG_H_

//
// Card identification - what card are we configuring for?
//
#define CARD_NAME               L"Generic ISA Card"
#define CARD_DRIVER_VERSION     L"1.0"

//
// I/O Space Configuration (up to 6 spaces supported by ITE8888)
// Set ENABLE to 1 to configure, 0 to disable
// SIZE: 0=1byte, 1=2bytes, 2=4bytes, 3=8bytes, 4=16bytes, 5=32bytes, 6=64bytes, 7=128bytes
// SPEED: 0=Subtractive, 1=Slow, 2=Medium, 3=Fast
// ALIAS: 0=Fully decode, 1=Ignore A[15:10] (legacy ISA compatibility)
//

// I/O Space 0 - Primary registers
#define IO_SPACE_0_ENABLE       1
#define IO_SPACE_0_BASE         0x220
#define IO_SPACE_0_SIZE         4           // 16 bytes
#define IO_SPACE_0_SPEED        2           // Medium speed
#define IO_SPACE_0_ALIAS        0           // Full decode

// I/O Space 1 - Secondary registers  
#define IO_SPACE_1_ENABLE       1
#define IO_SPACE_1_BASE         0x320
#define IO_SPACE_1_SIZE         4           // 16 bytes
#define IO_SPACE_1_SPEED        2           // Medium speed
#define IO_SPACE_1_ALIAS        0           // Full decode

// I/O Space 2 - Tertiary registers
#define IO_SPACE_2_ENABLE       1
#define IO_SPACE_2_BASE         0x706
#define IO_SPACE_2_SIZE         0           // 1 byte
#define IO_SPACE_2_SPEED        2           // Medium speed
#define IO_SPACE_2_ALIAS        0           // Full decode

// I/O Space 3-5 - Additional spaces (disabled by default)
#define IO_SPACE_3_ENABLE       0
#define IO_SPACE_3_BASE         0x000
#define IO_SPACE_3_SIZE         0
#define IO_SPACE_3_SPEED        2
#define IO_SPACE_3_ALIAS        0

#define IO_SPACE_4_ENABLE       0
#define IO_SPACE_4_BASE         0x000
#define IO_SPACE_4_SIZE         0
#define IO_SPACE_4_SPEED        2
#define IO_SPACE_4_ALIAS        0

#define IO_SPACE_5_ENABLE       0
#define IO_SPACE_5_BASE         0x000
#define IO_SPACE_5_SIZE         0
#define IO_SPACE_5_SPEED        2
#define IO_SPACE_5_ALIAS        0

//
// Memory Space Configuration (up to 4 spaces supported)
// SIZE: 0=16KB, 1=32KB, 2=64KB, 3=128KB, 4=256KB, 5=512KB, 6=1MB, 7=2MB
//

// Memory Space 0 - Usually disabled for I/O cards
#define MEM_SPACE_0_ENABLE      0
#define MEM_SPACE_0_BASE        0x00000000
#define MEM_SPACE_0_SIZE        0
#define MEM_SPACE_0_SPEED       2

#define MEM_SPACE_1_ENABLE      0
#define MEM_SPACE_1_BASE        0x00000000
#define MEM_SPACE_1_SIZE        0
#define MEM_SPACE_1_SPEED       2

#define MEM_SPACE_2_ENABLE      0
#define MEM_SPACE_2_BASE        0x00000000
#define MEM_SPACE_2_SIZE        0
#define MEM_SPACE_2_SPEED       2

#define MEM_SPACE_3_ENABLE      0
#define MEM_SPACE_3_BASE        0x00000000
#define MEM_SPACE_3_SIZE        0
#define MEM_SPACE_3_SPEED       2

//
// DMA Channel Configuration
// Set ENABLE to 1 to configure, 0 to disable
// CHANNEL: DMA channel number (0-7)
// TYPE_F: 0=Normal timing, 1=Type F timing (faster)
//

// DMA Channel 1 (8-bit)
#define DMA_CHAN_1_ENABLE       1
#define DMA_CHAN_1_CHANNEL      1
#define DMA_CHAN_1_TYPE_F       0

// DMA Channel 5 (16-bit) 
#define DMA_CHAN_5_ENABLE       1
#define DMA_CHAN_5_CHANNEL      5
#define DMA_CHAN_5_TYPE_F       1           // Type F for better performance

//
// Bridge Control Settings
//
#define ENABLE_DELAYED_TRANSACTION  1       // 0=Disabled, 1=Enabled
#define ENABLE_SUBTRACTIVE_DECODE   1       // 0=Disabled, 1=Enabled
#define IO_RECOVERY_TIME           4       // ISA bus recovery time (0-15)

//
// IRQ Configuration (Serial IRQ)
//
#define ENABLE_SERIAL_IRQ          1       // 0=Disabled, 1=Enabled
#define SERIAL_IRQ_MODE            0       // 0=Continuous, 1=Quiet

//
// Debug Configuration  
//
#define ENABLE_DEBUG_PRINTS        1       // 0=Disabled, 1=Enabled
#define DEBUG_LEVEL_DEFAULT        2       // 0=Error, 1=Warn, 2=Info, 3=Verbose

#endif // _ITE8888_CONFIG_H_