/*
 * ITE8888.C - ITE8888 Universal ISA Bridge Configuration Utility v1.1
 * Compiles with OpenWatcom C for 16-bit DOS
 * Uses .386 directive for atomic 32-bit PCI configuration access
 * Usage: ITE8888CFG [options]
 */

#if defined(__WATCOMC__)
    #include <conio.h>
    #include <i86.h>
#else
    #error "This version requires OpenWatcom C"
#endif

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// OpenWatcom compatibility
#define strcmpi stricmp

// 32-bit PCI port I/O functions using .386 directive
// These provide atomic 32-bit operations in 16-bit real mode

static void outpd(unsigned short port, unsigned long value) {
    _asm {
        .386
        mov dx, port
        mov eax, value
        out dx, eax
    }
}

static unsigned long inpd(unsigned short port) {
    unsigned long value;
    _asm {
        .386
        mov dx, port
        in eax, dx
        mov value, eax
    }
    return value;
}

//raw opcodes with db
/*
static void outpd(unsigned short port, unsigned long value) {
    _asm {
        mov dx, port
        // Encode: mov eax, value (opcode 0x66, 0xB8 + 4-byte immediate)
        db 0x66, 0xB8
        dw word ptr value      // Low 16 bits
        dw word ptr value+2    // High 16 bits
        // Encode: out dx, eax (opcode 0x66, 0xEF)
        db 0x66, 0xEF
    }
}

static unsigned long inpd(unsigned short port) {
    unsigned long value;
    _asm {
        mov dx, port
        // Encode: in eax, dx (opcode 0x66, 0xED)
        db 0x66, 0xED
        // Store result: mov word ptr value, ax gets low word
        mov word ptr value, ax
        // Shift eax right 16 bits to get high word into ax
        db 0x66, 0xC1, 0xE8, 0x10  // shr eax, 16
        mov word ptr value+2, ax
    }
    return value;
}
*/

// PCI Configuration Space Access
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// ITE8888 PCI IDs
#define ITE_VENDOR_ID       0x1283
#define ITE8888_DEVICE_ID   0x8888

// ITE8888 Register Offsets
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
    char name[32];
    char description[80];
    unsigned long io_spaces[6];
    unsigned long mem_spaces[4];
    unsigned long dma_config[4];
    unsigned long misc_control;
    unsigned long isa_control;
} CARD_CONFIG;

// Global variables
static int bridge_bus = -1, bridge_dev = -1, bridge_func = -1;

