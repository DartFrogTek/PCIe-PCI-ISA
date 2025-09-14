/*
 * ITE8888_DOS.C - DOS-specific shell for ITE8888 configuration utility
 * OpenWatcom C for DOS with proper 32-bit PCI access
 */

#include "ITE8888_CORE.H"

#if defined(__WATCOMC__)
    #include <conio.h>
    #include <i86.h>
    #include <dos.h>
    
    // OpenWatcom compatibility
    #define strcmpi stricmp
    
    // Proper atomic 32-bit PCI port access using inline assembly
    // This fixes the critical bug in the original code
    static void port_out_32(unsigned short port, unsigned long value) {
        _asm {
            mov dx, port
            mov eax, value
            out dx, eax
        }
    }
    
    static unsigned long port_in_32(unsigned short port) {
        unsigned long result;
        _asm {
            mov dx, port
            in eax, dx
            mov result, eax
        }
        return result;
    }
    
#else
    #error "This version requires OpenWatcom C for DOS"
#endif

// DOS-specific hardware interface implementation
static bridge_state_t dos_state = {-1, -1, -1, NULL, FALSE};
static int detection_method_used = 0;
static const char* method_names[] = {
    "Original Direct Method",
    "PCI BIOS INT 1A Services", 
    "DOS-Safe Byte Access Method",
    "Enhanced Direct Method"
};

// Function prototypes
static int dos_hw_init(void);
static void dos_hw_cleanup(void);
static DWORD dos_pci_read(int bus, int dev, int func, int reg);
static void dos_pci_write(int bus, int dev, int func, int reg, DWORD val);
static DWORD dos_pci_read_safe(int bus, int dev, int func, int reg);
static void dos_pci_write_safe(int bus, int dev, int func, int reg, DWORD val);
static DWORD dos_pci_read_bios(int bus, int dev, int func, int reg);
static int dos_find_bridge(void);
static int dos_get_key(void);
static void dos_wait_key(void);
static void dos_print_msg(const char* msg);

// Additional DOS-specific functions
static int test_pci_access_basic(void);
static int check_pci_bios(void);
static int find_ite8888_direct(void);
static int find_ite8888_bios(void);
static int find_ite8888_safe(void);
static int find_ite8888_enhanced(void);
static int test_detection_method(const char* method_name, int (*detection_func)(void), int method_id);

// DOS hardware interface
static hardware_interface_t dos_hardware = {
    dos_hw_init,
    dos_hw_cleanup,
    dos_pci_read,
    dos_pci_write,
    dos_find_bridge,
    dos_get_key,
    dos_wait_key,
    dos_print_msg,
    FALSE,
    0,
    "Unknown"
};

// PCI access function pointers (can be changed based on detection method)
static DWORD (*active_pci_read)(int, int, int, int) = dos_pci_read;
static void (*active_pci_write)(int, int, int, int, DWORD) = dos_pci_write;

