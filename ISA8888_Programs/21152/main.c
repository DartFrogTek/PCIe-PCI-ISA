/*
 * 21152 - Intel 21152 PCI Bridge Configuration Utility
 * Visual Studio 2010 / Windows XP version with dynamic WinIO loading
 * Usage: 21152 [options]
 */

#define WIN32_LEAN_AND_MEAN
#define USE_WINIO
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>

// Dynamic WinIO function pointers
typedef BOOL (WINAPI *PFNINITIALIZEWINIO)(VOID);
typedef VOID (WINAPI *PFNSHUTDOWNWINIO)(VOID);
typedef BOOL (WINAPI *PFNGETPORTVAL)(WORD wPortAddr, PDWORD pdwPortVal, UCHAR bSize);
typedef BOOL (WINAPI *PFNSETPORTVAL)(WORD wPortAddr, DWORD dwPortVal, UCHAR bSize);

static HMODULE hWinIo = NULL;
static PFNINITIALIZEWINIO pInitializeWinIo = NULL;
static PFNSHUTDOWNWINIO pShutdownWinIo = NULL;
static PFNGETPORTVAL pGetPortVal = NULL;
static PFNSETPORTVAL pSetPortVal = NULL;

// PCI Configuration Space Access
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// Intel 21152 PCI IDs
#define INTEL_VENDOR_ID     0x8086
#define INTEL21152_DEVICE_ID 0x0024

// Intel 21152 Register Offsets (PCI-to-PCI Bridge Type 1 Header)
#define CMD_REG             0x04
#define STATUS_REG          0x06
#define PRIMARY_BUS         0x18
#define SECONDARY_BUS       0x19
#define SUBORDINATE_BUS     0x1A
#define SEC_LATENCY         0x1B
#define IO_BASE             0x1C
#define IO_LIMIT            0x1D
#define SEC_STATUS          0x1E
#define MEM_BASE            0x20
#define MEM_LIMIT           0x22
#define PREFETCH_BASE       0x24
#define PREFETCH_LIMIT      0x26
#define PREFETCH_BASE_UPPER 0x28
#define PREFETCH_LIMIT_UPPER 0x2C
#define IO_BASE_UPPER       0x30
#define IO_LIMIT_UPPER      0x32
#define BRIDGE_CONTROL      0x3E

// Configuration structure
typedef struct {
    char name[32];           // Short name identifier
    char description[80];    // Human-readable description
    BYTE primary_bus;        // Primary bus number (usually 0)
    BYTE secondary_bus;      // Secondary bus number
    BYTE subordinate_bus;    // Subordinate bus number
    WORD io_base;            // I/O base address (bits 15:12)
    WORD io_limit;           // I/O limit address (bits 15:12)
    WORD io_base_upper;      // I/O base upper 16 bits
    WORD io_limit_upper;     // I/O limit upper 16 bits
    WORD mem_base;           // Memory base (bits 31:20)
    WORD mem_limit;          // Memory limit (bits 31:20)
    WORD prefetch_base;      // Prefetchable memory base (bits 31:20)
    WORD prefetch_limit;     // Prefetchable memory limit (bits 31:20)
    DWORD prefetch_base_upper;  // Prefetchable base upper 32 bits
    DWORD prefetch_limit_upper; // Prefetchable limit upper 32 bits
    WORD command_flags;      // PCI command register
    WORD bridge_control;     // Bridge control register
} BRIDGE_CONFIG;

// Global variables
static int bridge_bus = -1, bridge_dev = -1, bridge_func = -1;
static BOOL hardware_access_available = FALSE;
static OSVERSIONINFO os_version;