// Preset configurations
CARD_CONFIG presets[] = {
    // Gravis UltraSound
    {
        "GUS", "Gravis UltraSound Audio Card",
        {0xE4000220UL, 0xE3000330UL, 0xE2000388UL, 0, 0, 0},
        {0, 0, 0, 0},
        {0x80000001UL, 0, 0x80000005UL, 0}, 
        0x8C000002UL,
        0x00000000UL
    },
    
    // Floppy Drive Controller  
    {
        "FDC", "Floppy Drive Controller",
        {0xE30003F0UL, 0, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0x80000002UL, 0, 0}, 
        0x8C000002UL,
        0x00000000UL
    },
    
    // Sound Blaster Compatible
    {
        "SB", "Sound Blaster Compatible", 
        {0xE4000220UL, 0xE2000388UL, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0x80000001UL, 0x80000005UL, 0, 0}, 
        0x8C000002UL,
        0x00000000UL
    },
    
    // NE2000 Ethernet
    {
        "NE2000", "NE2000 Compatible Ethernet Adapter",
        {0xE5000300UL, 0, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        0x8C000002UL,
        0x00000000UL
    },
    
    // SCSI Host Adapter
    {
        "SCSI", "SCSI Host Adapter",
        {0xE4000330UL, 0xE4000140UL, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0x80000005UL, 0}, 
        0x8C000002UL,
        0x00000000UL
    },
    
    // Multi-Serial Controller
    {
        "SERIAL", "Multi-Port Serial Controller",
        {0xE30003F8UL, 0xE30002F8UL, 0xE30003E8UL, 0xE30002E8UL, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        0x8C000000UL,
        0x00000000UL
    }
};

#define NUM_PRESETS (sizeof(presets) / sizeof(presets[0]))

// Function prototypes
unsigned long pci_read_config_dword(int bus, int dev, int func, int reg);
void pci_write_config_dword(int bus, int dev, int func, int reg, unsigned long val);
int find_ite8888(void);
int configure_bridge(CARD_CONFIG *config);
int reset_bridge(void);
int scan_bridge(void);
int load_preset(char *preset_name);
int save_config_file(char *filename, CARD_CONFIG *config);
int load_config_file(char *filename, CARD_CONFIG *config);
void show_usage(void);
int parse_hex(char *str, unsigned long *value);

// PCI configuration access functions using atomic 32-bit operations
unsigned long pci_read_config_dword(int bus, int dev, int func, int reg) {
    unsigned long addr = 0x80000000UL | 
                        ((unsigned long)bus << 16) | 
                        ((unsigned long)dev << 11) | 
                        ((unsigned long)func << 8) | 
                        (reg & 0xFC);
    unsigned long result;
    
    outpd(PCI_CONFIG_ADDRESS, addr);
    result = inpd(PCI_CONFIG_DATA);
    outpd(PCI_CONFIG_ADDRESS, 0);
    
    return result;
}

void pci_write_config_dword(int bus, int dev, int func, int reg, unsigned long val) {
    unsigned long addr = 0x80000000UL | 
                        ((unsigned long)bus << 16) | 
                        ((unsigned long)dev << 11) | 
                        ((unsigned long)func << 8) | 
                        (reg & 0xFC);
    
    outpd(PCI_CONFIG_ADDRESS, addr);
    outpd(PCI_CONFIG_DATA, val);
    outpd(PCI_CONFIG_ADDRESS, 0);
}

// Find ITE8888 bridge
int find_ite8888(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for ITE8888 bridge...\n");
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                unsigned long id = pci_read_config_dword(bus, dev, func, 0);
                
                if ((id & 0xFFFF) == ITE_VENDOR_ID && (id >> 16) == ITE8888_DEVICE_ID) {
                    bridge_bus = bus;
                    bridge_dev = dev;
                    bridge_func = func;
                    printf("Found ITE8888 at Bus %d, Device %d, Function %d\n", 
                           bus, dev, func);
                    return 1;
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
        
        if ((bus & 0x3F) == 0 && bus > 0) {
            printf("  Scanning bus %d...\n", bus);
        }
    }
    
    printf("ERROR: ITE8888 bridge not found!\n");
    return 0;
}

// Configure bridge with given configuration
int configure_bridge(CARD_CONFIG *config) {
    int i;
    unsigned long cmd_reg;
    unsigned long io_regs[6] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    unsigned long mem_regs[4] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    unsigned long dma_regs[4] = {DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    printf("\nConfiguring ITE8888 for: %s\n", config->description);
    
    // Enable I/O space, memory space, and bus mastering
    cmd_reg = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    cmd_reg |= 0x07; 
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG, cmd_reg);
    printf("  PCI Command: 0x%04lX\n", cmd_reg & 0xFFFF);
    
    // Configure miscellaneous control register
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL, config->misc_control);
    printf("  Misc Control: 0x%08lX\n", config->misc_control);
    
    // Configure ISA control register if needed
    if (config->isa_control) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, ISA_CONTROL, config->isa_control);
        printf("  ISA Control: 0x%08lX\n", config->isa_control);
    }
    
    // Configure I/O spaces
    for (i = 0; i < 6; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, io_regs[i], config->io_spaces[i]);
        if (config->io_spaces[i] & 0x80000000UL) {
            unsigned long base = config->io_spaces[i] & 0xFFFF;
            unsigned long size = (config->io_spaces[i] >> 24) & 0x07;
            unsigned long bytes = 1UL << size;
            printf("  I/O Space %d: 0x%04lX-0x%04lX (%lu bytes)\n", 
                   i, base, base + bytes - 1, bytes);
        }
    }
    
    // Configure memory spaces
    for (i = 0; i < 4; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, mem_regs[i], config->mem_spaces[i]);
        if (config->mem_spaces[i] & 0x80000000UL) {
            unsigned long base = (config->mem_spaces[i] & 0xFFFFFF) << 8;
            unsigned long size = (config->mem_spaces[i] >> 24) & 0x07;
            unsigned long kb = 16UL << size;
            printf("  Memory Space %d: 0x%08lX (%luKB)\n", i, base, kb);
        }
    }
    
    // Configure DMA channels
    for (i = 0; i < 4; i++) {
        if (config->dma_config[i]) {
            pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, dma_regs[i], config->dma_config[i]);
            printf("  DMA Channel: Enabled (0x%08lX)\n", config->dma_config[i]);
        }
    }
    
    printf("Configuration completed successfully!\n");
    return 1;
}

