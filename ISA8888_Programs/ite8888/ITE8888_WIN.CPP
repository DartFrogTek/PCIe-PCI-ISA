/*
 * ITE8888_WIN.CPP - Windows-specific shell for ITE8888 configuration utility
 * Visual Studio / Windows XP version with dynamic WinIO loading
 */

extern "C" {
    #include "ite8888_core.h"
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

// Windows-specific state
static bridge_state_t win_state = {-1, -1, -1, NULL, FALSE};
static OSVERSIONINFO os_version;

// Function prototypes
static int win_hw_init(void);
static void win_hw_cleanup(void);
static DWORD win_pci_read(int bus, int dev, int func, int reg);
static void win_pci_write(int bus, int dev, int func, int reg, DWORD val);
static int win_find_bridge(void);
static int win_get_key(void);
static void win_wait_key(void);
static void win_print_msg(const char* msg);

// Additional Windows-specific functions
static BOOL init_hardware_access(void);
static void cleanup_hardware_access(void);
static BOOL port_out_dword(WORD port, DWORD value);
static DWORD port_in_dword(WORD port);
static int find_ite8888(void);

// Windows hardware interface
static hardware_interface_t win_hardware = {
    win_hw_init,
    win_hw_cleanup,
    win_pci_read,
    win_pci_write,
    win_find_bridge,
    win_get_key,
    win_wait_key,
    win_print_msg,
    FALSE,
    0,
    "Windows Direct Access"
};

// Windows hardware interface implementation
static int win_hw_init(void) {
    printf("ITE8888 Windows Configuration Utility v1.1\n");
    printf("==========================================\n\n");
    
    if (!init_hardware_access()) {
        printf("Failed to initialize hardware access\n");
        return 0;
    }
    
    if (!find_ite8888()) {
        printf("ITE8888 bridge not found!\n");
        cleanup_hardware_access();
        return 0;
    }
    
    printf("ITE8888 found and ready for configuration!\n");
    printf("Bridge location: Bus %d, Device %d, Function %d\n", 
           win_state.bridge_bus, win_state.bridge_dev, win_state.bridge_func);
    
    win_hardware.hardware_available = TRUE;
    return 1;
}

static void win_hw_cleanup(void) {
    cleanup_hardware_access();
}

static DWORD win_pci_read(int bus, int dev, int func, int reg) {
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

static void win_pci_write(int bus, int dev, int func, int reg, DWORD val) {
    DWORD addr = 0x80000000UL | 
                ((DWORD)bus << 16) | 
                ((DWORD)dev << 11) | 
                ((DWORD)func << 8) | 
                (reg & 0xFC);
    
    port_out_dword(PCI_CONFIG_ADDRESS, addr);
    port_out_dword(PCI_CONFIG_DATA, val);
}

static int win_find_bridge(void) {
    return (win_state.bridge_bus >= 0) ? 1 : 0;
}

static int win_get_key(void) {
    return _getch();
}

static void win_wait_key(void) {
    printf("\nPress any key to continue...");
    _getch();
    printf("\n");
}

static void win_print_msg(const char* msg) {
    printf("%s", msg);
}

// Hardware access initialization with dynamic WinIO loading
static BOOL init_hardware_access(void) {
    os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os_version);
    
    printf("Initializing hardware access...\n");
    printf("Windows Version: %d.%d Build %d\n", 
           os_version.dwMajorVersion, os_version.dwMinorVersion, os_version.dwBuildNumber);
    
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        printf("Windows 95/98/ME - Direct hardware access enabled\n");
        return TRUE;
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
                    return TRUE;
                } else {
                    DWORD error = GetLastError();
                    printf("WinIo driver failed to initialize (Error: %d)\n", error);
                    printf("Common causes:\n");
                    printf("  - Not running as Administrator\n");
                    printf("  - Driver signing policy blocking unsigned driver\n");
                    printf("  - WinIo32.sys not found or corrupted\n");
                    return FALSE;
                }
            } else {
                printf("Failed to get WinIo function addresses\n");
                return FALSE;
            }
        } else {
            DWORD error = GetLastError();
            printf("Failed to load WinIo32.dll (Error: %d)\n", error);
            printf("Make sure WinIo32.dll is in the application directory\n");
            return FALSE;
        }
    }
}

