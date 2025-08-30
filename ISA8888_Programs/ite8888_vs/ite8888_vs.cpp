/*
 * IT8888CFG_XP.CPP - IT8888F Universal ISA Bridge Configuration Utility
 * Visual Studio 2010 / Windows XP version with dynamic WinIO loading
 * Usage: IT8888CFG_XP [options]
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

// IT8888F PCI IDs
#define ITE_VENDOR_ID       0x1283
#define ITE8888_DEVICE_ID   0x8888

// IT8888F Register Offsets
#define CMD_REG             0x04
#define ISA_CONTROL         0x50
#define MISC_CONTROL        0x54
#define IO_SPACE_0          0x58
#define IO_SPACE_1          0x5C
#define IO_SPACE_2          0x60
#define IO_SPACE_3          0x64
#define IO_SPACE_4          0x68
#define IO_SPACE_5          0x6C
#define MEM_SPACE_0         0x70
#define MEM_SPACE_1         0x74
#define MEM_SPACE_2         0x78
#define MEM_SPACE_3         0x7C
#define DMA_CHANNEL_01      0x40
#define DMA_CHANNEL_23      0x44
#define DMA_CHANNEL_5       0x48
#define DMA_CHANNEL_67      0x4C

// Configuration structure
typedef struct {
    char name[32];           // Short name identifier
    char description[80];    // Human-readable description
    DWORD io_spaces[6];      // 6 I/O space decode registers
    DWORD mem_spaces[4];     // 4 memory space decode registers  
    DWORD dma_config[4];     // DMA channel configurations
    DWORD misc_control;      // Miscellaneous control register
    DWORD isa_control;       // ISA-specific control register
} CARD_CONFIG;

// Global variables
static int bridge_bus = -1, bridge_dev = -1, bridge_func = -1;
static BOOL hardware_access_available = FALSE;
static OSVERSIONINFO os_version;

// Preset configurations
CARD_CONFIG presets[] = {
    // Gravis UltraSound - Detailed explanation of configuration format
    {
        "GUS", "Gravis UltraSound Audio Card",
        
        // I/O Space Configuration (6 registers available):
        // Format: 0x80000000 | (speed<<29) | (size<<24) | (base_address & 0xFFFF)
        // - Bit 31: Enable bit (1=enabled, 0=disabled)
        // - Bits 30-29: Access speed (0=Subtractive, 1=Slow, 2=Medium, 3=Fast)
        // - Bits 27-24: Size (0=1 byte, 1=2 bytes, 2=4 bytes, 3=8 bytes, 4=16 bytes, 5=32 bytes, 6=64 bytes, 7=128 bytes)
        // - Bits 15-0: Base I/O address
        {
            0xE4000220,  // I/O Space 0: 0x220-0x22F (16 bytes, fast access) - GUS main I/O port
            0xE3000330,  // I/O Space 1: 0x330-0x337 (8 bytes, fast access) - GUS MIDI port  
            0xE2000388,  // I/O Space 2: 0x388-0x38B (4 bytes, fast access) - AdLib/OPL2 compatibility
            0,           // I/O Space 3: Disabled
            0,           // I/O Space 4: Disabled  
            0            // I/O Space 5: Disabled
        },
        
        // Memory Space Configuration (4 registers available):
        // Format similar to I/O but for memory ranges
        // - Base address is shifted left by 8 bits (256-byte granularity)
        // - Size field specifies memory window size in 16KB increments
        {0, 0, 0, 0},    // All memory spaces disabled for GUS
        
        // DMA Channel Configuration (4 registers):
        // Format: 0x80000000 | (dma_channel & 0x0F)
        // - Bit 31: Enable bit
        // - Bits 3-0: DMA channel number (0-7)
        // Register mapping: [0]=DMA 0-1, [1]=DMA 2-3, [2]=DMA 5, [3]=DMA 6-7
        {
            0x80000001,  // Enable DMA channel 1 (8-bit audio playback)
            0,           // DMA 2-3 disabled
            0x80000005,  // Enable DMA channel 5 (16-bit audio recording) 
            0            // DMA 6-7 disabled
        },
        
        // Miscellaneous Control Register:
        // Bit 31: DDMA Concurrent Mode (1=enabled)
        // Bit 27: PCI Clock Enable (1=enabled) 
        // Bit 26: ISA Refresh Enable (1=enabled)
        // Bit 1: Delayed Transaction Enable (1=enabled)
        // Bit 0: Subtractive Decode Enable (1=enabled)
        0x8C000003,     // DDMA + PCI Clock + ISA Refresh + Subtractive + Delayed Transaction
        
        0x00000000      // ISA Control Register: Default/unused for GUS
    },
    
    // Floppy Drive Controller
    {
        "FDC", "Floppy Drive Controller",
        {0xE20003F0, 0, 0, 0, 0, 0},    // Standard FDC I/O at 0x3F0-0x3F7
        {0, 0, 0, 0},
        {0, 0x80000002, 0, 0},          // DMA channel 2 for floppy transfers
        0x8C000003,
        0x00000000
    },
    
    // Sound Blaster Compatible
    {
        "SB", "Sound Blaster Compatible",
        {0xE4000220, 0xE2000388, 0, 0, 0, 0},  // SB at 0x220, AdLib at 0x388
        {0, 0, 0, 0},
        {0x80000001, 0x80000005, 0, 0},        // DMA 1 (8-bit) and DMA 5 (16-bit)
        0x8C000003,
        0x00000000
    },
    
    // NE2000 Ethernet
    {
        "NE2000", "NE2000 Compatible Ethernet Adapter", 
        {0xE5000300, 0, 0, 0, 0, 0},    // NE2000 at 0x300-0x31F (32 bytes)
        {0, 0, 0, 0},
        {0, 0, 0, 0},                   // No DMA required
        0x8C000003,
        0x00000000
    },
    
    // SCSI Host Adapter
    {
        "SCSI", "SCSI Host Adapter",
        {0xE4000330, 0xE4000140, 0, 0, 0, 0},  // SCSI controller I/O ports
        {0, 0, 0, 0},
        {0, 0, 0x80000005, 0},          // DMA channel 5 for SCSI transfers
        0x8C000003,
        0x00000000
    },
    
    // Multi-Serial Controller  
    {
        "SERIAL", "Multi-Port Serial Controller",
        {0xE20003F8, 0xE20002F8, 0xE20003E8, 0xE20002E8, 0, 0}, // COM1-4 ports
        {0, 0, 0, 0},
        {0, 0, 0, 0},                   // Serial ports don't use DMA
        0x8C000001,                     // Subtractive decode only
        0x00000000
    }
};

#define NUM_PRESETS (sizeof(presets) / sizeof(presets[0]))

// Function prototypes
BOOL init_hardware_access(void);
void cleanup_hardware_access(void);
DWORD pci_read_config_dword(int bus, int dev, int func, int reg);
void pci_write_config_dword(int bus, int dev, int func, int reg, DWORD val);
BOOL port_out_dword(WORD port, DWORD value);
DWORD port_in_dword(WORD port);
int find_it8888f(void);
void show_main_menu(void);
int interactive_mode(void);
int load_preset(char *preset_name);
int configure_bridge(CARD_CONFIG *config);
int reset_bridge(void);
int scan_bridge(void);
int manual_config_mode(void);
int save_config_file(char *filename, CARD_CONFIG *config);
int load_config_file(char *filename, CARD_CONFIG *config);
void show_usage(void);
void decode_io_space(DWORD reg_val, char *buffer);
void decode_mem_space(DWORD reg_val, char *buffer);
void wait_key(void);
char *trim(char *str);
int parse_hex(char *str, DWORD *value);
void show_system_info(void);

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

void pci_write_config_dword(int bus, int dev, int func, int reg, DWORD val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    port_out_dword(PCI_CONFIG_ADDRESS, addr);
    port_out_dword(PCI_CONFIG_DATA, val);
}

// Find IT8888F bridge
int find_it8888f(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for IT8888F bridge...\n");
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                DWORD id = pci_read_config_dword(bus, dev, func, 0);
                
                if ((id & 0xFFFF) == ITE_VENDOR_ID && (id >> 16) == ITE8888_DEVICE_ID) {
                    bridge_bus = bus;
                    bridge_dev = dev;
                    bridge_func = func;
                    printf("Found IT8888F at Bus %d, Device %d, Function %d\n", 
                           bus, dev, func);
                    return 1;
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
    }
    
    printf("ERROR: IT8888F bridge not found!\n");
    return 0;
}

// Configure bridge with given configuration
int configure_bridge(CARD_CONFIG *config) {
    int i;
    DWORD cmd_reg;
    DWORD io_regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    DWORD mem_regs[] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    DWORD dma_regs[] = {DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot configure - hardware access not available!\n");
        return 0;
    }
    
    printf("Configuring IT8888F for: %s\n", config->description);
    
    // Enable I/O space, memory space, and bus mastering
    cmd_reg = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    cmd_reg |= 0x07; // Set bits 0, 1, 2 for I/O, Memory, Bus Master
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG, cmd_reg);
    printf("  PCI Command register: 0x%04X\n", cmd_reg & 0xFFFF);
    
    // Configure miscellaneous control register
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL, config->misc_control);
    printf("  Misc Control: 0x%08X\n", config->misc_control);
    
    // Configure ISA control register if needed
    if (config->isa_control) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, ISA_CONTROL, config->isa_control);
        printf("  ISA Control: 0x%08X\n", config->isa_control);
    }
    
    // Configure I/O spaces
    for (i = 0; i < 6; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, io_regs[i], config->io_spaces[i]);
        if (config->io_spaces[i] & 0x80000000) {
            char decode_buf[80];
            decode_io_space(config->io_spaces[i], decode_buf);
            printf("  I/O Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Configure memory spaces
    for (i = 0; i < 4; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, mem_regs[i], config->mem_spaces[i]);
        if (config->mem_spaces[i] & 0x80000000) {
            char decode_buf[80];
            decode_mem_space(config->mem_spaces[i], decode_buf);
            printf("  Memory Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Configure DMA channels
    for (i = 0; i < 4; i++) {
        if (config->dma_config[i]) {
            pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, dma_regs[i], config->dma_config[i]);
            printf("  DMA Channel %d: Enabled (0x%08X)\n", i, config->dma_config[i]);
        }
    }
    
    printf("Configuration completed successfully!\n");
    return 1;
}

// Reset bridge to defaults
int reset_bridge(void) {
    int i;
    DWORD regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5,
                   MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3,
                   DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot reset - hardware access not available!\n");
        return 0;
    }
    
    printf("Resetting IT8888F bridge to defaults...\n");
    
    // Clear all configuration registers
    for (i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, regs[i], 0);
    }
    
    // Reset control registers
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL, 0);
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, ISA_CONTROL, 0);
    
    printf("Bridge reset completed!\n");
    return 1;
}

// Scan and display current configuration
int scan_bridge(void) {
    int i;
    DWORD value;
    char decode_buf[80];
    DWORD io_regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    DWORD mem_regs[] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!hardware_access_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    printf("\nCurrent IT8888F Configuration:\n");
    printf("==============================\n");
    printf("Location: Bus %d, Device %d, Function %d\n", bridge_bus, bridge_dev, bridge_func);
    
    // Show device ID
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, 0);
    printf("Device ID: 0x%04X, Vendor ID: 0x%04X\n", (value >> 16) & 0xFFFF, value & 0xFFFF);
    
    // Show command register
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    printf("Command Register: 0x%04X", value & 0xFFFF);
    if (value & 0x01) printf(" [I/O Enabled]");
    if (value & 0x02) printf(" [Memory Enabled]");
    if (value & 0x04) printf(" [Bus Master]");
    printf("\n");
    
    // Show control registers
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL);
    printf("Misc Control: 0x%08X", value);
    if (value & 0x80000000) printf(" [DDMA-Concurrent]");
    if (value & 0x08000000) printf(" [PCI-Clock]");
    if (value & 0x04000000) printf(" [ISA-Refresh]");
    if (value & 0x00000001) printf(" [Subtractive]");
    if (value & 0x00000002) printf(" [Delayed-Tx]");
    printf("\n");
    
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, ISA_CONTROL);
    if (value) {
        printf("ISA Control: 0x%08X\n", value);
    }
    
    // Show I/O spaces
    printf("\nI/O Spaces:\n");
    for (i = 0; i < 6; i++) {
        value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, io_regs[i]);
        if (value & 0x80000000) {
            decode_io_space(value, decode_buf);
            printf("  Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Show memory spaces
    printf("\nMemory Spaces:\n");
    for (i = 0; i < 4; i++) {
        value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, mem_regs[i]);
        if (value & 0x80000000) {
            decode_mem_space(value, decode_buf);
            printf("  Space %d: %s\n", i, decode_buf);
        }
    }
    
    printf("\n");
    return 1;
}

// Manual configuration mode
int manual_config_mode(void) {
    CARD_CONFIG config = {0};
    int i, choice;
    DWORD base, size, speed;
    char buffer[256];
    
    printf("\nManual Configuration Mode\n");
    printf("=========================\n");
    
    strcpy(config.name, "MANUAL");
    printf("Enter card description: ");
    fgets(config.description, sizeof(config.description), stdin);
    trim(config.description);
    
    // Configure I/O spaces
    printf("\nConfigure I/O Spaces (6 available):\n");
    for (i = 0; i < 6; i++) {
        printf("I/O Space %d - Enable? (y/n): ", i);
        choice = _getch();
        printf("%c\n", choice);
        
        if (choice == 'y' || choice == 'Y') {
            printf("  Base address (hex, no 0x): ");
            scanf("%s", buffer);
            if (parse_hex(trim(buffer), &base)) {
                printf("  Size (0=1B, 1=2B, 2=4B, 3=8B, 4=16B, 5=32B, 6=64B, 7=128B): ");
                scanf("%d", &size);
                printf("  Speed (0=Subtractive, 1=Slow, 2=Medium, 3=Fast): ");
                scanf("%d", &speed);
                
                config.io_spaces[i] = 0x80000000 | (speed << 29) | (size << 24) | (base & 0xFFFF);
                printf("  Configured: 0x%04X-0x%04X\n", base, base + (1 << size) - 1);
            }
        }
    }
    
    // Configure DMA
    printf("\nConfigure DMA channels:\n");
    printf("8-bit DMA channel (0-3, 255=none): ");
    scanf("%d", &choice);
    if (choice >= 0 && choice <= 3) {
        config.dma_config[0] = 0x80000000 | choice;
    }
    
    printf("16-bit DMA channel (5-7, 255=none): ");
    scanf("%d", &choice);
    if (choice >= 5 && choice <= 7) {
        config.dma_config[2] = 0x80000000 | choice;
    }
    
    // Configure control options
    config.misc_control = 0x8C000000;
    printf("\nEnable subtractive decode? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.misc_control |= 0x01;
    }
    
    printf("Enable delayed transaction? (y/n): ");
    choice = _getch();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.misc_control |= 0x02;
    }
    
    printf("\nConfiguration Summary:\n");
    printf("Name: %s\n", config.description);
    for (i = 0; i < 6; i++) {
        if (config.io_spaces[i] & 0x80000000) {
            char decode_buf[80];
            decode_io_space(config.io_spaces[i], decode_buf);
            printf("I/O %d: %s\n", i, decode_buf);
        }
    }
    
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
    
    printf("\nIT8888F Universal Configuration Utility (Windows XP)\n");
    printf("====================================================\n");
    printf("Preset Configurations:\n");
    for (i = 0; i < NUM_PRESETS; i++) {
        printf("  %d. %-8s - %s\n", i + 1, presets[i].name, presets[i].description);
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
    CARD_CONFIG config;
    
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
int save_config_file(char *filename, CARD_CONFIG *config) {
    FILE *f;
    int i;
    
    f = fopen(filename, "w");
    if (!f) {
        printf("ERROR: Cannot create file '%s'\n", filename);
        return 0;
    }
    
    fprintf(f, "; IT8888F Configuration File\n");
    fprintf(f, "[INFO]\n");
    fprintf(f, "NAME=%s\n", config->name);
    fprintf(f, "DESCRIPTION=%s\n", config->description);
    fprintf(f, "\n[IO_SPACES]\n");
    
    for (i = 0; i < 6; i++) {
        fprintf(f, "IO%d=0x%08X\n", i, config->io_spaces[i]);
    }
    
    fprintf(f, "\n[MEMORY_SPACES]\n");
    for (i = 0; i < 4; i++) {
        fprintf(f, "MEM%d=0x%08X\n", i, config->mem_spaces[i]);
    }
    
    fprintf(f, "\n[DMA]\n");
    for (i = 0; i < 4; i++) {
        fprintf(f, "DMA%d=0x%08X\n", i, config->dma_config[i]);
    }
    
    fprintf(f, "\n[CONTROL]\n");
    fprintf(f, "MISC_CONTROL=0x%08X\n", config->misc_control);
    fprintf(f, "ISA_CONTROL=0x%08X\n", config->isa_control);
    
    fclose(f);
    printf("Configuration saved to '%s'\n", filename);
    return 1;
}

int load_config_file(char *filename, CARD_CONFIG *config) {
    FILE *f;
    char line[256];
    char *key, *value;
    
    memset(config, 0, sizeof(CARD_CONFIG));
    
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
        else if (strncmp(key, "IO", 2) == 0 && strlen(key) == 3) {
            int idx = key[2] - '0';
            if (idx >= 0 && idx < 6) {
                parse_hex(value, &config->io_spaces[idx]);
            }
        }
        else if (strncmp(key, "MEM", 3) == 0 && strlen(key) == 4) {
            int idx = key[3] - '0';
            if (idx >= 0 && idx < 4) {
                parse_hex(value, &config->mem_spaces[idx]);
            }
        }
        else if (strncmp(key, "DMA", 3) == 0 && strlen(key) == 4) {
            int idx = key[3] - '0';
            if (idx >= 0 && idx < 4) {
                parse_hex(value, &config->dma_config[idx]);
            }
        }
        else if (strcmp(key, "MISC_CONTROL") == 0) {
            parse_hex(value, &config->misc_control);
        }
        else if (strcmp(key, "ISA_CONTROL") == 0) {
            parse_hex(value, &config->isa_control);
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
        printf("IT8888F Bridge: Found at Bus %d, Device %d, Function %d\n",
               bridge_bus, bridge_dev, bridge_func);
    } else {
        printf("IT8888F Bridge: Not detected\n");
    }
    printf("\n");
}

// Utility functions
void decode_io_space(DWORD reg_val, char *buffer) {
    DWORD base = reg_val & 0xFFFF;
    DWORD size = (reg_val >> 24) & 0x07;
    DWORD speed = (reg_val >> 29) & 0x03;
    DWORD bytes = 1UL << size;
    char *speed_names[] = {"Sub", "Slow", "Med", "Fast"};
    
    sprintf(buffer, "0x%04X-0x%04X (%d bytes, %s)", 
            base, base + bytes - 1, bytes, speed_names[speed]);
}

void decode_mem_space(DWORD reg_val, char *buffer) {
    DWORD base = (reg_val & 0xFFFFFF) << 8;
    DWORD size = (reg_val >> 24) & 0x07;
    DWORD speed = (reg_val >> 29) & 0x03;
    DWORD kb = 16UL << size;
    char *speed_names[] = {"Sub", "Slow", "Med", "Fast"};
    
    sprintf(buffer, "0x%08X (%dKB, %s)", base, kb, speed_names[speed]);
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
    
    printf("IT8888F Universal Configuration Utility v1.0 (Windows XP)\n");
    printf("Usage:\n");
    printf("  IT8888CFG_XP                    - Interactive mode\n");
    printf("  IT8888CFG_XP -preset <name>     - Load preset (GUS, FDC, SB, etc.)\n");
    printf("  IT8888CFG_XP -load <file.cfg>   - Load configuration file\n");
    printf("  IT8888CFG_XP -reset             - Reset bridge to defaults\n");
    printf("  IT8888CFG_XP -scan              - Scan and display current config\n");
    printf("  IT8888CFG_XP -info              - Show system information\n");
    printf("  IT8888CFG_XP -help              - Show this help\n");
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
    printf("IT8888F Universal ISA Bridge Configuration Utility v1.0\n");
    printf("=======================================================\n");
    printf("Windows XP Version with Dynamic WinIo Loading\n\n");
    
    // Initialize hardware access
    if (!init_hardware_access()) {
        printf("Failed to initialize hardware access\n");
        return 1;
    }
    
    // Find IT8888F bridge if hardware access is available
    if (hardware_access_available) {
        find_it8888f();
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
            CARD_CONFIG config;
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