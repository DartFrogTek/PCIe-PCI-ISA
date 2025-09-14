/*
 * ITE8888_CORE.H - Shared definitions and interface for ITE8888 configuration utilities
 * Common header for both DOS and Windows versions
 */

#ifndef ITE8888_CORE_H
#define ITE8888_CORE_H

// Platform detection
#if defined(__WATCOMC__) || defined(__DOS__)
    #define PLATFORM_DOS
    typedef unsigned long DWORD;
    typedef unsigned short WORD;
    typedef unsigned char BYTE;
    #define BOOL int
    #define TRUE 1
    #define FALSE 0
#elif defined(_WIN32)
    #define PLATFORM_WINDOWS
    #include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
    char name[32];           // Short name identifier
    char description[80];    // Human-readable description
    DWORD io_spaces[6];      // 6 I/O space decode registers
    DWORD mem_spaces[4];     // 4 memory space decode registers  
    DWORD dma_config[4];     // DMA channel configurations
    DWORD misc_control;      // Miscellaneous control register
    DWORD isa_control;       // ISA-specific control register
} CARD_CONFIG;

// Hardware abstraction layer interface
typedef struct {
    // Initialization and cleanup
    int (*hw_init)(void);
    void (*hw_cleanup)(void);
    
    // PCI access methods
    DWORD (*pci_read)(int bus, int dev, int func, int reg);
    void (*pci_write)(int bus, int dev, int func, int reg, DWORD val);
    
    // Device detection
    int (*find_bridge)(void);
    
    // Console I/O abstraction
    int (*get_key)(void);
    void (*wait_key)(void);
    void (*print_msg)(const char* msg);
    
    // Status
    BOOL hardware_available;
    int detection_method;
    const char* method_name;
} hardware_interface_t;

// Global state
typedef struct {
    int bridge_bus;
    int bridge_dev; 
    int bridge_func;
    hardware_interface_t* hw;
    BOOL debug_mode;
} bridge_state_t;

// External preset configurations
extern CARD_CONFIG presets[];
extern int num_presets;

// Core functionality (implemented in ite8888_core.c)
int configure_bridge(bridge_state_t* state, CARD_CONFIG* config);
int reset_bridge(bridge_state_t* state);
int scan_bridge(bridge_state_t* state);
int load_preset(bridge_state_t* state, char* preset_name);
int interactive_mode(bridge_state_t* state);
int manual_config_mode(bridge_state_t* state);

// Configuration file functions
int save_config_file(char* filename, CARD_CONFIG* config);
int load_config_file(char* filename, CARD_CONFIG* config);

// Menu and UI functions
void show_main_menu(void);
void show_usage(const char* program_name);
void show_system_info(bridge_state_t* state);

// Utility functions
void decode_io_space(DWORD reg_val, char* buffer);
void decode_mem_space(DWORD reg_val, char* buffer);
char* trim(char* str);
int parse_hex(char* str, DWORD* value);

// Platform-specific implementations (must be provided by platform shells)
extern hardware_interface_t* get_hardware_interface(void);

#endif // ITE8888_CORE_H