static void cleanup_hardware_access(void) {
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT) {
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
static BOOL port_out_dword(WORD port, DWORD value) {
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

static DWORD port_in_dword(WORD port) {
    DWORD value = 0xFFFFFFFF;
    
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

// Find ITE8888 bridge
static int find_ite8888(void) {
    int bus, dev, func;
    
    printf("Scanning PCI buses for ITE8888 bridge...\n");
    
    for (bus = 0; bus < 256; bus++) {
        for (dev = 0; dev < 32; dev++) {
            for (func = 0; func < 8; func++) {
                DWORD id = win_pci_read(bus, dev, func, 0);
                
                if ((id & 0xFFFF) == ITE_VENDOR_ID && (id >> 16) == ITE8888_DEVICE_ID) {
                    win_state.bridge_bus = bus;
                    win_state.bridge_dev = dev;
                    win_state.bridge_func = func;
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

// Windows-specific system info display  
extern "C" void show_system_info(bridge_state_t* state) {
    SYSTEM_INFO sys_info;
    MEMORYSTATUS mem_status;
    
    GetSystemInfo(&sys_info);
    GlobalMemoryStatus(&mem_status);
    
    printf("\nWindows System Information:\n");
    printf("==========================\n");
    printf("Windows Version: %d.%d Build %d %s\n",
           os_version.dwMajorVersion, os_version.dwMinorVersion, os_version.dwBuildNumber,
           (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) ? "(9x/ME)" : "(NT/2000/XP)");
    printf("Processor: %s (%d CPUs)\n",
           (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) ? "Intel x86" : "Other",
           sys_info.dwNumberOfProcessors);
    printf("Memory: %d MB total, %d MB available\n", 
           (int)(mem_status.dwTotalPhys / (1024*1024)), 
           (int)(mem_status.dwAvailPhys / (1024*1024)));
    
    if (state->hw->hardware_available && hWinIo) {
        printf("Hardware Access: WinIO driver loaded and initialized\n");
    } else if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        printf("Hardware Access: Direct port I/O (Windows 9x)\n");
    } else {
        printf("Hardware Access: Not available (need WinIO on NT-based systems)\n");
    }
    
    if (state->bridge_bus >= 0) {
        printf("ITE8888 Bridge: Found at Bus %d, Device %d, Function %d\n",
               state->bridge_bus, state->bridge_dev, state->bridge_func);
    } else {
        printf("ITE8888 Bridge: Not detected\n");
    }
    printf("\n");
}

// Get hardware interface for Windows
extern "C" hardware_interface_t* get_hardware_interface(void) {
    return &win_hardware;
}

// Windows main function
extern "C" int main(int argc, char* argv[]) {
    bridge_state_t* state = &win_state;
    
    // Initialize hardware interface
    state->hw = get_hardware_interface();
    
    // Initialize hardware access
    if (!state->hw->hw_init()) {
        printf("Failed to initialize hardware access\n");
        printf("\nNote: On Windows NT/2000/XP, you must:\n");
        printf("1. Run as Administrator\n");
        printf("2. Have WinIo32.dll and WinIo32.sys in the application directory\n");
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
            show_usage("ITE8888CFG_XP");
            state->hw->hw_cleanup();
            return 0;
        }
        else {
            printf("Invalid option: %s\n", argv[1]);
            show_usage("ITE8888CFG_XP");
            state->hw->hw_cleanup();
            return 1;
        }
    }
    
    state->hw->hw_cleanup();
    return 0;
}