// DOS hardware interface implementation
static int dos_hw_init(void) {
    int detection_success = 0;
    int pci_bios_available = 0;
    
    printf("ITE8888 DOS Configuration Utility v1.1\n");
    printf("=====================================\n\n");
    
    // Step 1: Check for PCI BIOS support
    pci_bios_available = check_pci_bios();
    printf("\n");
    
    // Step 2: Test basic PCI access
    if (!test_pci_access_basic()) {
        printf("\nFATAL ERROR: Cannot access PCI configuration space at all!\n");
        printf("This system may not support PCI or lacks proper PCI BIOS.\n");
        if (!pci_bios_available) {
            printf("Recommendation: Boot from a DOS with PCI support or try a PCI support driver.\n");
        }
        return 0;
    }
    printf("\n");
    
    // Step 3: Try PCI BIOS method first (most compatible)
    if (pci_bios_available) {
        if (test_detection_method("PCI BIOS INT 1A Services", find_ite8888_bios, 1)) {
            detection_success = 1;
            active_pci_read = dos_pci_read;  // BIOS found it, use direct access
            active_pci_write = dos_pci_write;
        }
    }
    
    // Step 4: Try DOS-safe byte-by-byte method
    if (!detection_success) {
        if (test_detection_method("DOS-Safe Byte Access Method", find_ite8888_safe, 2)) {
            detection_success = 1;
            active_pci_read = dos_pci_read_safe;
            active_pci_write = dos_pci_write_safe;
        }
    }
    
    // Step 5: Try enhanced version of original method
    if (!detection_success) {
        if (test_detection_method("Enhanced Direct Method", find_ite8888_enhanced, 3)) {
            detection_success = 1;
        }
    }
    
    // Step 6: Try original method as last resort
    if (!detection_success) {
        if (test_detection_method("Original Direct Method", find_ite8888_direct, 0)) {
            detection_success = 1;
        }
    }
    
    // Final result
    printf("\n==================================================\n");
    if (!detection_success) {
        printf("DETECTION FAILED: ITE8888 bridge not found with any method!\n\n");
        
        printf("Possible causes:\n");
        printf("1. ITE8888 not installed or not powered properly\n");
        printf("2. Hardware strapping pins (TC, AEN, BALE) incorrectly configured\n");
        printf("3. SMB boot configuration hanging (check TC pin - should be pulled down)\n");
        printf("4. DOS PCI BIOS limitations\n");
        printf("5. NOGO pin asserted (should be pulled high or floating)\n");
        
        if (!pci_bios_available) {
            printf("\nNOTE: No PCI BIOS detected. Try booting with:\n");
            printf("- A DOS version with PCI support\n");
            printf("- UMBPCI.SYS or similar PCI driver\n");
            printf("- FreeDOS with PCI support\n");
        }
        
        printf("\nIf Windows XP/10 can detect the bridge device, the hardware is likely OK.\n");
        printf("This is likely a DOS/BIOS compatibility issue.\n");
        
        return 0;
    }
    
    printf("DETECTION SUCCESS: ITE8888 found and ready for configuration!\n");
    printf("Bridge location: Bus %d, Device %d, Function %d\n", 
           dos_state.bridge_bus, dos_state.bridge_dev, dos_state.bridge_func);
    printf("Detection method: %s\n", method_names[detection_method_used]);
    
    dos_hardware.hardware_available = TRUE;
    dos_hardware.detection_method = detection_method_used;
    dos_hardware.method_name = method_names[detection_method_used];
    
    // Update function pointers in the hardware interface
    dos_hardware.pci_read = active_pci_read;
    dos_hardware.pci_write = active_pci_write;
    
    return 1;
}

static void dos_hw_cleanup(void) {
    // No cleanup needed for DOS
}

// Proper atomic 32-bit PCI configuration space access
static DWORD dos_pci_read(int bus, int dev, int func, int reg) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    port_out_32(PCI_CONFIG_ADDRESS, addr);
    return port_in_32(PCI_CONFIG_DATA);
}

static void dos_pci_write(int bus, int dev, int func, int reg, DWORD val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    port_out_32(PCI_CONFIG_ADDRESS, addr);
    port_out_32(PCI_CONFIG_DATA, val);
}

// DOS-safe byte-by-byte PCI access (fallback method)
static DWORD dos_pci_read_safe(int bus, int dev, int func, int reg) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    // Write address as separate bytes to avoid 32-bit issues
    outp(PCI_CONFIG_ADDRESS + 0, addr & 0xFF);
    outp(PCI_CONFIG_ADDRESS + 1, (addr >> 8) & 0xFF);
    outp(PCI_CONFIG_ADDRESS + 2, (addr >> 16) & 0xFF);
    outp(PCI_CONFIG_ADDRESS + 3, (addr >> 24) & 0xFF);
    
    // Read data as separate bytes
    return (DWORD)inp(PCI_CONFIG_DATA) |
           ((DWORD)inp(PCI_CONFIG_DATA + 1) << 8) |
           ((DWORD)inp(PCI_CONFIG_DATA + 2) << 16) |
           ((DWORD)inp(PCI_CONFIG_DATA + 3) << 24);
}