// Preset configurations
BRIDGE_CONFIG presets[] = {
    // Full ISA forwarding configuration
    {
        "ISA_FULL", "Full ISA Range Forwarding (0x0000-0xFFFF I/O, 0xA0000-0xFFFFF Mem)",
        0,           // Primary bus = 0
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0000,      // I/O base = 0x0000
        0x00FF,      // I/O limit = 0xFFFF
        0x0000,      // I/O base upper = 0x0000
        0x0000,      // I/O limit upper = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF
        0x0001,      // Prefetchable base = disabled
        0x0000,      // Prefetchable limit = disabled
        0x00000000,  // Prefetchable base upper = 0
        0x00000000,  // Prefetchable limit upper = 0
        0x0007,      // Command: I/O + Memory + Bus Master
        0x0000       // Bridge control: default
    },
    
    // Sound card I/O range
    {
        "SOUND", "Sound Card I/O Range (0x0200-0x03FF)",
        0,           // Primary bus = 0
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0002,      // I/O base = 0x0200
        0x0003,      // I/O limit = 0x03FF
        0x0000,      // I/O base upper = 0x0000
        0x0000,      // I/O limit upper = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF
        0x0001,      // Prefetchable base = disabled
        0x0000,      // Prefetchable limit = disabled
        0x00000000,  // Prefetchable base upper = 0
        0x00000000,  // Prefetchable limit upper = 0
        0x0007,      // Command: I/O + Memory + Bus Master
        0x0000       // Bridge control: default
    },
    
    // VGA forwarding configuration
    {
        "VGA", "VGA Compatible Forwarding",
        0,           // Primary bus = 0
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0003,      // I/O base = 0x03B0 (covers VGA I/O)
        0x0003,      // I/O limit = 0x03DF
        0x0000,      // I/O base upper = 0x0000
        0x0000,      // I/O limit upper = 0x0000
        0x00A0,      // Memory base = 0xA0000 (VGA memory)
        0x00BF,      // Memory limit = 0xBFFFF
        0x0001,      // Prefetchable base = disabled
        0x0000,      // Prefetchable limit = disabled
        0x00000000,  // Prefetchable base upper = 0
        0x00000000,  // Prefetchable limit upper = 0
        0x0007,      // Command: I/O + Memory + Bus Master
        0x0008       // Bridge control: VGA enable
    },
    
    // ISA-aware mode
    {
        "ISA_AWARE", "ISA-Aware Mode (Standard ISA holes)",
        0,           // Primary bus = 0
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0000,      // I/O base = 0x0000
        0x00FF,      // I/O limit = 0xFFFF
        0x0000,      // I/O base upper = 0x0000
        0x0000,      // I/O limit upper = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF
        0x0001,      // Prefetchable base = disabled
        0x0000,      // Prefetchable limit = disabled
        0x00000000,  // Prefetchable base upper = 0
        0x00000000,  // Prefetchable limit upper = 0
        0x0007,      // Command: I/O + Memory + Bus Master
        0x0004       // Bridge control: ISA enable
    },
    
    // Disabled bridge
    {
        "DISABLE", "Disable Bridge (No Forwarding)",
        0,           // Primary bus = 0
        0,           // Secondary bus = 0
        0,           // Subordinate bus = 0
        0x00FF,      // I/O base > limit (disabled)
        0x0000,      // I/O limit < base (disabled)
        0x0000,      // I/O base upper = 0x0000
        0x0000,      // I/O limit upper = 0x0000
        0x00FF,      // Memory base > limit (disabled)
        0x0000,      // Memory limit < base (disabled)
        0x0001,      // Prefetchable base = disabled
        0x0000,      // Prefetchable limit = disabled
        0x00000000,  // Prefetchable base upper = 0
        0x00000000,  // Prefetchable limit upper = 0
        0x0000,      // Command: all disabled
        0x0040       // Bridge control: secondary reset
    }
};

#define NUM_PRESETS (sizeof(presets) / sizeof(presets[0]))

// Function prototypes
BOOL init_hardware_access(void);
void cleanup_hardware_access(void);
DWORD pci_read_config_dword(int bus, int dev, int func, int reg);
void pci_write_config_dword(int bus, int dev, int func, int reg, DWORD val);
WORD pci_read_config_word(int bus, int dev, int func, int reg);
void pci_write_config_word(int bus, int dev, int func, int reg, WORD val);
BYTE pci_read_config_byte(int bus, int dev, int func, int reg);
void pci_write_config_byte(int bus, int dev, int func, int reg, BYTE val);
BOOL port_out_dword(WORD port, DWORD value);
DWORD port_in_dword(WORD port);
int find_intel21152(void);
void show_main_menu(void);
int interactive_mode(void);
int load_preset(char *preset_name);
int configure_bridge(BRIDGE_CONFIG *config);
int reset_bridge(void);
int scan_bridge(void);
int manual_config_mode(void);
int save_config_file(char *filename, BRIDGE_CONFIG *config);
int load_config_file(char *filename, BRIDGE_CONFIG *config);
void show_usage(void);
void wait_key(void);
char *trim(char *str);
int parse_hex(char *str, DWORD *value);
void show_system_info(void);
void decode_bridge_config(BRIDGE_CONFIG *config, char *buffer);

// Hardware access initialization with dynamic WinIO loading
BOOL init_hardware_access(void) {
    os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os_version);
    
    printf("Initializing hardware access...\n");
    printf("Windows Version: %d.%d Build %d\n", 
           os_version.dwMajorVersion, os_version.dwMinorVersion, os_version.dwBuildNumber);
    
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        printf("Windows 95/98/ME - Direct hardware access enabled\n");
        hardware_access_available = TRUE;
    } else {
        printf("Windows NT/2000/XP - Loading WinIo driver...\n");
        
        // Load WinIO DLL dynamically to avoid static linking issues
        hWinIo = LoadLibraryA("WinIo32.dll");
        if (hWinIo) {
            // Get function addresses from the loaded DLL
            pInitializeWinIo = (PFNINITIALIZEWINIO)GetProcAddress(hWinIo, "InitializeWinIo");
            pShutdownWinIo = (PFNSHUTDOWNWINIO)GetProcAddress(hWinIo, "ShutdownWinIo");
            pGetPortVal = (PFNGETPORTVAL)GetProcAddress(hWinIo, "GetPortVal");
            pSetPortVal = (PFNSETPORTVAL)GetProcAddress(hWinIo, "SetPortVal");
            
            if (pInitializeWinIo && pShutdownWinIo && pGetPortVal && pSetPortVal) {
                if (pInitializeWinIo()) {
                    printf("WinIo driver loaded successfully\n");
                    hardware_access_available = TRUE;
                } else {
                    DWORD error = GetLastError();
                    printf("WinIo driver failed to initialize (Error: %d)\n", error);
                    printf("Common causes:\n");
                    printf("  - Not running as Administrator\n");
                    printf("  - Driver signing policy blocking unsigned driver\n");
                    printf("  - WinIo32.sys not found or corrupted\n");
                    hardware_access_available = FALSE;
                }
            } else {
                printf("Failed to get WinIo function addresses\n");
                hardware_access_available = FALSE;
            }
        } else {
            DWORD error = GetLastError();
            printf("Failed to load WinIo32.dll (Error: %d)\n", error);
            printf("Make sure WinIo32.dll is in the application directory\n");
            hardware_access_available = FALSE;
        }
    }
    
    if (!hardware_access_available) {
        printf("WARNING: Hardware access not available - running in scan-only mode\n");
    }
    
    return TRUE;
}

