/*
 * ITE8888_CORE.C - Shared implementation for ITE8888 configuration utilities
 * Platform-independent core functionality
 */

#include "ite8888_core.h"

// Preset configurations
CARD_CONFIG presets[] = {
    // Gravis UltraSound
    {
        "GUS", "Gravis UltraSound Audio Card",
        {0xE4000220UL, 0xE3000330UL, 0xE2000388UL, 0, 0, 0},
        {0, 0, 0, 0},
        {0x80000001UL, 0, 0x80000005UL, 0}, 
        0x8C000002UL,  // Removed subtractive decode bit for positive decode
        0x00000000UL
    },
    
    // Floppy Drive Controller  
    {
        "FDC", "Floppy Drive Controller",
        {0xE30003F0UL, 0, 0, 0, 0, 0},  // Fixed: 8 bytes for FDC
        {0, 0, 0, 0},
        {0, 0x80000002UL, 0, 0}, 
        0x8C000002UL,  // Removed subtractive decode bit
        0x00000000UL
    },
    
    // Sound Blaster Compatible
    {
        "SB", "Sound Blaster Compatible", 
        {0xE4000220UL, 0xE2000388UL, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0x80000001UL, 0x80000005UL, 0, 0}, 
        0x8C000002UL,  // Removed subtractive decode bit
        0x00000000UL
    },
    
    // NE2000 Ethernet
    {
        "NE2000", "NE2000 Compatible Ethernet Adapter",
        {0xE5000300UL, 0, 0, 0, 0, 0},  // 32 bytes for NE2000
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        0x8C000002UL,  // Removed subtractive decode bit
        0x00000000UL
    },
    
    // SCSI Host Adapter
    {
        "SCSI", "SCSI Host Adapter",
        {0xE4000330UL, 0xE4000140UL, 0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 0x80000005UL, 0}, 
        0x8C000002UL,  // Removed subtractive decode bit
        0x00000000UL
    },
    
    // Multi-Serial Controller
    {
        "SERIAL", "Multi-Port Serial Controller",
        {0xE30003F8UL, 0xE30002F8UL, 0xE30003E8UL, 0xE30002E8UL, 0, 0},  // Fixed: 8 bytes each
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        0x8C000000UL,  // Removed both subtractive and delayed transaction bits
        0x00000000UL
    }
};

int num_presets = sizeof(presets) / sizeof(presets[0]);

// Configure bridge with given configuration
int configure_bridge(bridge_state_t* state, CARD_CONFIG* config) {
    int i;
    DWORD cmd_reg;
    DWORD io_regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    DWORD mem_regs[] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    DWORD dma_regs[] = {DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (state->bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!state->hw->hardware_available) {
        printf("ERROR: Cannot configure - hardware access not available!\n");
        return 0;
    }
    
    printf("Configuring ITE8888 for: %s\n", config->description);
    
    // Enable I/O space, memory space, and bus mastering
    cmd_reg = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, CMD_REG);
    cmd_reg |= 0x07; // Set bits 0, 1, 2 for I/O, Memory, Bus Master
    state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, CMD_REG, cmd_reg);
    printf("  PCI Command register: 0x%04X\n", cmd_reg & 0xFFFF);
    
    // Configure miscellaneous control register
    state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, MISC_CONTROL, config->misc_control);
    printf("  Misc Control: 0x%08X\n", (unsigned int)config->misc_control);
    
    // Configure ISA control register if needed
    if (config->isa_control) {
        state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, ISA_CONTROL, config->isa_control);
        printf("  ISA Control: 0x%08X\n", (unsigned int)config->isa_control);
    }
    
    // Configure I/O spaces
    for (i = 0; i < 6; i++) {
        state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, io_regs[i], config->io_spaces[i]);
        if (config->io_spaces[i] & 0x80000000UL) {
            char decode_buf[80];
            decode_io_space(config->io_spaces[i], decode_buf);
            printf("  I/O Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Configure memory spaces
    for (i = 0; i < 4; i++) {
        state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, mem_regs[i], config->mem_spaces[i]);
        if (config->mem_spaces[i] & 0x80000000UL) {
            char decode_buf[80];
            decode_mem_space(config->mem_spaces[i], decode_buf);
            printf("  Memory Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Configure DMA channels
    for (i = 0; i < 4; i++) {
        if (config->dma_config[i]) {
            state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, dma_regs[i], config->dma_config[i]);
            printf("  DMA Channel %d: Enabled (0x%08X)\n", i, (unsigned int)config->dma_config[i]);
        }
    }
    
    printf("Configuration completed successfully!\n");
    return 1;
}

// Reset bridge to defaults
int reset_bridge(bridge_state_t* state) {
    int i;
    DWORD regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5,
                   MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3,
                   DMA_CHANNEL_01, DMA_CHANNEL_23, DMA_CHANNEL_5, DMA_CHANNEL_67};
    
    if (state->bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!state->hw->hardware_available) {
        printf("ERROR: Cannot reset - hardware access not available!\n");
        return 0;
    }
    
    printf("Resetting ITE8888 bridge to defaults...\n");
    
    // Clear all configuration registers
    for (i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
        state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, regs[i], 0);
    }
    
    // Reset control registers
    state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, MISC_CONTROL, 0);
    state->hw->pci_write(state->bridge_bus, state->bridge_dev, state->bridge_func, ISA_CONTROL, 0);
    
    printf("Bridge reset completed!\n");
    return 1;
}