// Reset bridge to defaults
int reset_bridge(void) {
    int i;
    unsigned long regs[14] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5,
                             MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3,
                             DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    printf("Resetting ITE8888 bridge to defaults...\n");
    
    for (i = 0; i < 14; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, regs[i], 0);
    }
    
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL, 0);
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, ISA_CONTROL, 0);
    
    printf("Bridge reset completed!\n");
    return 1;
}

// Scan and display current configuration
int scan_bridge(void) {
    int i;
    unsigned long value;
    unsigned long io_regs[6] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    unsigned long mem_regs[4] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    printf("\nCurrent ITE8888 Configuration:\n");
    printf("==============================\n");
    printf("Location: Bus %d, Device %d, Function %d\n", bridge_bus, bridge_dev, bridge_func);
    
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, 0);
    printf("Device/Vendor ID: %04lX:%04lX\n", (value >> 16) & 0xFFFF, value & 0xFFFF);
    
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    printf("Command: 0x%04lX", value & 0xFFFF);
    if (value & 0x01) printf(" [I/O]");
    if (value & 0x02) printf(" [Mem]");
    if (value & 0x04) printf(" [Master]");
    printf("\n");
    
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL);
    printf("Misc Control: 0x%08lX", value);
    if (value & 0x80000000UL) printf(" [DDMA-Concurrent]");
    if (value & 0x08000000UL) printf(" [PCI-Clk]");
    if (value & 0x04000000UL) printf(" [ISA-Refresh]");
    if (value & 0x00000001UL) printf(" [Subtractive]");
    if (value & 0x00000002UL) printf(" [Delayed-Tx]");
    printf("\n");
    
    printf("\nI/O Spaces:\n");
    for (i = 0; i < 6; i++) {
        value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, io_regs[i]);
        if (value & 0x80000000UL) {
            unsigned long base = value & 0xFFFF;
            unsigned long size = (value >> 24) & 0x07;
            unsigned long bytes = 1UL << size;
            printf("  Space %d: 0x%04lX-0x%04lX (%lu bytes)\n", 
                   i, base, base + bytes - 1, bytes);
        }
    }
    
    printf("\nMemory Spaces:\n");
    for (i = 0; i < 4; i++) {
        value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, mem_regs[i]);
        if (value & 0x80000000UL) {
            unsigned long base = (value & 0xFFFFFF) << 8;
            unsigned long size = (value >> 24) & 0x07;
            unsigned long kb = 16UL << size;
            printf("  Space %d: 0x%08lX (%luKB)\n", i, base, kb);
        }
    }
    
    printf("\n");
    return 1;
}

// Load preset configuration
int load_preset(char *preset_name) {
    int i;
    
    for (i = 0; i < NUM_PRESETS; i++) {
        if (strcmpi(presets[i].name, preset_name) == 0) {
            return configure_bridge(&presets[i]);
        }
    }
    
    printf("ERROR: Preset '%s' not found!\n", preset_name);
    return 0;
}

// Save configuration to file
int save_config_file(char *filename, CARD_CONFIG *config) {
    FILE *f;
    int i;
    
    f = fopen(filename, "w");
    if (!f) {
        printf("ERROR: Cannot create file '%s'\n", filename);
        return 0;
    }
    
    fprintf(f, "; ITE8888 Configuration File\n");
    fprintf(f, "[INFO]\n");
    fprintf(f, "NAME=%s\n", config->name);
    fprintf(f, "DESCRIPTION=%s\n", config->description);
    fprintf(f, "\n[IO_SPACES]\n");
    
    for (i = 0; i < 6; i++) {
        fprintf(f, "IO%d=0x%08lX\n", i, config->io_spaces[i]);
    }
    
    fprintf(f, "\n[MEMORY_SPACES]\n");
    for (i = 0; i < 4; i++) {
        fprintf(f, "MEM%d=0x%08lX\n", i, config->mem_spaces[i]);
    }
    
    fprintf(f, "\n[DMA]\n");
    for (i = 0; i < 4; i++) {
        fprintf(f, "DMA%d=0x%08lX\n", i, config->dma_config[i]);
    }
    
    fprintf(f, "\n[CONTROL]\n");
    fprintf(f, "MISC_CONTROL=0x%08lX\n", config->misc_control);
    fprintf(f, "ISA_CONTROL=0x%08lX\n", config->isa_control);
    
    fclose(f);
    printf("Configuration saved to '%s'\n", filename);
    return 1;
}