void cleanup_hardware_access(void) {
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT && hardware_access_available) {
        if (pShutdownWinIo) {
            pShutdownWinIo();
            printf("WinIo driver unloaded\n");
        }
        if (hWinIo) {
            FreeLibrary(hWinIo);
            hWinIo = NULL;
        }
    }
}

// Port I/O functions with dynamic WinIO support
BOOL port_out_dword(WORD port, DWORD value) {
    if (!hardware_access_available) {
        return FALSE;
    }
    
    __try {
        if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            // Windows 95/98 - Direct port access via inline assembly
            __asm {
                mov dx, port
                mov eax, value
                out dx, eax
            }
        } else {
            // Windows NT/2000/XP - Use WinIo driver
            if (!pSetPortVal || !pSetPortVal(port, value, 4)) {
                return FALSE;
            }
        }
        return TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        printf("Port I/O exception at port 0x%X\n", port);
        return FALSE;
    }
}

DWORD port_in_dword(WORD port) {
    DWORD value = 0xFFFFFFFF;
    
    if (!hardware_access_available) {
        return value;
    }
    
    __try {
        if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            // Windows 95/98 - Direct port access via inline assembly
            __asm {
                mov dx, port
                in eax, dx
                mov value, eax
            }
        } else {
            // Windows NT/2000/XP - Use WinIo driver  
            if (!pGetPortVal || !pGetPortVal(port, &value, 4)) {
                return 0xFFFFFFFF;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        printf("Port I/O exception at port 0x%X\n", port);
        return 0xFFFFFFFF;
    }
    
    return value;
}

// PCI configuration access functions
DWORD pci_read_config_dword(int bus, int dev, int func, int reg) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    if (!port_out_dword(PCI_CONFIG_ADDRESS, addr)) {
        return 0xFFFFFFFF;
    }
    return port_in_dword(PCI_CONFIG_DATA);
}

WORD pci_read_config_word(int bus, int dev, int func, int reg) {
    DWORD dword_val = pci_read_config_dword(bus, dev, func, reg & 0xFC);
    if ((reg & 2) == 0) {
        return (WORD)(dword_val & 0xFFFF);
    } else {
        return (WORD)(dword_val >> 16);
    }
}

BYTE pci_read_config_byte(int bus, int dev, int func, int reg) {
    DWORD dword_val = pci_read_config_dword(bus, dev, func, reg & 0xFC);
    return (BYTE)(dword_val >> ((reg & 3) * 8));
}

void pci_write_config_dword(int bus, int dev, int func, int reg, DWORD val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    port_out_dword(PCI_CONFIG_ADDRESS, addr);
    port_out_dword(PCI_CONFIG_DATA, val);
}

void pci_write_config_word(int bus, int dev, int func, int reg, WORD val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    DWORD dword_val;
    DWORD mask;
    
    port_out_dword(PCI_CONFIG_ADDRESS, addr);
    dword_val = port_in_dword(PCI_CONFIG_DATA);
    
    if ((reg & 2) == 0) {
        mask = 0xFFFF0000;
        dword_val = (dword_val & mask) | val;
    } else {
        mask = 0x0000FFFF;
        dword_val = (dword_val & mask) | ((DWORD)val << 16);
    }
    
    port_out_dword(PCI_CONFIG_DATA, dword_val);
}

void pci_write_config_byte(int bus, int dev, int func, int reg, BYTE val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    DWORD dword_val;
    DWORD mask;
    int shift = (reg & 3) * 8;
    
    port_out_dword(PCI_CONFIG_ADDRESS, addr);
    dword_val = port_in_dword(PCI_CONFIG_DATA);
    
    mask = ~(0xFFUL << shift);
    dword_val = (dword_val & mask) | ((DWORD)val << shift);
    
    port_out_dword(PCI_CONFIG_DATA, dword_val);
}