// Scan and display current configuration
int scan_bridge(bridge_state_t* state) {
    int i;
    DWORD value;
    char decode_buf[80];
    DWORD io_regs[] = {IO_SPACE_0, IO_SPACE_1, IO_SPACE_2, IO_SPACE_3, IO_SPACE_4, IO_SPACE_5};
    DWORD mem_regs[] = {MEM_SPACE_0, MEM_SPACE_1, MEM_SPACE_2, MEM_SPACE_3};
    
    if (state->bridge_bus < 0) {
        printf("ERROR: Bridge not found!\n");
        return 0;
    }
    
    if (!state->hw->hardware_available) {
        printf("ERROR: Cannot scan - hardware access not available!\n");
        return 0;
    }
    
    printf("\nCurrent ITE8888 Configuration:\n");
    printf("==============================\n");
    printf("Location: Bus %d, Device %d, Function %d\n", state->bridge_bus, state->bridge_dev, state->bridge_func);
    printf("Detection Method: %s\n", state->hw->method_name);
    
    // Show device ID
    value = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, 0);
    printf("Device ID: 0x%04X, Vendor ID: 0x%04X\n", (unsigned int)((value >> 16) & 0xFFFF), (unsigned int)(value & 0xFFFF));
    
    // Show command register
    value = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, CMD_REG);
    printf("Command Register: 0x%04X", (unsigned int)(value & 0xFFFF));
    if (value & 0x01) printf(" [I/O Enabled]");
    if (value & 0x02) printf(" [Memory Enabled]");
    if (value & 0x04) printf(" [Bus Master]");
    printf("\n");
    
    // Show control registers
    value = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, MISC_CONTROL);
    printf("Misc Control: 0x%08X", (unsigned int)value);
    if (value & 0x80000000UL) printf(" [DDMA-Concurrent]");
    if (value & 0x08000000UL) printf(" [PCI-Clock]");
    if (value & 0x04000000UL) printf(" [ISA-Refresh]");
    if (value & 0x00000001UL) printf(" [Subtractive]");
    if (value & 0x00000002UL) printf(" [Delayed-Tx]");
    printf("\n");
    
    value = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, ISA_CONTROL);
    if (value) {
        printf("ISA Control: 0x%08X\n", (unsigned int)value);
    }
    
    // Show I/O spaces
    printf("\nI/O Spaces:\n");
    for (i = 0; i < 6; i++) {
        value = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, io_regs[i]);
        if (value & 0x80000000UL) {
            decode_io_space(value, decode_buf);
            printf("  Space %d: %s\n", i, decode_buf);
        }
    }
    
    // Show memory spaces
    printf("\nMemory Spaces:\n");
    for (i = 0; i < 4; i++) {
        value = state->hw->pci_read(state->bridge_bus, state->bridge_dev, state->bridge_func, mem_regs[i]);
        if (value & 0x80000000UL) {
            decode_mem_space(value, decode_buf);
            printf("  Space %d: %s\n", i, decode_buf);
        }
    }
    
    printf("\n");
    return 1;
}