static void dos_pci_write_safe(int bus, int dev, int func, int reg, DWORD val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    // Write address as separate bytes
    outp(PCI_CONFIG_ADDRESS + 0, addr & 0xFF);
    outp(PCI_CONFIG_ADDRESS + 1, (addr >> 8) & 0xFF);
    outp(PCI_CONFIG_ADDRESS + 2, (addr >> 16) & 0xFF);
    outp(PCI_CONFIG_ADDRESS + 3, (addr >> 24) & 0xFF);
    
    // Write data as separate bytes
    outp(PCI_CONFIG_DATA + 0, val & 0xFF);
    outp(PCI_CONFIG_DATA + 1, (val >> 8) & 0xFF);
    outp(PCI_CONFIG_DATA + 2, (val >> 16) & 0xFF);
    outp(PCI_CONFIG_DATA + 3, (val >> 24) & 0xFF);
}

// PCI BIOS function approach
static DWORD dos_pci_read_bios(int bus, int dev, int func, int reg) {
    union REGS regs;
    
    regs.h.ah = 0xB1;        // PCI Function
    regs.h.al = 0x0A;        // Read Configuration Dword
    regs.h.bh = bus;
    regs.h.bl = (dev << 3) | func;
    regs.h.di = reg;
    
    int86(0x1A, &regs, &regs);
    
    if (regs.x.cflag == 0) {
        return regs.x.ecx;
    }
    
    return 0xFFFFFFFFUL;
}

static int dos_find_bridge(void) {
    // This function is called by the core, but actual detection
    // happens during initialization in dos_hw_init()
    return (dos_state.bridge_bus >= 0) ? 1 : 0;
}

static int dos_get_key(void) {
    return getch();
}

static void dos_wait_key(void) {
    printf("\nPress any key to continue...");
    getch();
    printf("\n");
}

static void dos_print_msg(const char* msg) {
    printf("%s", msg);
}

// Test basic PCI configuration access
static int test_pci_access_basic(void) {
    DWORD id;
    
    printf("Testing basic PCI configuration access...\n");
    
    // Try to read Host Bridge at 0:0:0 (should always exist)
    id = dos_pci_read(0, 0, 0, 0);
    printf("  Host Bridge ID (0:0:0): 0x%08lX\n", id);
    
    if (id == 0xFFFFFFFFUL || id == 0x00000000UL) {
        printf("  ERROR: PCI configuration access not working!\n");
        return 0;
    }
    
    printf("  PCI configuration access appears to work\n");
    return 1;
}

// Check for PCI BIOS support
static int check_pci_bios(void) {
    union REGS regs;
    
    printf("Checking for PCI BIOS support...\n");
    
    // INT 1A AH=B1h AL=01h - PCI BIOS Installation Check
    regs.h.ah = 0xB1;
    regs.h.al = 0x01;
    int86(0x1A, &regs, &regs);
    
    if (regs.h.ah == 0x00 && regs.x.edx == 0x20494350) { // 'PCI '
        printf("  PCI BIOS v%d.%d found\n", regs.h.bh, regs.h.bl);
        printf("  Hardware mechanism: %02Xh\n", regs.h.al & 0x03);
        return 1;
    }
    
    printf("  No PCI BIOS support detected (AH=%02X, EDX=%08lX)\n", regs.h.ah, regs.x.edx);
    return 0;
}

// Original direct scanning method
static int find_ite8888_direct(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for ITE8888 bridge...\n");
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                DWORD id = dos_pci_read(bus, dev, func, 0);
                
                if ((id & 0xFFFF) == ITE_VENDOR_ID && (id >> 16) == ITE8888_DEVICE_ID) {
                    dos_state.bridge_bus = bus;
                    dos_state.bridge_dev = dev;
                    dos_state.bridge_func = func;
                    printf("Found ITE8888 at Bus %d, Device %d, Function %d\n", 
                           bus, dev, func);
                    return 1;
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
    }
    
    printf("ERROR: ITE8888 bridge not found!\n");
    return 0;
}