// Find Intel 21152 bridge
int find_intel21152(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for Intel 21152 bridge...\n");
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                DWORD id = pci_read_config_dword(bus, dev, func, 0);
                BYTE header_type = pci_read_config_byte(bus, dev, func, 0x0E);
                
                if ((id & 0xFFFF) == INTEL_VENDOR_ID && 
                    (id >> 16) == INTEL21152_DEVICE_ID &&
                    (header_type & 0x7F) == 1) {
                    bridge_bus = bus;
                    bridge_dev = dev;
                    bridge_func = func;
                    printf("Found Intel 21152 PCI-to-PCI Bridge at Bus %d, Device %d, Function %d\n", 
                           bus, dev, func);
                    return 1;
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
    }
    
    printf("ERROR: Intel 21152 bridge not found!\n");
    printf("Note: Looking for Intel vendor (8086h) with device ID 0024h and header type 1\n");
    return 0;
}

// Configure bridge with given configuration
int configure_bridge(BRIDGE_CONFIG *config) {
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot configure - hardware access not available!\n");
        return 0;
    }
    
    printf("Configuring Intel 21152 Bridge: %s\n", config->description);
    
    // Configure bus numbers
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, PRIMARY_BUS, config->primary_bus);
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, SECONDARY_BUS, config->secondary_bus);
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, SUBORDINATE_BUS, config->subordinate_bus);
    printf("  Bus numbers: Primary=%d, Secondary=%d, Subordinate=%d\n", 
           config->primary_bus, config->secondary_bus, config->subordinate_bus);
    
    // Configure I/O forwarding
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, IO_BASE, 
                         (BYTE)((config->io_base << 4) | 0x01));
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, IO_LIMIT, 
                         (BYTE)((config->io_limit << 4) | 0x01));
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, IO_BASE_UPPER, config->io_base_upper);
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, IO_LIMIT_UPPER, config->io_limit_upper);
    
    if (config->io_base <= config->io_limit) {
        DWORD io_start = ((DWORD)config->io_base_upper << 16) | ((DWORD)config->io_base << 12);
        DWORD io_end = ((DWORD)config->io_limit_upper << 16) | ((DWORD)config->io_limit << 12) | 0xFFF;
        printf("  I/O forwarding: 0x%08X - 0x%08X\n", io_start, io_end);
    } else {
        printf("  I/O forwarding: Disabled\n");
    }
    
    // Configure memory forwarding
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, MEM_BASE, config->mem_base << 4);
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, MEM_LIMIT, config->mem_limit << 4);
    
    if (config->mem_base <= config->mem_limit) {
        DWORD mem_start = (DWORD)config->mem_base << 20;
        DWORD mem_end = ((DWORD)config->mem_limit << 20) | 0xFFFFF;
        printf("  Memory forwarding: 0x%08X - 0x%08X\n", mem_start, mem_end);
    } else {
        printf("  Memory forwarding: Disabled\n");
    }
    
    // Configure prefetchable memory forwarding
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PREFETCH_BASE, 
                         (config->prefetch_base << 4) | 0x0001);
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PREFETCH_LIMIT, 
                         (config->prefetch_limit << 4) | 0x0001);
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, PREFETCH_BASE_UPPER, 
                          config->prefetch_base_upper);
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, PREFETCH_LIMIT_UPPER, 
                          config->prefetch_limit_upper);
    
    if (config->prefetch_base <= config->prefetch_limit) {
        printf("  Prefetchable memory: Enabled\n");
    } else {
        printf("  Prefetchable memory: Disabled\n");
    }
    
    // Configure bridge control register
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, BRIDGE_CONTROL, config->bridge_control);
    printf("  Bridge control: 0x%04X", config->bridge_control);
    if (config->bridge_control & 0x0004) printf(" [ISA]");
    if (config->bridge_control & 0x0008) printf(" [VGA]");
    if (config->bridge_control & 0x0040) printf(" [SecReset]");
    printf("\n");
    
    // Configure command register (must be last to enable forwarding)
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, CMD_REG, config->command_flags);
    printf("  Command flags: 0x%04X", config->command_flags);
    if (config->command_flags & 0x01) printf(" [I/O]");
    if (config->command_flags & 0x02) printf(" [Memory]");
    if (config->command_flags & 0x04) printf(" [Bus Master]");
    if (config->command_flags & 0x20) printf(" [VGA Snoop]");
    printf("\n");
    
    printf("Bridge configuration completed successfully!\n");
    return 1;
}

// Reset bridge to defaults
int reset_bridge(void) {
    BRIDGE_CONFIG reset_config = {"RESET", "Reset Bridge", 0, 0, 0, 0x00FF, 0x0000, 0, 0, 
                                 0x00FF, 0x0000, 0x0001, 0x0000, 0, 0, 0x0000, 0x0040};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot reset - hardware access not available!\n");
        return 0;
    }
    
    printf("Resetting Intel 21152 bridge to defaults...\n");
    
    // First reset the secondary bus
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, BRIDGE_CONTROL, 0x0040);
    Sleep(100);
    
    // Then apply the reset configuration
    return configure_bridge(&reset_config);
}