// Load preset configuration
int load_preset(bridge_state_t* state, char* preset_name) {
    int i;
    
    for (i = 0; i < num_presets; i++) {
#ifdef PLATFORM_DOS
        if (strcmpi(presets[i].name, preset_name) == 0) {
#else
        if (_stricmp(presets[i].name, preset_name) == 0) {
#endif
            return configure_bridge(state, &presets[i]);
        }
    }
    
    printf("ERROR: Preset '%s' not found!\n", preset_name);
    printf("Available presets: ");
    for (i = 0; i < num_presets; i++) {
        printf("%s", presets[i].name);
        if (i < num_presets - 1) printf(", ");
    }
    printf("\n");
    return 0;
}

// Show main menu
void show_main_menu(void) {
    int i;
    
    printf("\nITE8888 Universal Configuration Utility\n");
    printf("========================================\n");
    printf("Preset Configurations:\n");
    for (i = 0; i < num_presets; i++) {
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
int interactive_mode(bridge_state_t* state) {
    int choice;
    char filename[256];
    CARD_CONFIG config;
    
    while (1) {
        show_main_menu();
        choice = state->hw->get_key();
        printf("%c\n", choice);
        
        if (choice >= '1' && choice <= '0' + num_presets) {
            configure_bridge(state, &presets[choice - '1']);
            state->hw->wait_key();
        }
        else if (choice == 'M' || choice == 'm') {
            manual_config_mode(state);
            state->hw->wait_key();
        }
        else if (choice == 'L' || choice == 'l') {
            printf("Configuration filename: ");
            fgets(filename, sizeof(filename), stdin);
            if (load_config_file(trim(filename), &config)) {
                configure_bridge(state, &config);
            }
            state->hw->wait_key();
        }
        else if (choice == 'S' || choice == 's') {
            scan_bridge(state);
            state->hw->wait_key();
        }
        else if (choice == 'I' || choice == 'i') {
            show_system_info(state);
            state->hw->wait_key();
        }
        else if (choice == 'R' || choice == 'r') {
            reset_bridge(state);
            state->hw->wait_key();
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

// Manual configuration mode
int manual_config_mode(bridge_state_t* state) {
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
        choice = state->hw->get_key();
        printf("%c\n", choice);
        
        if (choice == 'y' || choice == 'Y') {
            printf("  Base address (hex, no 0x): ");
            scanf("%s", buffer);
            if (parse_hex(trim(buffer), &base)) {
                printf("  Size (0=1B, 1=2B, 2=4B, 3=8B, 4=16B, 5=32B, 6=64B, 7=128B): ");
                scanf("%d", (int*)&size);
                printf("  Speed (0=Subtractive, 1=Slow, 2=Medium, 3=Fast): ");
                scanf("%d", (int*)&speed);
                
                config.io_spaces[i] = 0x80000000UL | (speed << 29) | (size << 24) | (base & 0xFFFF);
                printf("  Configured: 0x%04X-0x%04X\n", (unsigned int)base, (unsigned int)(base + (1UL << size) - 1));
            }
        }
    }
    
    // Configure DMA
    printf("\nConfigure DMA channels:\n");
    printf("8-bit DMA channel (0-3, 255=none): ");
    scanf("%d", &choice);
    if (choice >= 0 && choice <= 3) {
        config.dma_config[0] = 0x80000000UL | choice;
    }
    
    printf("16-bit DMA channel (5-7, 255=none): ");
    scanf("%d", &choice);
    if (choice >= 5 && choice <= 7) {
        config.dma_config[2] = 0x80000000UL | choice;
    }
    
    // Configure control options
    config.misc_control = 0x8C000000UL;
    printf("\nEnable subtractive decode? (y/n): ");
    choice = state->hw->get_key();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.misc_control |= 0x01;
    }
    
    printf("Enable delayed transaction? (y/n): ");
    choice = state->hw->get_key();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        config.misc_control |= 0x02;
    }
    
    printf("\nConfiguration Summary:\n");
    printf("Name: %s\n", config.description);
    for (i = 0; i < 6; i++) {
        if (config.io_spaces[i] & 0x80000000UL) {
            char decode_buf[80];
            decode_io_space(config.io_spaces[i], decode_buf);
            printf("I/O %d: %s\n", i, decode_buf);
        }
    }
    
    printf("\nApply configuration? (y/n): ");
    choice = state->hw->get_key();
    printf("%c\n", choice);
    if (choice == 'y' || choice == 'Y') {
        configure_bridge(state, &config);
        
        printf("Save configuration to file? (y/n): ");
        choice = state->hw->get_key();
        printf("%c\n", choice);
        if (choice == 'y' || choice == 'Y') {
            printf("Filename: ");
            scanf("%s", buffer);
            save_config_file(trim(buffer), &config);
        }
    }
    
    return 1;
}

// Configuration file functions
int save_config_file(char* filename, CARD_CONFIG* config) {
    FILE* f;
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
        fprintf(f, "IO%d=0x%08X\n", i, (unsigned int)config->io_spaces[i]);
    }
    
    fprintf(f, "\n[MEMORY_SPACES]\n");
    for (i = 0; i < 4; i++) {
        fprintf(f, "MEM%d=0x%08X\n", i, (unsigned int)config->mem_spaces[i]);
    }
    
    fprintf(f, "\n[DMA]\n");
    for (i = 0; i < 4; i++) {
        fprintf(f, "DMA%d=0x%08X\n", i, (unsigned int)config->dma_config[i]);
    }
    
    fprintf(f, "\n[CONTROL]\n");
    fprintf(f, "MISC_CONTROL=0x%08X\n", (unsigned int)config->misc_control);
    fprintf(f, "ISA_CONTROL=0x%08X\n", (unsigned int)config->isa_control);
    
    fclose(f);
    printf("Configuration saved to '%s'\n", filename);
    return 1;
}

int load_config_file(char* filename, CARD_CONFIG* config) {
    FILE* f;
    char line[256];
    char* key;
    char* value;
    
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
void decode_io_space(DWORD reg_val, char* buffer) {
    DWORD base = reg_val & 0xFFFF;
    DWORD size = (reg_val >> 24) & 0x07;
    DWORD speed = (reg_val >> 29) & 0x03;
    DWORD bytes = 1UL << size;
    char* speed_names[] = {"Sub", "Slow", "Med", "Fast"};
    
    sprintf(buffer, "0x%04X-0x%04X (%lu bytes, %s)", 
            (unsigned int)base, (unsigned int)(base + bytes - 1), (unsigned long)bytes, speed_names[speed]);
}

void decode_mem_space(DWORD reg_val, char* buffer) {
    DWORD base = (reg_val & 0xFFFFFF) << 8;
    DWORD size = (reg_val >> 24) & 0x07;
    DWORD speed = (reg_val >> 29) & 0x03;
    DWORD kb = 16UL << size;
    char* speed_names[] = {"Sub", "Slow", "Med", "Fast"};
    
    sprintf(buffer, "0x%08X (%luKB, %s)", (unsigned int)base, (unsigned long)kb, speed_names[speed]);
}

char* trim(char* str) {
    char* end;
    
    while (isspace(*str)) str++;
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    *(end + 1) = 0;
    
    return str;
}

int parse_hex(char* str, DWORD* value) {
    char* endptr;
    
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        str += 2;
    }
    
    *value = strtoul(str, &endptr, 16);
    return (*endptr == 0);
}

void show_usage(const char* program_name) {
    int i;
    
    printf("ITE8888 Universal Configuration Utility v1.1\n");
    printf("Usage:\n");
    printf("  %s                    - Interactive mode\n", program_name);
    printf("  %s -preset <name>     - Load preset (GUS, FDC, SB, etc.)\n", program_name);
    printf("  %s -load <file.cfg>   - Load configuration file\n", program_name);
    printf("  %s -reset             - Reset bridge to defaults\n", program_name);
    printf("  %s -scan              - Scan and display current config\n", program_name);
    printf("  %s -info              - Show system information\n", program_name);
    printf("  %s -help              - Show this help\n", program_name);
    printf("\nAvailable presets: ");
    for (i = 0; i < num_presets; i++) {
        printf("%s", presets[i].name);
        if (i < num_presets - 1) printf(", ");
    }
    printf("\n");
}