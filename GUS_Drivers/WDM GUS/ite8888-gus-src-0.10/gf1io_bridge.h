/*
 * gf1io_bridge.h
 *
 * Bridge-aware I/O function replacements for PicoGUS support
 */

#ifndef _GF1IO_BRIDGE_H_
#define _GF1IO_BRIDGE_H_

// These macros replace the direct port I/O when using bridge
// They should be conditionally compiled based on bridge detection

#ifdef BRIDGE_SUPPORT_ENABLED

// Bridge-aware byte write
#define BRIDGE_WRITE_PORT_UCHAR(port, value) \
    if (using_bridge) { \
        /* Use PCI I/O transaction instead of direct ISA */ \
        WRITE_PORT_UCHAR((PUCHAR)(port), (value)); \
    } else { \
        WRITE_PORT_UCHAR((port), (value)); \
    }

// Bridge-aware byte read  
#define BRIDGE_READ_PORT_UCHAR(port) \
    (using_bridge ? \
        READ_PORT_UCHAR((PUCHAR)(port)) : \
        READ_PORT_UCHAR((port)))

// Bridge-aware word write
#define BRIDGE_WRITE_PORT_USHORT(port, value) \
    if (using_bridge) { \
        WRITE_PORT_USHORT((PUSHORT)(port), (value)); \
    } else { \
        WRITE_PORT_USHORT((port), (value)); \
    }

// Bridge-aware word read
#define BRIDGE_READ_PORT_USHORT(port) \
    (using_bridge ? \
        READ_PORT_USHORT((PUSHORT)(port)) : \
        READ_PORT_USHORT((port)))

#else

// Fallback to direct I/O when bridge support is disabled
#define BRIDGE_WRITE_PORT_UCHAR(port, value) WRITE_PORT_UCHAR((port), (value))
#define BRIDGE_READ_PORT_UCHAR(port) READ_PORT_UCHAR((port))
#define BRIDGE_WRITE_PORT_USHORT(port, value) WRITE_PORT_USHORT((port), (value))
#define BRIDGE_READ_PORT_USHORT(port) READ_PORT_USHORT((port))

#endif // BRIDGE_SUPPORT_ENABLED

#endif // _GF1IO_BRIDGE_H_