// Scan and display current configuration
int scan_bridge(void) {
    DWORD value;
    BYTE primary_bus, secondary_bus, subordinate_bus;
    WORD io_base, io_limit, io_base_upper, io_limit_upper;
    WORD mem_base, mem_limit;
    WORD prefetch_base, prefetch_limit;
    DWORD prefetch_base_upper, prefetch_limit_upper;
    WORD command, bridge_control;
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    printf("\nCurrent Intel 21152 Bridge Configuration:\n");
    printf("=========================================\n");
    printf("Location: Bus %d, Device %d, Function %d\n", bridge_bus, bridge_dev, bridge_func);
    
    // Show device ID
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, 0);
    printf("Device ID: 0x%04X, Vendor ID: 0x%04X\n", (value >> 16) & 0xFFFF, value & 0xFFFF);
    
    // Show command register
    command = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    printf("Command Register: 0x%04X", command);
    if (command & 0x01) printf(" [I/O]");
    if (command & 0x02) printf(" [Memory]");
    if (command & 0x04) printf(" [Bus Master]");
    if (command & 0x20) printf(" [VGA Snoop]");
    printf("\n");
    
    // Show bus numbers
    primary_bus = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, PRIMARY_BUS);
    secondary_bus = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, SECONDARY_BUS);
    subordinate_bus = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, SUBORDINATE_BUS);
    printf("Bus Numbers: Primary=%d, Secondary=%d, Subordinate=%d\n", 
           primary_bus, secondary_bus, subordinate_bus);
    
    // Show I/O forwarding
    io_base = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, IO_BASE) >> 4;
    io_limit = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, IO_LIMIT) >> 4;
    io_base_upper = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, IO_BASE_UPPER);
    io_limit_upper = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, IO_LIMIT_UPPER);
    
    if (io_base <= io_limit) {
        DWORD io_start = ((DWORD)io_base_upper << 16) | ((DWORD)io_base << 12);
        DWORD io_end = ((DWORD)io_limit_upper << 16) | ((DWORD)io_limit << 12) | 0xFFF;
        printf("I/O Forwarding: 0x%08X - 0x%08X (%s)\n", 
               io_start, io_end, (command & 0x01) ? "Enabled" : "Disabled");
    } else {
        printf("I/O Forwarding: Not configured\n");
    }
    
    // Show memory forwarding
    mem_base = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, MEM_BASE) >> 4;
    mem_limit = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, MEM_LIMIT) >> 4;
    
    if (mem_base <= mem_limit) {
        DWORD mem_start = (DWORD)mem_base << 20;
        DWORD mem_end = ((DWORD)mem_limit << 20) | 0xFFFFF;
        printf("Memory Forwarding: 0x%08X - 0x%08X (%s)\n", 
               mem_start, mem_end, (command & 0x02) ? "Enabled" : "Disabled");
    } else {
        printf("Memory Forwarding: Not configured\n");
    }
    
    // Show prefetchable memory forwarding
    prefetch_base = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PREFETCH_BASE) >> 4;
    prefetch_limit = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PREFETCH_LIMIT) >> 4;
    prefetch_base_upper = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, PREFETCH_BASE_UPPER);
    prefetch_limit_upper = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, PREFETCH_LIMIT_UPPER);
    
    if (prefetch_base <= prefetch_limit) {
        printf("Prefetchable Memory: Enabled\n");
    } else {
        printf("Prefetchable Memory: Disabled\n");
    }
    
    // Show bridge control
    bridge_control = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, BRIDGE_CONTROL);
    printf("Bridge Control: 0x%04X", bridge_control);
    if (bridge_control & 0x0004) printf(" [ISA]");
    if (bridge_control & 0x0008) printf(" [VGA]");
    if (bridge_control & 0x0040) printf(" [SecReset]");
    printf("\n");
    
    printf("\n");
    return 1;
}

