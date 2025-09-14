/*
 * INTEL82801_XP.CPP - Intel 82801 PCI Bridge Configuration Utility
 * Visual Studio 2010 / Windows XP version with dynamic WinIO loading
 * Usage: INTEL82801_XP [options]
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

// Intel 82801 PCI IDs
#define INTEL_VENDOR_ID     0x8086

// Known Intel 82801 Device IDs
#define ICH2_DEVICE_ID      0x244E
#define ICH2M_DEVICE_ID     0x2448
#define ICH3_DEVICE_ID      0x248C
#define ICH4_DEVICE_ID      0x24C4
#define ICH5_DEVICE_ID      0x24D4
#define ICH6_DEVICE_ID      0x2640
#define ICH7_DEVICE_ID      0x2770

// PCI Bridge Register Offsets (Type 1 Header)
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_HEADER_TYPE     0x0E
#define PCI_PRIMARY_BUS     0x18
#define PCI_SECONDARY_BUS   0x19
#define PCI_SUBORDINATE_BUS 0x1A
#define PCI_IO_BASE         0x1C
#define PCI_IO_LIMIT        0x1D
#define PCI_MEMORY_BASE     0x20
#define PCI_MEMORY_LIMIT    0x22
#define PCI_IO_BASE_UPPER   0x30
#define PCI_IO_LIMIT_UPPER  0x32

// Bridge configuration structure
typedef struct {
    char name[32];           // Short name identifier
    char description[80];    // Human-readable description
    BYTE secondary_bus;      // Secondary bus number
    BYTE subordinate_bus;    // Subordinate bus number
    WORD io_base;            // I/O base (bits 15:12)
    WORD io_limit;           // I/O limit (bits 15:12)
    WORD io_base_upper;      // Upper 16 bits of I/O base
    WORD io_limit_upper;     // Upper 16 bits of I/O limit
    WORD mem_base;           // Memory base (bits 31:20)
    WORD mem_limit;          // Memory limit (bits 31:20)
    WORD command_flags;      // PCI command register flags
} BRIDGE_CONFIG;

// Global variables
static int bridge_bus = -1, bridge_dev = -1, bridge_func = -1;
static WORD bridge_device_id = 0;
static BOOL hardware_access_available = FALSE;
static OSVERSIONINFO os_version;

// Preset configurations for common ISA setups
BRIDGE_CONFIG presets[] = {
    // Standard ISA forwarding - forwards full ISA I/O and memory ranges
    {
        "ISA_FULL", "Full ISA Range Forwarding",
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0000,      // I/O base = 0x0000
        0x00FF,      // I/O limit = 0xFFFF (full 64K I/O space)
        0x0000,      // I/O upper base = 0x0000
        0x0000,      // I/O upper limit = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF (ISA memory hole)
        0x0007       // Enable I/O, Memory, Bus Master
    },
    
    // Limited ISA forwarding - only low I/O range
    {
        "ISA_LOW", "Low ISA I/O Range (0x0000-0x0FFF)",
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0000,      // I/O base = 0x0000
        0x000F,      // I/O limit = 0x0FFF
        0x0000,      // I/O upper base = 0x0000
        0x0000,      // I/O upper limit = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF
        0x0007       // Enable I/O, Memory, Bus Master
    },
    
    // High ISA forwarding - only high I/O range
    {
        "ISA_HIGH", "High ISA I/O Range (0x0100-0x03FF)",
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0001,      // I/O base = 0x0100
        0x0003,      // I/O limit = 0x03FF
        0x0000,      // I/O upper base = 0x0000
        0x0000,      // I/O upper limit = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF
        0x0007       // Enable I/O, Memory, Bus Master
    },
    
    // Sound card range
    {
        "SOUND", "Sound Card I/O Range (0x0200-0x03FF)",
        1,           // Secondary bus = 1
        255,         // Subordinate bus = 255
        0x0002,      // I/O base = 0x0200
        0x0003,      // I/O limit = 0x03FF
        0x0000,      // I/O upper base = 0x0000
        0x0000,      // I/O upper limit = 0x0000
        0x00A0,      // Memory base = 0xA0000
        0x00FF,      // Memory limit = 0xFFFFF
        0x0007       // Enable I/O, Memory, Bus Master
    },
    
    // Disabled bridge
    {
        "DISABLE", "Disable Bridge (No Forwarding)",
        0,           // Secondary bus = 0
        0,           // Subordinate bus = 0
        0x00FF,      // I/O base > limit (disabled)
        0x0000,      // I/O limit < base (disabled)
        0x0000,      // I/O upper base = 0x0000
        0x0000,      // I/O upper limit = 0x0000
        0x00FF,      // Memory base > limit (disabled)
        0x0000,      // Memory limit < base (disabled)
        0x0000       // All disabled
    }
};

#define NUM_PRESETS (sizeof(presets) / sizeof(presets[0]))

// Function prototypes
BOOL init_hardware_access(void);
void cleanup_hardware_access(void);
DWORD pci_read_config_dword(int bus, int dev, int func, int reg);
WORD pci_read_config_word(int bus, int dev, int func, int reg);
BYTE pci_read_config_byte(int bus, int dev, int func, int reg);
void pci_write_config_dword(int bus, int dev, int func, int reg, DWORD val);
void pci_write_config_word(int bus, int dev, int func, int reg, WORD val);
void pci_write_config_byte(int bus, int dev, int func, int reg, BYTE val);
BOOL port_out_dword(WORD port, DWORD value);
DWORD port_in_dword(WORD port);
int find_intel_bridge(void);
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
char *get_ich_name(WORD device_id);

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

// Find Intel 82801 bridge
int find_intel_bridge(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for Intel 82801 bridge...\n");
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                DWORD id = pci_read_config_dword(bus, dev, func, 0);
                BYTE header_type = pci_read_config_byte(bus, dev, func, PCI_HEADER_TYPE);
                
                if ((id & 0xFFFF) == INTEL_VENDOR_ID && (header_type & 0x7F) == 1) {
                    bridge_device_id = (WORD)(id >> 16);
                    bridge_bus = bus;
                    bridge_dev = dev;
                    bridge_func = func;
                    printf("Found Intel bridge %s at Bus %d, Device %d, Function %d\n", 
                           get_ich_name(bridge_device_id), bus, dev, func);
                    return 1;
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
    }
    
    printf("ERROR: Intel 82801 bridge not found!\n");
    printf("Note: Looking for PCI-to-PCI bridges (Header Type 1) only.\n");
    return 0;
}

// Get ICH name from device ID
char *get_ich_name(WORD device_id) {
    switch (device_id) {
        case ICH2_DEVICE_ID:  return "ICH2 (82801BA)";
        case ICH2M_DEVICE_ID: return "ICH2 Mobile (82801BAM)";
        case ICH3_DEVICE_ID:  return "ICH3 (82801CA/CAM)";
        case ICH4_DEVICE_ID:  return "ICH4 (82801DB/DBM)";
        case ICH5_DEVICE_ID:  return "ICH5 (82801EB/ER)";
        case ICH6_DEVICE_ID:  return "ICH6 (82801FB/FR/FW/FRW)";
        case ICH7_DEVICE_ID:  return "ICH7 (82801GB/GR/GDH/GBM/GHM)";
        default:              return "Unknown Intel Bridge";
    }
}

// Configure bridge with given configuration
int configure_bridge(BRIDGE_CONFIG *config) {
    DWORD bus_config;
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot configure - hardware access not available!\n");
        return 0;
    }
    
    printf("Configuring Intel 82801 Bridge: %s\n", config->description);
    
    // Configure bus numbers
    bus_config = ((DWORD)config->subordinate_bus << 16) |
                 ((DWORD)config->secondary_bus << 8) |
                 0; // Primary bus = 0
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, PCI_PRIMARY_BUS, bus_config);
    printf("  Bus numbers: Primary=0, Secondary=%d, Subordinate=%d\n", 
           config->secondary_bus, config->subordinate_bus);
    
    // Configure I/O forwarding
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_IO_BASE, 
                         (BYTE)(config->io_base & 0xFF));
    pci_write_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_IO_LIMIT, 
                         (BYTE)(config->io_limit & 0xFF));
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PCI_IO_BASE_UPPER, 
                         config->io_base_upper);
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PCI_IO_LIMIT_UPPER, 
                         config->io_limit_upper);
    
    if (config->io_base <= config->io_limit) {
        DWORD io_start = ((DWORD)config->io_base_upper << 16) | ((DWORD)config->io_base << 8);
        DWORD io_end = ((DWORD)config->io_limit_upper << 16) | ((DWORD)config->io_limit << 8) | 0xFF;
        printf("  I/O forwarding: 0x%08X - 0x%08X\n", io_start, io_end);
    } else {
        printf("  I/O forwarding: Disabled\n");
    }
    
    // Configure memory forwarding
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PCI_MEMORY_BASE, config->mem_base);
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PCI_MEMORY_LIMIT, config->mem_limit);
    
    if (config->mem_base <= config->mem_limit) {
        DWORD mem_start = (DWORD)config->mem_base << 16;
        DWORD mem_end = ((DWORD)config->mem_limit << 16) | 0xFFFFF;
        printf("  Memory forwarding: 0x%08X - 0x%08X\n", mem_start, mem_end);
    } else {
        printf("  Memory forwarding: Disabled\n");
    }
    
    // Configure command register
    pci_write_config_word(bridge_bus, bridge_dev, bridge_func, PCI_COMMAND, config->command_flags);
    printf("  Command flags: 0x%04X", config->command_flags);
    if (config->command_flags & 0x01) printf(" [I/O]");
    if (config->command_flags & 0x02) printf(" [Memory]");
    if (config->command_flags & 0x04) printf(" [Bus Master]");
    printf("\n");
    
    printf("Bridge configuration completed successfully!\n");
    return 1;
}

// Reset bridge to defaults
int reset_bridge(void) {
    BRIDGE_CONFIG reset_config = {"RESET", "Reset Bridge", 0, 0, 0x00FF, 0x0000, 0, 0, 0x00FF, 0x0000, 0};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot reset - hardware access not available!\n");
        return 0;
    }
    
    printf("Resetting Intel 82801 bridge to defaults...\n");
    return configure_bridge(&reset_config);
}

// Scan and display current configuration
int scan_bridge(void) {
    DWORD value;
    BYTE primary_bus, secondary_bus, subordinate_bus;
    BYTE io_base, io_limit;
    WORD io_base_upper, io_limit_upper;
    WORD mem_base, mem_limit;
    WORD command;
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    printf("\nCurrent Intel 82801 Bridge Configuration:\n");
    printf("=========================================\n");
    printf("Location: Bus %d, Device %d, Function %d\n", bridge_bus, bridge_dev, bridge_func);
    printf("Bridge Type: %s\n", get_ich_name(bridge_device_id));
    
    // Show device ID
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, 0);
    printf("Device ID: 0x%04X, Vendor ID: 0x%04X\n", (value >> 16) & 0xFFFF, value & 0xFFFF);
    
    // Show command register
    command = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PCI_COMMAND);
    printf("Command Register: 0x%04X", command);
    if (command & 0x01) printf(" [I/O]");
    if (command & 0x02) printf(" [Memory]");
    if (command & 0x04) printf(" [Bus Master]");
    printf("\n");
    
    // Show bus numbers
    primary_bus = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_PRIMARY_BUS);
    secondary_bus = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_SECONDARY_BUS);
    subordinate_bus = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_SUBORDINATE_BUS);
    printf("Bus Numbers: Primary=%d, Secondary=%d, Subordinate=%d\n", 
           primary_bus, secondary_bus, subordinate_bus);
    
    // Show I/O forwarding
    io_base = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_IO_BASE);
    io_limit = pci_read_config_byte(bridge_bus, bridge_dev, bridge_func, PCI_IO_LIMIT);
    io_base_upper = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PCI_IO_BASE_UPPER);
    io_limit_upper = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PCI_IO_LIMIT_UPPER);
    
    if (io_base <= io_limit) {
        DWORD io_start = ((DWORD)io_base_upper << 16) | ((DWORD)io_base << 8);
        DWORD io_end = ((DWORD)io_limit_upper << 16) | ((DWORD)io_limit << 8) | 0xFF;
        printf("I/O Forwarding: 0x%08X - 0x%08X (%s)\n", 
               io_start, io_end, (command & 0x01) ? "Enabled" : "Disabled");
    } else {
        printf("I/O Forwarding: Not configured\n");
    }
    
    // Show memory forwarding
    mem_base = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PCI_MEMORY_BASE);
    mem_limit = pci_read_config_word(bridge_bus, bridge_dev, bridge_func, PCI_MEMORY_LIMIT);
    
    if (mem_base <= mem_limit) {
        DWORD mem_start = (DWORD)mem_base << 16;
        DWORD mem_end = ((DWORD)mem_limit << 16) | 0xFFFFF;
        printf("Memory Forwarding: 0x%08X - 0x%08X (%s)\n", 
               mem_start, mem_end, (command & 0x02) ? "Enabled" : "Disabled");
    } else {
        printf("Memory Forwarding: Not configured\n");
    }
    
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
    printf("Enter configuration description: ");
    fgets(config.description, sizeof(config.description), stdin);
    trim(config.description);
    
    printf("Secondary bus number (1-254): ");
    scanf("%d", &choice);
    config.secondary_bus = (BYTE)choice;
    
    printf("Subordinate bus number (%d-254): ", config.secondary_bus);
    scanf("%d", &choice);
    config.subordinate_bus = (BYTE)choice;
    
    printf("\nConfigure I/O forwarding:\n");
    printf("I/O base address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.io_base = (WORD)(value >> 8);
        config.io_base_upper = (WORD)(value >> 16);
    }
    
    printf("I/O limit address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.io_limit = (WORD)(value >> 8);
        config.io_limit_upper = (WORD)(value >> 16);
    }
    
    printf("\nConfigure memory forwarding:\n");
    printf("Memory base address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.mem_base = (WORD)(value >> 16);
    }
    
    printf("Memory limit address (hex, no 0x): ");
    scanf("%s", buffer);
    if (parse_hex(trim(buffer), &value)) {
        config.mem_limit = (WORD)(value >> 16);
    }
    
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
    
    printf("\nIntel 82801 Bridge Configuration Utility (Windows XP)\n");
    printf("=====================================================\n");
    if (bridge_bus >= 0) {
        printf("Bridge: %s at Bus %d, Device %d, Function %d\n\n", 
               get_ich_name(bridge_device_id), bridge_bus, bridge_dev, bridge_func);
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
    printf("  R. Reset bridge (disable forwarding)\n");
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
    
    fprintf(f, "; Intel 82801 Bridge Configuration File\n");
    fprintf(f, "[INFO]\n");
    fprintf(f, "NAME=%s\n", config->name);
    fprintf(f, "DESCRIPTION=%s\n", config->description);
    fprintf(f, "\n[BRIDGE]\n");
    fprintf(f, "SECONDARY_BUS=%d\n", config->secondary_bus);
    fprintf(f, "SUBORDINATE_BUS=%d\n", config->subordinate_bus);
    fprintf(f, "IO_BASE=0x%04X\n", config->io_base);
    fprintf(f, "IO_LIMIT=0x%04X\n", config->io_limit);
    fprintf(f, "IO_BASE_UPPER=0x%04X\n", config->io_base_upper);
    fprintf(f, "IO_LIMIT_UPPER=0x%04X\n", config->io_limit_upper);
    fprintf(f, "MEM_BASE=0x%04X\n", config->mem_base);
    fprintf(f, "MEM_LIMIT=0x%04X\n", config->mem_limit);
    fprintf(f, "COMMAND_FLAGS=0x%04X\n", config->command_flags);
    
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
        else if (strcmp(key, "COMMAND_FLAGS") == 0) {
            if (parse_hex(value, &temp_value)) {
                config->command_flags = (WORD)temp_value;
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
        printf("Intel Bridge: %s at Bus %d, Device %d, Function %d\n",
               get_ich_name(bridge_device_id), bridge_bus, bridge_dev, bridge_func);
    } else {
        printf("Intel Bridge: Not detected\n");
    }
    printf("\n");
}

// Utility functions
void decode_bridge_config(BRIDGE_CONFIG *config, char *buffer) {
    DWORD io_start = ((DWORD)config->io_base_upper << 16) | ((DWORD)config->io_base << 8);
    DWORD io_end = ((DWORD)config->io_limit_upper << 16) | ((DWORD)config->io_limit << 8) | 0xFF;
    DWORD mem_start = (DWORD)config->mem_base << 16;
    DWORD mem_end = ((DWORD)config->mem_limit << 16) | 0xFFFFF;
    
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
    
    printf("Intel 82801 Bridge Configuration Utility v1.0 (Windows XP)\n");
    printf("Usage:\n");
    printf("  INTEL82801_XP                    - Interactive mode\n");
    printf("  INTEL82801_XP -preset <name>     - Load preset configuration\n");
    printf("  INTEL82801_XP -load <file.cfg>   - Load configuration file\n");
    printf("  INTEL82801_XP -reset             - Reset bridge to defaults\n");
    printf("  INTEL82801_XP -scan              - Scan and display current config\n");
    printf("  INTEL82801_XP -info              - Show system information\n");
    printf("  INTEL82801_XP -help              - Show this help\n");
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
    printf("Intel 82801 PCI Bridge Configuration Utility v1.0\n");
    printf("==================================================\n");
    printf("Windows XP Version with Dynamic WinIo Loading\n\n");
    
    // Initialize hardware access
    if (!init_hardware_access()) {
        printf("Failed to initialize hardware access\n");
        return 1;
    }
    
    // Find Intel 82801 bridge if hardware access is available
    if (hardware_access_available) {
        find_intel_bridge();
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