// PCI BIOS-based device search
static int find_ite8888_bios(void) {
    union REGS regs;
    
    printf("Searching for ITE8888 via PCI BIOS services...\n");
    
    // Find PCI device using INT 1A AH=B1h AL=02h
    regs.h.ah = 0xB1;
    regs.h.al = 0x02;        // Find PCI Device
    regs.x.cx = ITE8888_DEVICE_ID;
    regs.x.dx = ITE_VENDOR_ID;
    regs.x.si = 0;           // Index (start with 0)
    
    int86(0x1A, &regs, &regs);
    
    if (regs.x.cflag == 0 && regs.h.ah == 0x00) {
        dos_state.bridge_bus = regs.h.bh;
        dos_state.bridge_dev = (regs.h.bl >> 3) & 0x1F;
        dos_state.bridge_func = regs.h.bl & 0x07;
        printf("  Found ITE8888 via PCI BIOS at Bus %d, Device %d, Function %d\n", 
               dos_state.bridge_bus, dos_state.bridge_dev, dos_state.bridge_func);
        return 1;
    }
    
    printf("  ITE8888 not found via PCI BIOS (AH=%02X, CF=%d)\n", regs.h.ah, regs.x.cflag);
    return 0;
}

// Safe method using byte-by-byte access
static int find_ite8888_safe(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses using DOS-safe method...\n");
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                DWORD id = dos_pci_read_safe(bus, dev, func, 0);
                
                if ((id & 0xFFFF) == ITE_VENDOR_ID && (id >> 16) == ITE8888_DEVICE_ID) {
                    dos_state.bridge_bus = bus;
                    dos_state.bridge_dev = dev;
                    dos_state.bridge_func = func;
                    printf("Found ITE8888 at Bus %d, Device %d, Function %d\n", 
                           bus, dev, func);
                    return 1;
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
        
        // Progress indicator for slow systems
        if ((bus & 0x3F) == 0) {
            printf("  Scanned bus %d...\n", bus);
        }
    }
    
    printf("ERROR: ITE8888 bridge not found!\n");
    return 0;
}

// Enhanced scanning with detailed debug info
static int find_ite8888_enhanced(void) {
    int bus, dev, func;
    DWORD id, class_code, status;
    int found_any_ite = 0;
    
    printf("Enhanced PCI scan for ITE8888...\n");
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                id = dos_pci_read(bus, dev, func, 0x00);
                
                if (id == 0xFFFFFFFFUL || id == 0x00000000UL) {
                    if (func == 0) break; // No device at this slot
                    continue;
                }
                
                // Check for any ITE device
                if ((id & 0xFFFF) == ITE_VENDOR_ID) {
                    found_any_ite = 1;
                    class_code = dos_pci_read(bus, dev, func, 0x08);
                    status = dos_pci_read(bus, dev, func, 0x04);
                    
                    printf("  Found ITE device at Bus %d, Device %d, Function %d\n", bus, dev, func);
                    printf("    Vendor:Device = %04lX:%04lX\n", id & 0xFFFF, id >> 16);
                    printf("    Class Code = %08lX\n", class_code);
                    printf("    Status/Command = %08lX\n", status);
                    
                    if ((id >> 16) == ITE8888_DEVICE_ID) {
                        printf("    *** THIS IS ITE8888 ***\n");
                        
                        // Check for SMB configuration in progress
                        DWORD misc_ctrl = dos_pci_read(bus, dev, func, 0x50);
                        if (misc_ctrl & 0x10) {
                            printf("    WARNING: SMB configuration in progress (Cfg_50h bit 4 set)\n");
                            printf("    This may indicate TC pin strapping issue\n");
                        }
                        
                        dos_state.bridge_bus = bus; 
                        dos_state.bridge_dev = dev; 
                        dos_state.bridge_func = func;
                        return 1;
                    }
                }
                
                if (func == 0 && id == 0xFFFFFFFFUL) break;
            }
        }
        
        // Progress indicator for slow systems
        if ((bus & 0x3F) == 0) {
            printf("  Scanned bus %d...\n", bus);
        }
    }
    
    if (found_any_ite) {
        printf("  Found ITE devices but no ITE8888\n");
    } else {
        printf("  No ITE devices found at all\n");
    }
    
    return 0;
}