// Manual configuration mode
int manual_config_mode(void) {
    BRIDGE_CONFIG config = {0};
    int choice;
    DWORD value;
    char buffer[256];
    
    printf("\nManual Bridge Configuration Mode\n");
    printf("================================\n");
    
    strcpy(config.name, "MANUAL");
    printf("Enter bridge description: ");
    fgets(config.description, sizeof(config.description), stdin);
    trim(config.description);
    
    // Configure bus numbers
    printf("Primary bus number (usually 0): ");
    scanf("%d", &choice);
    config.primary_bus = (BYTE)choice;
    
    printf("Secondary bus number (1-254): ");
    scanf("%d", &choice);
    config.secondary_bus = (BYTE)choice;
    
    printf("Subordinate bus number (%d-254): ", config.secondary_bus);
    scanf("%d", &choice);
    config.subordinate_bus = (BYTE)choice;
    
    // Configure I/O forwarding
    printf("\nConfigure I/O forwarding:\n");
    printf("I/O base address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.io_base = (WORD)(value >> 12);
        config.io_base_upper = (WORD)(value >> 16);
    }
    
    printf("I/O limit address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.io_limit = (WORD)(value >> 12);
        config.io_limit_upper = (WORD)(value >> 16);
    }
    
    // Configure memory forwarding
    printf("\nConfigure memory forwarding:\n");
    printf("Memory base address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.mem_base = (WORD)(value >> 20);
    }
    
    printf("Memory limit address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.mem_limit = (WORD)(value >> 20);
    }
    
    // Configure control options
    printf("\nEnable I/O space? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.command_flags |= 0x01;
    }
    
    printf("Enable memory space? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.command_flags |= 0x02;
    }
    
    printf("Enable bus master? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.command_flags |= 0x04;
    }
    
    printf("Enable ISA mode? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.bridge_control |= 0x04;
    }
    
    printf("Enable VGA forwarding? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.bridge_control |= 0x08;
    }
    
    // Set default values for unused fields
    config.prefetch_base = 0x0001;  // Disabled
    config.prefetch_limit = 0x0000;
    config.prefetch_base_upper = 0;
    config.prefetch_limit_upper = 0;
    
    printf("\nConfiguration Summary:\n");
    decode_bridge_config(&config, buffer);
    printf("%s\n", buffer);
    
    printf("\nApply configuration? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        configure_bridge(&config);
        
        printf("Save configuration to file? (y/n): ");
        choice = _getch();
        printf("%c\n", choice);
        if (choice == 'y' || choice == 'Y') {
            printf("Filename: ");
            scanf("%s", buffer);
            save_config_file(trim(buffer), &config);
        }
    }
    
    return 1;
}

// Load preset configuration
int load_preset(char *preset_name) {
    int i;
    
    for (i = 0; i < NUM_PRESETS; i++) {
        if (_stricmp(presets[i].name, preset_name) == 0) {
            return configure_bridge(&presets[i]);
        }
    }
    
    printf("ERROR: Preset '%s' not found!\n", preset_name);
    printf("Available presets: ");
    for (i = 0; i < NUM_PRESETS; i++) {
        printf("%s", presets[i].name);
        if (i < NUM_PRESETS - 1) printf(", ");
    }
    printf("\n");
    return 0;
}

// Show main menu
void show_main_menu(void) {
    int i;
    
    printf("\nIntel 21152 Bridge Configuration Utility (Windows XP)\n");
    printf("=====================================================\n");
    if (bridge_bus >= 0) {
        printf("Bridge: Intel 21152 at Bus %d, Device %d, Function %d\n\n", 
               bridge_bus, bridge_dev, bridge_func);
    }
    printf("Preset Configurations:\n");
    for (i = 0; i < NUM_PRESETS; i++) {
        printf("  %d. %-10s - %s\n", i + 1, presets[i].name, presets[i].description);
    }
    printf("\nOther Options:\n");
    printf("  M. Manual configuration\n");
    printf("  L. Load configuration file\n");
    printf("  S. Scan current configuration\n");
    printf("  I. Show system information\n");
    printf("  R. Reset bridge (disable all)\n");
    printf("  Q. Quit\n");
    printf("\nChoice: ");
}

// Interactive mode
int interactive_mode(void) {
    int choice;
    char filename[256];
    BRIDGE_CONFIG config;
    
    while (1) {
        show_main_menu();
        choice = _getch();
        printf("%c\n", choice);
        
        if (choice >= '1' && choice <= '0' + NUM_PRESETS) {
            configure_bridge(&presets[choice - '1']);
            wait_key();
        }
        else if (choice == 'M' || choice == 'm') {
            manual_config_mode();
            wait_key();
        }
        else if (choice == 'L' || choice == 'l') {
            printf("Configuration filename: ");
            scanf("%s", filename);
            if (load_config_file(trim(filename), &config)) {
                configure_bridge(&config);
            }
            wait_key();
        }
        else if (choice == 'S' || choice == 's') {
            scan_bridge();
            wait_key();
        }
        else if (choice == 'I' || choice == 'i') {
            show_system_info();
            wait_key();
        }
        else if (choice == 'R' || choice == 'r') {
            reset_bridge();
            wait_key();
        }
        else if (choice == 'Q' || choice == 'q') {
            break;
        }
        else {
            printf("Invalid choice!\n");
        }
    }
    
    return 1;
}

