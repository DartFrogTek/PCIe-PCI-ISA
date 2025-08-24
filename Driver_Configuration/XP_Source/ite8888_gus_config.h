/*
 * ITE8888_gus_config.h
 * 
 * ITE8888 Bridge Configuration - Pre-configured for GUS Cards
 * Just include this instead of ITE8888_config.h for GUS support
 */

#ifndef _ITE8888_GUS_CONFIG_H_
#define _ITE8888_GUS_CONFIG_H_

//
// Card identification
//
#define CARD_NAME               L"Gravis UltraSound"
#define CARD_DRIVER_VERSION     L"1.0"

//
// GUS I/O Space Configuration
//

// I/O Space 0 - Main GUS registers (0x220-0x22F)
#define IO_SPACE_0_ENABLE       1
#define IO_SPACE_0_BASE         0x220
#define IO_SPACE_0_SIZE         4           // 16 bytes  
#define IO_SPACE_0_SPEED        2           // Medium speed
#define IO_SPACE_0_ALIAS        0           // Full decode

// I/O Space 1 - MIDI/Voice registers (0x320-0x32F)
#define IO_SPACE_1_ENABLE       1
#define IO_SPACE_1_BASE         0x320
#define IO_SPACE_1_SIZE         4           // 16 bytes
#define IO_SPACE_1_SPEED        2           // Medium speed
#define IO_SPACE_1_ALIAS        0           // Full decode

// I/O Space 2 - Revision register (0x706)
#define IO_SPACE_2_ENABLE       1
#define IO_SPACE_2_BASE         0x706
#define IO_SPACE_2_SIZE         0           // 1 byte
#define IO_SPACE_2_SPEED        2           // Medium speed
#define IO_SPACE_2_ALIAS        0           // Full decode

// I/O Space 3-5 - Additional GUS ports if needed
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
// Memory spaces - disabled for GUS
//
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
// GUS DMA Configuration
//

// DMA Channel 1 (8-bit) - Standard GUS DMA
#define DMA_CHAN_1_ENABLE       1
#define DMA_CHAN_1_CHANNEL      1
#define DMA_CHAN_1_TYPE_F       0

// DMA Channel 5 (16-bit) - Standard GUS DMA
#define DMA_CHAN_5_ENABLE       1
#define DMA_CHAN_5_CHANNEL      5
#define DMA_CHAN_5_TYPE_F       1           // Type F for better performance

//
// Bridge Control Settings - Optimized for GUS
//
#define ENABLE_DELAYED_TRANSACTION  1       // Good for audio performance
#define ENABLE_SUBTRACTIVE_DECODE   1       // Handle any unmapped accesses
#define IO_RECOVERY_TIME           2       // Conservative timing for GUS

//
// IRQ Configuration
//
#define ENABLE_SERIAL_IRQ          1       // Enable for IRQ routing
#define SERIAL_IRQ_MODE            0       // Continuous mode

//
// Debug Configuration  
//
#define ENABLE_DEBUG_PRINTS        1       // Enable for troubleshooting
#define DEBUG_LEVEL_DEFAULT        2       // Info level

#endif // _ITE8888_GUS_CONFIG_H_