// Test a specific detection method
static int test_detection_method(const char* method_name, int (*detection_func)(void), int method_id) {
    printf("\n=== Testing %s ===\n", method_name);
    
    dos_state.bridge_bus = dos_state.bridge_dev = dos_state.bridge_func = -1;
    
    if (detection_func()) {
        printf("SUCCESS: ITE8888 found using %s\n", method_name);
        detection_method_used = method_id;
        return 1;
    } else {
        printf("FAILED: ITE8888 not found using %s\n", method_name);
        return 0;
    }
}

// DOS-specific system info display
void show_system_info(bridge_state_t* state) {
    union REGS regs;
    
    printf("\nDOS System Information:\n");
    printf("======================\n");
    
    // Get DOS version
    regs.h.ah = 0x30;
    int86(0x21, &regs, &regs);
    printf("DOS Version: %d.%d\n", regs.h.al, regs.h.ah);
    
    // Get memory info
    regs.h.ah = 0x48;
    regs.x.bx = 0xFFFF;
    int86(0x21, &regs, &regs);
    printf("Available Memory: %d KB\n", regs.x.bx / 64);
    
    printf("Compiler: OpenWatcom C\n");
    printf("Hardware Access: Direct port I/O\n");
    
    if (state->bridge_bus >= 0) {
        printf("ITE8888 Bridge: Found at Bus %d, Device %d, Function %d\n",
               state->bridge_bus, state->bridge_dev, state->bridge_func);
        printf("Detection Method: %s\n", state->hw->method_name);
    } else {
        printf("ITE8888 Bridge: Not detected\n");
    }
    printf("\n");
}

// Get hardware interface for DOS
hardware_interface_t* get_hardware_interface(void) {
    return &dos_hardware;
}

// DOS main function
int main(int argc, char* argv[]) {
    bridge_state_t* state = &dos_state;
    
    // Initialize hardware interface
    state->hw = get_hardware_interface();
    
    // Initialize hardware access
    if (!state->hw->hw_init()) {
        printf("Failed to initialize hardware access\n");
        return 1;
    }
    
    // Process command line arguments
    if (argc == 1) {
        interactive_mode(state);
    }
    else if (argc >= 2) {
        if (strcmp(argv[1], "-preset") == 0 && argc >= 3) {
            if (load_preset(state, argv[2])) {
                state->hw->hw_cleanup();
                return 0;
            }
            state->hw->hw_cleanup();
            return 1;
        }
        else if (strcmp(argv[1], "-load") == 0 && argc >= 3) {
            CARD_CONFIG config;
            if (load_config_file(argv[2], &config)) {
                if (configure_bridge(state, &config)) {
                    state->hw->hw_cleanup();
                    return 0;
                }
            }
            state->hw->hw_cleanup();
            return 1;
        }
        else if (strcmp(argv[1], "-reset") == 0) {
            if (reset_bridge(state)) {
                state->hw->hw_cleanup();
                return 0;
            }
            state->hw->hw_cleanup();
            return 1;
        }
        else if (strcmp(argv[1], "-scan") == 0) {
            if (scan_bridge(state)) {
                state->hw->hw_cleanup();
                return 0;
            }
            state->hw->hw_cleanup();
            return 1;
        }
        else if (strcmp(argv[1], "-info") == 0) {
            show_system_info(state);
            state->hw->hw_cleanup();
            return 0;
        }
        else if (strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "/?") == 0) {
            show_usage("ITE8888CFG");
            state->hw->hw_cleanup();
            return 0;
        }
        else {
            printf("Invalid option: %s\n", argv[1]);
            show_usage("ITE8888CFG");
            state->hw->hw_cleanup();
            return 1;
        }
    }
    
    state->hw->hw_cleanup();
    return 0;
}