// Configuration file functions
int save_config_file(char *filename, BRIDGE_CONFIG *config) {
    FILE *f;
    
    f = fopen(filename, "w");
    if (!f) {
        printf("ERROR: Cannot create file '%s'\n", filename);
        return 0;
    }
    
    fprintf(f, "; Intel 21152 Bridge Configuration File\n");
    fprintf(f, "[INFO]\n");
    fprintf(f, "NAME=%s\n", config->name);
    fprintf(f, "DESCRIPTION=%s\n", config->description);
    fprintf(f, "\n[BUS_NUMBERS]\n");
    fprintf(f, "PRIMARY_BUS=%d\n", config->primary_bus);
    fprintf(f, "SECONDARY_BUS=%d\n", config->secondary_bus);
    fprintf(f, "SUBORDINATE_BUS=%d\n", config->subordinate_bus);
    fprintf(f, "\n[IO_FORWARDING]\n");
    fprintf(f, "IO_BASE=0x%04X\n", config->io_base);
    fprintf(f, "IO_LIMIT=0x%04X\n", config->io_limit);
    fprintf(f, "IO_BASE_UPPER=0x%04X\n", config->io_base_upper);
    fprintf(f, "IO_LIMIT_UPPER=0x%04X\n", config->io_limit_upper);
    fprintf(f, "\n[MEMORY_FORWARDING]\n");
    fprintf(f, "MEM_BASE=0x%04X\n", config->mem_base);
    fprintf(f, "MEM_LIMIT=0x%04X\n", config->mem_limit);
    fprintf(f, "PREFETCH_BASE=0x%04X\n", config->prefetch_base);
    fprintf(f, "PREFETCH_LIMIT=0x%04X\n", config->prefetch_limit);
    fprintf(f, "PREFETCH_BASE_UPPER=0x%08X\n", config->prefetch_base_upper);
    fprintf(f, "PREFETCH_LIMIT_UPPER=0x%08X\n", config->prefetch_limit_upper);
    fprintf(f, "\n[CONTROL]\n");
    fprintf(f, "COMMAND_FLAGS=0x%04X\n", config->command_flags);
    fprintf(f, "BRIDGE_CONTROL=0x%04X\n", config->bridge_control);
    
    fclose(f);
    printf("Configuration saved to '%s'\n", filename);
    return 1;
}

int load_config_file(char *filename, BRIDGE_CONFIG *config) {
    FILE *f;
    char line[256];
    char *key, *value;
    DWORD temp_value;
    
    memset(config, 0, sizeof(BRIDGE_CONFIG));
    
    f = fopen(filename, "r");
    if (!f) {
        printf("ERROR: Cannot open file '%s'\n", filename);
        return 0;
    }
    
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == ';' || line[0] == '[' || line[0] == 0) continue;
        
        key = strtok(line, "=");
        value = strtok(NULL, "=");
        if (!key || !value) continue;
        
        key = trim(key);
        value = trim(value);
        
        if (strcmp(key, "NAME") == 0) {
            strcpy(config->name, value);
        }
        else if (strcmp(key, "DESCRIPTION") == 0) {
            strcpy(config->description, value);
        }
        else if (strcmp(key, "PRIMARY_BUS") == 0) {
            config->primary_bus = (BYTE)atoi(value);
        }
        else if (strcmp(key, "SECONDARY_BUS") == 0) {
            config->secondary_bus = (BYTE)atoi(value);
        }
        else if (strcmp(key, "SUBORDINATE_BUS") == 0) {
            config->subordinate_bus = (BYTE)atoi(value);
        }
        else if (strcmp(key, "IO_BASE") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->io_base = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "IO_LIMIT") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->io_limit = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "IO_BASE_UPPER") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->io_base_upper = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "IO_LIMIT_UPPER") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->io_limit_upper = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "MEM_BASE") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->mem_base = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "MEM_LIMIT") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->mem_limit = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "PREFETCH_BASE") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->prefetch_base = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "PREFETCH_LIMIT") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->prefetch_limit = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "PREFETCH_BASE_UPPER") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->prefetch_base_upper = temp_value;
            }
        }
        else if (strcmp(key, "PREFETCH_LIMIT_UPPER") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->prefetch_limit_upper = temp_value;
            }
        }
        else if (strcmp(key, "COMMAND_FLAGS") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->command_flags = (WORD)temp_value;
            }
        }
        else if (strcmp(key, "BRIDGE_CONTROL") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->bridge_control = (WORD)temp_value;
            }
        }
    }
    
    fclose(f);
    printf("Configuration loaded from '%s'\n", filename);
    return 1;
}

// Show system information
void show_system_info(void) {
    SYSTEM_INFO sys_info;
    MEMORYSTATUS mem_status;
    
    GetSystemInfo(&sys_info);
    GlobalMemoryStatus(&mem_status);
    
    printf("\nSystem Information:\n");
    printf("==================\n");
    printf("Windows Version: %d.%d Build %d %s\n",
           os_version.dwMajorVersion, os_version.dwMinorVersion, os_version.dwBuildNumber,
           (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) ? "(9x/ME)" : "(NT/2000/XP)");
    printf("Processor: %s (%d CPUs)\n",
           (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) ? "Intel x86" : "Other",
           sys_info.dwNumberOfProcessors);
    printf("Memory: %d MB total, %d MB available\n", 
           (int)(mem_status.dwTotalPhys / (1024*1024)), 
           (int)(mem_status.dwAvailPhys / (1024*1024)));
    printf("Hardware Access: %s\n", hardware_access_available ? "Available" : "Not Available");
    
    if (hardware_access_available && hWinIo) {
        printf("WinIO Status: Loaded and initialized\n");
    } else if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        printf("Hardware Access: Direct port I/O (Windows 9x)\n");
    } else {
        printf("Hardware Access: Not available (need WinIO on NT-based systems)\n");
    }
    
    if (bridge_bus >= 0) {
        printf("Intel 21152 Bridge: Found at Bus %d, Device %d, Function %d\n",
               bridge_bus, bridge_dev, bridge_func);
    } else {
        printf("Intel 21152 Bridge: Not detected\n");
    }
    printf("\n");
}