// Load configuration from file
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
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Skip comments, section headers, and empty lines
        if (line[0] == ';' || line[0] == '[' || line[0] == 0) continue;
        
        // Parse key=value
        key = strtok(line, "=");
        value = strtok(NULL, "=");
        if (!key || !value) continue;
        
        // Handle NAME
        if (strcmp(key, "NAME") == 0) {
            strncpy(config->name, value, sizeof(config->name) - 1);
        }
        // Handle DESCRIPTION
        else if (strcmp(key, "DESCRIPTION") == 0) {
            strncpy(config->description, value, sizeof(config->description) - 1);
        }
        // Handle IO spaces (IO0-IO5)
        else if (strncmp(key, "IO", 2) == 0 && strlen(key) == 3) {
            int idx = key[2] - '0';
            if (idx >= 0 && idx < 6) {
                parse_hex(value, &config->io_spaces[idx]);
            }
        }
        // Handle memory spaces (MEM0-MEM3)
        else if (strncmp(key, "MEM", 3) == 0 && strlen(key) == 4) {
            int idx = key[3] - '0';
            if (idx >= 0 && idx < 4) {
                parse_hex(value, &config->mem_spaces[idx]);
            }
        }
        // Handle DMA channels (DMA0-DMA3)
        else if (strncmp(key, "DMA", 3) == 0 && strlen(key) == 4) {
            int idx = key[3] - '0';
            if (idx >= 0 && idx < 4) {
                parse_hex(value, &config->dma_config[idx]);
            }
        }
        // Handle MISC_CONTROL
        else if (strcmp(key, "MISC_CONTROL") == 0) {
            parse_hex(value, &config->misc_control);
        }
        // Handle ISA_CONTROL
        else if (strcmp(key, "ISA_CONTROL") == 0) {
            parse_hex(value, &config->isa_control);
        }
    }
    
    fclose(f);
    printf("Configuration loaded from '%s'\n", filename);
    return 1;
}

// Parse hex string to unsigned long
int parse_hex(char *str, unsigned long *value) {
    char *endptr;
    
    // Skip "0x" or "0X" prefix if present
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        str += 2;
    }
    
    *value = strtoul(str, &endptr, 16);
    return (*endptr == 0);
}

void show_usage(void) {
    int i;
    
    printf("\nITE8888 Configuration Utility v1.1\n");
    printf("Usage:\n");
    printf("  ITE8888CFG -preset <name>   Load preset configuration\n");
    printf("  ITE8888CFG -load <file>     Load configuration from file\n");
    printf("  ITE8888CFG -reset           Reset bridge to defaults\n");
    printf("  ITE8888CFG -scan            Display current configuration\n");
    printf("\nAvailable presets: ");
    for (i = 0; i < NUM_PRESETS; i++) {
        printf("%s", presets[i].name);
        if (i < NUM_PRESETS - 1) printf(", ");
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    unsigned long host_id;
    CARD_CONFIG config;
    
    printf("ITE8888 Configuration Utility v1.1\n");
    printf("==================================\n\n");
    
    // Test PCI access with host bridge
    printf("Testing PCI configuration access...\n");
    host_id = pci_read_config_dword(0, 0, 0, 0);
    printf("  Host Bridge ID: 0x%08lX\n", host_id);
    
    if (host_id == 0xFFFFFFFFUL || host_id == 0x00000000UL) {
        printf("ERROR: Cannot access PCI configuration space!\n");
        return 1;
    }
    printf("  PCI access OK\n\n");
    
    // Find ITE8888
    if (!find_ite8888()) {
        printf("\nITE8888 bridge not found!\n");
        printf("Possible causes:\n");
        printf("  - Card not installed properly\n");
        printf("  - Hardware configuration issue\n");
        printf("  - Incompatible system\n");
        return 1;
    }
    
    printf("\n");
    
    // Process command line
    if (argc < 2) {
        show_usage();
        return 0;
    }
    
    if (strcmp(argv[1], "-preset") == 0 && argc >= 3) {
        return load_preset(argv[2]) ? 0 : 1;
    }
    else if (strcmp(argv[1], "-load") == 0 && argc >= 3) {
        if (load_config_file(argv[2], &config)) {
            return configure_bridge(&config) ? 0 : 1;
        }
        return 1;
    }
    else if (strcmp(argv[1], "-reset") == 0) {
        return reset_bridge() ? 0 : 1;
    }
    else if (strcmp(argv[1], "-scan") == 0) {
        return scan_bridge() ? 0 : 1;
    }
    else {
        show_usage();
        return 1;
    }
}
