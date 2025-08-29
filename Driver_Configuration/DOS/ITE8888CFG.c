/*
 * IT8888CFG.C - IT8888F Universal ISA Bridge Configuration Utility v1.0
 * Compiles with OpenWatcom C
 * Usage: IT8888CFG [options]
 */

#if defined(__WATCOMC__)
    #include <i86.h>
    #include <conio.h>
    // Watcom has inp/outp and inpd/outpd built-in
#elif defined(__TURBOC__) || defined(__BORLANDC__)
    #include <conio.h>
    #include <dos.h>
    // Borland/Turbo C have inp/outp but need 32-bit versions defined
    #define inpd(port) (inp(port) | (inp(port+1)<<8) | (inp(port+2)<<16) | (inp(port+3)<<24))
    #define outpd(port,val) do { \
        outp(port, (val) & 0xFF); \
        outp(port+1, ((val)>>8) & 0xFF); \
        outp(port+2, ((val)>>16) & 0xFF); \
        outp(port+3, ((val)>>24) & 0xFF); \
    } while(0)
#else
    #error "Unsupported compiler"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Compiler compatibility for string functions
#if defined(__TURBOC__) || defined(__BORLANDC__)
    #define strcmpi stricmp  // Borland uses stricmp
#endif

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
        {0xE4000220, 0xE3000330, 0xE2000388, 0, 0, 0},
        {0, 0, 0, 0},
        {0x80000001, 0, 0x80000005, 0}, // DMA 1, 5
        0x8C000003, // DDMA + subtractive + delayed
        0x00000000
    },
    
    // Floppy Drive Controller
    {
        "FDC", "Floppy Drive Controller",
        {0xE20003F0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0x80000002, 0, 0}, // DMA 2
        0x8C000003,
        0x00000000
    },
    
    // Sound Blaster Compatible
    {
        "SB", "Sound Blaster Compatible",
        {0xE4000220, 0xE2000388, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0x80000001, 0x80000005, 0, 0}, // DMA 1, 5
        0x8C000003,
        0x00000000
    },
    
    // NE2000 Ethernet
    {
        "NE2000", "NE2000 Compatible Ethernet Adapter",
        {0xE5000300, 0, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        0x8C000003,
        0x00000000
    },
    
    // SCSI Host Adapter
    {
        "SCSI", "SCSI Host Adapter",
        {0xE4000330, 0xE4000140, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0x80000005, 0}, // DMA 5
        0x8C000003,
        0x00000000
    },
    
    // Multi-Serial Controller
    {
        "SERIAL", "Multi-Port Serial Controller",
        {0xE20003F8, 0xE20002F8, 0xE20003E8, 0xE20002E8, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        0x8C000001, // Subtractive only
        0x00000000
    }
};

#define NUM_PRESETS (sizeof(presets) / sizeof(presets[0]))

// Function prototypes
unsigned long pci_read_config_dword(int bus, int dev, int func, int reg);
void pci_write_config_dword(int bus, int dev, int func, int reg, unsigned long val);
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
void decode_io_space(unsigned long reg_val, char *buffer);
void decode_mem_space(unsigned long reg_val, char *buffer);
void wait_key(void);
char *trim(char *str);
int parse_hex(char *str, unsigned long *value);

// PCI configuration access functions
unsigned long pci_read_config_dword(int bus, int dev, int func, int reg) {
    unsigned long addr = 0x80000000UL | 
                        ((unsigned long)bus << 16) | 
                        ((unsigned long)dev << 11) | 
                        ((unsigned long)func << 8) | 
                        (reg & 0xFC);
    outpd(PCI_CONFIG_ADDRESS, addr);
    return inpd(PCI_CONFIG_DATA);
}

void pci_write_config_dword(int bus, int dev, int func, int reg, unsigned long val) {
    unsigned long addr = 0x80000000UL | 
                        ((unsigned long)bus << 16) | 
                        ((unsigned long)dev << 11) | 
                        ((unsigned long)func << 8) | 
                        (reg & 0xFC);
    outpd(PCI_CONFIG_ADDRESS, addr);
    outpd(PCI_CONFIG_DATA, val);
}

// Find IT8888F bridge
int find_it8888f(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for IT8888F bridge...\n");
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                unsigned long id = pci_read_config_dword(bus, dev, func, 0);
                
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
    unsigned long cmd_reg;
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    printf("Configuring IT8888F for: %s\n", config->description);
    
    // Enable I/O space, memory space, and bus mastering
    cmd_reg = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    cmd_reg |= 0x07; // I/O + Memory + Bus Master
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG, cmd_reg);
    
    // Configure miscellaneous control register
    pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL, config->misc_control);
    
    // Configure ISA control register if needed
    if (config->isa_control) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, ISA_CONTROL, config->isa_control);
    }
    
    // Configure I/O spaces
    unsigned long io_regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    for (i = 0; i < 6; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, io_regs[i], config->io_spaces[i]);
        if (config->io_spaces[i] & 0x80000000) {
            char decode_buf[80];
            decode_io_space(config->io_spaces[i], decode_buf);
            printf("  I/O Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Configure memory spaces
    unsigned long mem_regs[] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    for (i = 0; i < 4; i++) {
        pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, mem_regs[i], config->mem_spaces[i]);
        if (config->mem_spaces[i] & 0x80000000) {
            char decode_buf[80];
            decode_mem_space(config->mem_spaces[i], decode_buf);
            printf("  Memory Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Configure DMA channels
    unsigned long dma_regs[] = {DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    for (i = 0; i < 4; i++) {
        if (config->dma_config[i]) {
            pci_write_config_dword(bridge_bus, bridge_dev, bridge_func, dma_regs[i], config->dma_config[i]);
            printf("  DMA Channel %d: Enabled (0x%08lX)\n", i, config->dma_config[i]);
        }
    }
    
    printf("Configuration completed successfully!\n");
    return 1;
}

// Reset bridge to defaults
int reset_bridge(void) {
    int i;
    unsigned long regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5,
                           MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3,
                           DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
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
    unsigned long value;
    char decode_buf[80];
    
    if (bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    printf("\nCurrent IT8888F Configuration:\n");
    printf("==============================\n");
    
    // Show command register
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, CMD_REG);
    printf("Command Register: 0x%04lX", value & 0xFFFF);
    if (value & 0x01) printf(" [I/O Enabled]");
    if (value & 0x02) printf(" [Memory Enabled]");
    if (value & 0x04) printf(" [Bus Master]");
    printf("\n");
    
    // Show control registers
    value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, MISC_CONTROL);
    printf("Misc Control: 0x%08lX", value);
    if (value & 0x80000000) printf(" [DDMA-Concurrent]");
    if (value & 0x08000000) printf(" [PCI-Clock]");
    if (value & 0x04000000) printf(" [ISA-Refresh]");
    if (value & 0x00000001) printf(" [Subtractive]");
    if (value & 0x00000002) printf(" [Delayed-Tx]");
    printf("\n");
    
    // Show I/O spaces
    printf("\nI/O Spaces:\n");
    unsigned long io_regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    for (i = 0; i < 6; i++) {
        value = pci_read_config_dword(bridge_bus, bridge_dev, bridge_func, io_regs[i]);
        if (value & 0x80000000) {
            decode_io_space(value, decode_buf);
            printf("  Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Show memory spaces
    printf("\nMemory Spaces:\n");
    unsigned long mem_regs[] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
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
    unsigned long base, size, speed;
    char buffer[80];
    
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
        if (getch() == 'y') {
            printf("y\n");
            printf("  Base address (hex, no 0x): ");
            fgets(buffer, sizeof(buffer), stdin);
            if (parse_hex(trim(buffer), &base)) {
                printf("  Size (0=1B, 1=2B, 2=4B, 3=8B, 4=16B, 5=32B, 6=64B, 7=128B): ");
                scanf("%lu", &size);
                printf("  Speed (0=Subtractive, 1=Slow, 2=Medium, 3=Fast): ");
                scanf("%lu", &speed);
                
                config.io_spaces[i] = 0x80000000 | (speed << 29) | (size << 24) | (base & 0xFFFF);
                printf("  Configured: 0x%04lX-%04lX\n", base, base + (1 << size) - 1);
            }
        } else {
            printf("n\n");
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
    config.misc_control = 0x8C000000; // Default: DDMA + PCI Clock + ISA Refresh
    printf("\nEnable subtractive decode? (y/n): ");
    if (getch() == 'y') {
        printf("y\n");
        config.misc_control |= 0x01;
    } else {
        printf("n\n");
    }
    
    printf("Enable delayed transaction? (y/n): ");
    if (getch() == 'y') {
        printf("y\n");
        config.misc_control |= 0x02;
    } else {
        printf("n\n");
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
    if (getch() == 'y') {
        printf("y\n");
        configure_bridge(&config);
        
        printf("Save configuration to file? (y/n): ");
        if (getch() == 'y') {
            printf("y\n");
            printf("Filename: ");
            fgets(buffer, sizeof(buffer), stdin);
            save_config_file(trim(buffer), &config);
        } else {
            printf("n\n");
        }
    } else {
        printf("n\n");
    }
    
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
    
    printf("\nIT8888F Universal Configuration Utility\n");
    printf("========================================\n");
    printf("Preset Configurations:\n");
    for (i = 0; i < NUM_PRESETS; i++) {
        printf("  %d. %-8s - %s\n", i + 1, presets[i].name, presets[i].description);
    }
    printf("\nOther Options:\n");
    printf("  M. Manual configuration\n");
    printf("  L. Load configuration file\n");
    printf("  S. Scan current configuration\n");
    printf("  R. Reset bridge (disable all)\n");
    printf("  Q. Quit\n");
    printf("\nChoice: ");
}

// Interactive mode
int interactive_mode(void) {
    char choice;
    char filename[80];
    CARD_CONFIG config;
    
    while (1) {
        show_main_menu();
        choice = getch();
        printf("%c\n", choice);
        
        if (choice >= '1' && choice <= '0' + NUM_PRESETS) {
            configure_bridge(&presets[choice - '1']);
            wait_key();
        }
        else if (toupper(choice) == 'M') {
            manual_config_mode();
            wait_key();
        }
        else if (toupper(choice) == 'L') {
            printf("Configuration filename: ");
            fgets(filename, sizeof(filename), stdin);
            if (load_config_file(trim(filename), &config)) {
                configure_bridge(&config);
            }
            wait_key();
        }
        else if (toupper(choice) == 'S') {
            scan_bridge();
            wait_key();
        }
        else if (toupper(choice) == 'R') {
            reset_bridge();
            wait_key();
        }
        else if (toupper(choice) == 'Q') {
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

// Utility functions
void decode_io_space(unsigned long reg_val, char *buffer) {
    unsigned long base = reg_val & 0xFFFF;
    unsigned long size = (reg_val >> 24) & 0x07;
    unsigned long speed = (reg_val >> 29) & 0x03;
    unsigned long bytes = 1UL << size;
    char *speed_names[] = {"Sub", "Slow", "Med", "Fast"};
    
    sprintf(buffer, "0x%04lX-0x%04lX (%lu bytes, %s)", 
            base, base + bytes - 1, bytes, speed_names[speed]);
}

void decode_mem_space(unsigned long reg_val, char *buffer) {
    unsigned long base = (reg_val & 0xFFFFFF) << 8;
    unsigned long size = (reg_val >> 24) & 0x07;
    unsigned long speed = (reg_val >> 29) & 0x03;
    unsigned long kb = 16UL << size;
    char *speed_names[] = {"Sub", "Slow", "Med", "Fast"};
    
    sprintf(buffer, "0x%08lX (%luKB, %s)", base, kb, speed_names[speed]);
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

int parse_hex(char *str, unsigned long *value) {
    char *endptr;
    
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        str += 2;
    }
    
    *value = strtoul(str, &endptr, 16);
    return (*endptr == 0);
}

void wait_key(void) {
    printf("\nPress any key to continue...");
    getch();
    printf("\n");
}

void show_usage(void) {
    printf("IT8888F Universal Configuration Utility v1.0\n");
    printf("Usage:\n");
    printf("  IT8888CFG                    - Interactive mode\n");
    printf("  IT8888CFG -preset <name>     - Load preset (GUS, FDC, SB, etc.)\n");
    printf("  IT8888CFG -load <file.cfg>   - Load configuration file\n");
    printf("  IT8888CFG -reset             - Reset bridge to defaults\n");
    printf("  IT8888CFG -scan              - Scan and display current config\n");
    printf("  IT8888CFG -help              - Show this help\n");
    printf("\nAvailable presets: ");
    for (int i = 0; i < NUM_PRESETS; i++) {
        printf("%s", presets[i].name);
        if (i < NUM_PRESETS - 1) printf(", ");
    }
    printf("\n");
}

// Main function
int main(int argc, char *argv[]) {
    printf("IT8888F Universal ISA Bridge Configuration Utility v1.0\n");
    printf("=========================================================\n\n");
    
    // Find IT8888F bridge first
    if (!find_it8888f()) {
        printf("Make sure the IT8888F bridge is properly installed.\n");
        return 1;
    }
    
    // Process command line arguments
    if (argc == 1) {
        return interactive_mode();
    }
    else if (argc >= 2) {
        if (strcmp(argv[1], "-preset") == 0 && argc >= 3) {
            return load_preset(argv[2]) ? 0 : 1;
        }
        else if (strcmp(argv[1], "-load") == 0 && argc >= 3) {
            CARD_CONFIG config;
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
        else if (strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "/?") == 0) {
            show_usage();
            return 0;
        }
        else {
            printf("Invalid option: %s\n", argv[1]);
            show_usage();
            return 1;
        }
    }
    
    return 0;
}