// Utility functions
void decode_bridge_config(BRIDGE_CONFIG *config, char *buffer) {
    DWORD io_start = ((DWORD)config->io_base_upper << 16) | ((DWORD)config->io_base << 12);
    DWORD io_end = ((DWORD)config->io_limit_upper << 16) | ((DWORD)config->io_limit << 12) | 0xFFF;
    DWORD mem_start = (DWORD)config->mem_base << 20;
    DWORD mem_end = ((DWORD)config->mem_limit << 20) | 0xFFFFF;
    
    if (config->io_base <= config->io_limit) {
        sprintf(buffer, "Bus %d->%d, I/O: 0x%08X-0x%08X, Mem: 0x%08X-0x%08X", 
                config->secondary_bus, config->subordinate_bus,
                io_start, io_end, mem_start, mem_end);
    } else {
        sprintf(buffer, "Bus %d->%d, I/O: Disabled, Mem: 0x%08X-0x%08X", 
                config->secondary_bus, config->subordinate_bus,
                mem_start, mem_end);
    }
}

char *trim(char *str) {
    char *end;
    
    while (isspace(*str)) str++;
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    *(end + 1) = 0;
    
    return str;
}

int parse_hex(char *str, DWORD *value) {
    char *endptr;
    
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        str += 2;
    }
    
    *value = strtoul(str, &endptr, 16);
    return (*endptr == 0);
}

void wait_key(void) {
    printf("\nPress any key to continue...");
    _getch();
    printf("\n");
}

void show_usage(void) {
    int i;
    
    printf("Intel 21152 Bridge Configuration Utility v1.0 (Windows XP)\n");
    printf("Usage:\n");
    printf("  INTEL21152_XP                    - Interactive mode\n");
    printf("  INTEL21152_XP -preset <name>     - Load preset configuration\n");
    printf("  INTEL21152_XP -load <file.cfg>   - Load configuration file\n");
    printf("  INTEL21152_XP -reset             - Reset bridge to defaults\n");
    printf("  INTEL21152_XP -scan              - Scan and display current config\n");
    printf("  INTEL21152_XP -info              - Show system information\n");
    printf("  INTEL21152_XP -help              - Show this help\n");
    printf("\nAvailable presets: ");
    for (i = 0; i < NUM_PRESETS; i++) {
        printf("%s", presets[i].name);
        if (i < NUM_PRESETS - 1) printf(", ");
    }
    printf("\n");
    printf("\nNote: Must run as Administrator on Windows NT/2000/XP\n");
    printf("Required files: WinIo32.dll, WinIo32.sys\n");
}

// Main function
int main(int argc, char *argv[]) {
    printf("Intel 21152 PCI Bridge Configuration Utility v1.0\n");
    printf("==================================================\n");
    printf("Windows XP Version with Dynamic WinIo Loading\n\n");
    
    // Initialize hardware access
    if (!init_hardware_access()) {
        printf("Failed to initialize hardware access\n");
        return 1;
    }
    
    // Find Intel 21152 bridge if hardware access is available
    if (hardware_access_available) {
        find_intel21152();
    }
    
    // Process command line arguments
    if (argc == 1) {
        interactive_mode();
    }
    else if (argc >= 2) {
        if (strcmp(argv[1], "-preset") == 0 && argc >= 3) {
            if (load_preset(argv[2])) {
                cleanup_hardware_access();
                return 0;
            }
            cleanup_hardware_access();
            return 1;
        }
        else if (strcmp(argv[1], "-load") == 0 && argc >= 3) {
            BRIDGE_CONFIG config;
            if (load_config_file(argv[2], &config)) {
                if (configure_bridge(&config)) {
                    cleanup_hardware_access();
                    return 0;
                }
            }
            cleanup_hardware_access();
            return 1;
        }
        else if (strcmp(argv[1], "-reset") == 0) {
            if (reset_bridge()) {
                cleanup_hardware_access();
                return 0;
            }
            cleanup_hardware_access();
            return 1;
        }
        else if (strcmp(argv[1], "-scan") == 0) {
            if (scan_bridge()) {
                cleanup_hardware_access();
                return 0;
            }
            cleanup_hardware_access();
            return 1;
        }
        else if (strcmp(argv[1], "-info") == 0) {
            show_system_info();
            cleanup_hardware_access();
            return 0;
        }
        else if (strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "/?") == 0) {
            show_usage();
            cleanup_hardware_access();
            return 0;
        }
        else {
            printf("Invalid option: %s\n", argv[1]);
            show_usage();
            cleanup_hardware_access();
            return 1;
        }
    }
    
    cleanup_hardware_access();
    return 0;
}