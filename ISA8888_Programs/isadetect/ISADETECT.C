/*
 * ISADETECT.C - ISA Card Detection Utility v1.0
 * Visual Studio 2010 / Windows 95/98/ME/NT/2000/XP version
 * Detects ISA cards behind IT8888F bridge or standalone
 * Usage: ISADETECT [options]
 */

#define WIN32_LEAN_AND_MEAN
#define USE_WINIO
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>

// Dynamic WinIO function pointers (same as original)
typedef BOOL (WINAPI *PFNINITIALIZEWINIO)(VOID);
typedef VOID (WINAPI *PFNSHUTDOWNWINIO)(VOID);
typedef BOOL (WINAPI *PFNGETPORTVAL)(WORD wPortAddr, PDWORD pdwPortVal, UCHAR bSize);
typedef BOOL (WINAPI *PFNSETPORTVAL)(WORD wPortAddr, DWORD dwPortVal, UCHAR bSize);

static HMODULE hWinIo = NULL;
static PFNINITIALIZEWINIO pInitializeWinIo = NULL;
static PFNSHUTDOWNWINIO pShutdownWinIo = NULL;
static PFNGETPORTVAL pGetPortVal = NULL;
static PFNSETPORTVAL pSetPortVal = NULL;

static BOOL hardware_access_available = FALSE;
static OSVERSIONINFO os_version;

// ISA PnP constants
#define PNP_ADDRESS_PORT    0x279
#define PNP_WRITE_DATA      0xA79
#define PNP_READ_DATA       0x203  // Can vary, typically 0x203-0x3FF

// Common ISA card base addresses to test
WORD gus_ports[] = {0x220, 0x230, 0x240, 0x250, 0x260, 0x280, 0};
WORD sb_ports[] = {0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x2E0, 0};
WORD adlib_ports[] = {0x388, 0x38C, 0};

// Detected card structure
typedef struct {
    char name[64];
    char description[128];
    WORD base_port;
    WORD secondary_port;
    int irq;
    int dma_8bit;
    int dma_16bit;
    BOOL pnp_card;
    DWORD pnp_id;
} DETECTED_CARD;

static DETECTED_CARD detected_cards[32];
static int num_detected = 0;

// Function prototypes
BOOL init_hardware_access(void);
void cleanup_hardware_access(void);
BOOL port_out_byte(WORD port, BYTE value);
BYTE port_in_byte(WORD port);
BOOL port_out_word(WORD port, WORD value);
WORD port_in_word(WORD port);
void delay_ms(int ms);

// Detection functions
int detect_isapnp_cards(void);
int detect_gravis_ultrasound(void);
int detect_sound_blaster(void);
int detect_adlib_opl(void);
int detect_ne2000_ethernet(void);
int detect_serial_ports(void);
int detect_parallel_ports(void);

void show_detected_cards(void);
void show_main_menu(void);
int interactive_mode(void);
void show_usage(void);
int save_detection_results(char *filename);

// Hardware access functions (same pattern as IT8888F utility)
BOOL init_hardware_access(void) {
    os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&os_version);
    
    printf("Initializing hardware access...\n");
    
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
        printf("Windows 95/98/ME - Direct hardware access enabled\n");
        hardware_access_available = TRUE;
    } else {
        printf("Windows NT/2000/XP - Loading WinIo driver...\n");
        
        hWinIo = LoadLibraryA("WinIo32.dll");
        if (hWinIo) {
            pInitializeWinIo = (PFNINITIALIZEWINIO)GetProcAddress(hWinIo, "InitializeWinIo");
            pShutdownWinIo = (PFNSHUTDOWNWINIO)GetProcAddress(hWinIo, "ShutdownWinIo");
            pGetPortVal = (PFNGETPORTVAL)GetProcAddress(hWinIo, "GetPortVal");
            pSetPortVal = (PFNSETPORTVAL)GetProcAddress(hWinIo, "SetPortVal");
            
            if (pInitializeWinIo && pShutdownWinIo && pGetPortVal && pSetPortVal) {
                if (pInitializeWinIo()) {
                    printf("WinIo driver loaded successfully\n");
                    hardware_access_available = TRUE;
                } else {
                    printf("WinIo driver failed to initialize\n");
                    hardware_access_available = FALSE;
                }
            } else {
                printf("Failed to get WinIo function addresses\n");
                hardware_access_available = FALSE;
            }
        } else {
            printf("Failed to load WinIo32.dll\n");
            hardware_access_available = FALSE;
        }
    }
    
    if (!hardware_access_available) {
        printf("WARNING: Hardware access not available - detection disabled\n");
    }
    
    return hardware_access_available;
}

void cleanup_hardware_access(void) {
    if (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT && hardware_access_available) {
        if (pShutdownWinIo) {
            pShutdownWinIo();
        }
        if (hWinIo) {
            FreeLibrary(hWinIo);
            hWinIo = NULL;
        }
    }
}

BOOL port_out_byte(WORD port, BYTE value) {
    if (!hardware_access_available) return FALSE;
    
    __try {
        if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            __asm {
                mov dx, port
                mov al, value
                out dx, al
            }
        } else {
            if (!pSetPortVal || !pSetPortVal(port, value, 1)) {
                return FALSE;
            }
        }
        return TRUE;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return FALSE;
    }
}

BYTE port_in_byte(WORD port) {
    DWORD value = 0xFF;
    
    if (!hardware_access_available) return 0xFF;
    
    __try {
        if (os_version.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            __asm {
                mov dx, port
                in al, dx
                mov byte ptr [value], al
            }
        } else {
            if (!pGetPortVal || !pGetPortVal(port, &value, 1)) {
                return 0xFF;
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0xFF;
    }
    
    return (BYTE)value;
}

void delay_ms(int ms) {
    Sleep(ms);
}

// ISA PnP Detection
int detect_isapnp_cards(void) {
    int cards_found = 0;
    BYTE checksum, bit, data;
    int i, card_num;
    
    printf("Scanning for ISA PnP cards...\n");
    
    if (!hardware_access_available) {
        printf("  Hardware access not available\n");
        return 0;
    }
    
    // Send initiation key sequence
    port_out_byte(PNP_ADDRESS_PORT, 0x00);
    port_out_byte(PNP_ADDRESS_PORT, 0x00);
    
    // Send PnP initiation sequence
    checksum = 0x6A;
    port_out_byte(PNP_ADDRESS_PORT, checksum);
    
    for (i = 1; i < 32; i++) {
        data = checksum;
        checksum = ((checksum >> 1) | (((checksum ^ (checksum >> 1)) << 7) & 0x80)) & 0xFF;
        port_out_byte(PNP_ADDRESS_PORT, checksum);
    }
    
    // Try to isolate cards
    for (card_num = 0; card_num < 8; card_num++) {
        // Wake up cards
        port_out_byte(PNP_ADDRESS_PORT, 0x03);  // Wake[CSN]
        port_out_byte(PNP_WRITE_DATA, card_num);
        
        // Try to read vendor ID
        port_out_byte(PNP_ADDRESS_PORT, 0x01);  // Set RD_DATA port
        port_out_byte(PNP_WRITE_DATA, 0x03);    // Use port 0x203
        
        port_out_byte(PNP_ADDRESS_PORT, 0x00);  // Vendor ID high
        BYTE vendor_high = port_in_byte(0x203);
        
        port_out_byte(PNP_ADDRESS_PORT, 0x01);  // Vendor ID low  
        BYTE vendor_low = port_in_byte(0x203);
        
        if (vendor_high != 0xFF || vendor_low != 0xFF) {
            // Found potential PnP card
            DETECTED_CARD *card = &detected_cards[num_detected];
            card->pnp_card = TRUE;
            card->pnp_id = (vendor_high << 8) | vendor_low;
            
            sprintf(card->name, "PNP%04X", card->pnp_id);
            sprintf(card->description, "ISA PnP Card (ID: %04X)", card->pnp_id);
            
            printf("  Found PnP card: %s\n", card->description);
            num_detected++;
            cards_found++;
        }
    }
    
    return cards_found;
}

// Gravis UltraSound Detection
int detect_gravis_ultrasound(void) {
    int cards_found = 0;
    int i;
    
    printf("Scanning for Gravis UltraSound cards...\n");
    
    if (!hardware_access_available) {
        printf("  Hardware access not available\n");
        return 0;
    }
    
    for (i = 0; gus_ports[i] != 0; i++) {
        WORD base = gus_ports[i];
        
        // Test GUS-specific registers
        // Try to select voice 0
        port_out_byte(base + 0x102, 0x00);  // Voice select (low)
        port_out_byte(base + 0x103, 0x00);  // Voice select (high)
        
        // Read back voice select
        BYTE voice_low = port_in_byte(base + 0x102);
        BYTE voice_high = port_in_byte(base + 0x103);
        
        if (voice_low == 0x00 && voice_high == 0x00) {
            // Try selecting voice 31
            port_out_byte(base + 0x102, 31);
            port_out_byte(base + 0x103, 0);
            
            voice_low = port_in_byte(base + 0x102);
            
            if (voice_low == 31) {
                // Looks like a GUS
                DETECTED_CARD *card = &detected_cards[num_detected];
                strcpy(card->name, "GUS");
                sprintf(card->description, "Gravis UltraSound at 0x%03X", base);
                card->base_port = base;
                card->secondary_port = base + 0x100;
                card->pnp_card = FALSE;
                
                printf("  Found: %s\n", card->description);
                num_detected++;
                cards_found++;
            }
        }
    }
    
    return cards_found;
}

// Sound Blaster Detection
int detect_sound_blaster(void) {
    int cards_found = 0;
    int i;
    
    printf("Scanning for Sound Blaster cards...\n");
    
    if (!hardware_access_available) {
        printf("  Hardware access not available\n");
        return 0;
    }
    
    for (i = 0; sb_ports[i] != 0; i++) {
        WORD base = sb_ports[i];
        int timeout;
        
        // Reset DSP
        port_out_byte(base + 0x06, 0x01);
        delay_ms(3);
        port_out_byte(base + 0x06, 0x00);
        
        // Wait for DSP ready (0xAA at read port)
        timeout = 1000;
        while (timeout-- > 0) {
            if (port_in_byte(base + 0x0A) & 0x80) {  // Data available?
                if (port_in_byte(base + 0x0A) == 0xAA) {
                    // Found Sound Blaster
                    DETECTED_CARD *card = &detected_cards[num_detected];
                    strcpy(card->name, "SB");
                    sprintf(card->description, "Sound Blaster at 0x%03X", base);
                    card->base_port = base;
                    card->pnp_card = FALSE;
                    
                    printf("  Found: %s\n", card->description);
                    num_detected++;
                    cards_found++;
                    break;
                }
            }
            delay_ms(1);
        }
    }
    
    return cards_found;
}

// AdLib/OPL2 Detection  
int detect_adlib_opl(void) {
    int cards_found = 0;
    int i;
    
    printf("Scanning for AdLib/OPL cards...\n");
    
    if (!hardware_access_available) {
        printf("  Hardware access not available\n");
        return 0;
    }
    
    for (i = 0; adlib_ports[i] != 0; i++) {
        WORD base = adlib_ports[i];
        BYTE timer1, timer2;
        
        // Test OPL2 timers
        // Reset timers
        port_out_byte(base, 0x04);        // Timer control register
        port_out_byte(base + 1, 0x60);    // Reset IRQ and mask timers
        port_out_byte(base, 0x04);
        port_out_byte(base + 1, 0x80);    // Reset IRQ
        
        // Read status
        timer1 = port_in_byte(base);
        
        // Start timer 1
        port_out_byte(base, 0x02);        // Timer 1 register
        port_out_byte(base + 1, 0xFF);
        port_out_byte(base, 0x04);
        port_out_byte(base + 1, 0x21);    // Start timer 1
        
        delay_ms(80);  // Wait for timer
        
        timer2 = port_in_byte(base);
        
        // Reset
        port_out_byte(base, 0x04);
        port_out_byte(base + 1, 0x60);
        port_out_byte(base, 0x04);
        port_out_byte(base + 1, 0x80);
        
        if ((timer1 & 0xE0) == 0 && (timer2 & 0xE0) == 0xC0) {
            // Found OPL2
            DETECTED_CARD *card = &detected_cards[num_detected];
            strcpy(card->name, "ADLIB");
            sprintf(card->description, "AdLib/OPL2 at 0x%03X", base);
            card->base_port = base;
            card->pnp_card = FALSE;
            
            printf("  Found: %s\n", card->description);
            num_detected++;
            cards_found++;
        }
    }
    
    return cards_found;
}

// NE2000 Ethernet Detection
int detect_ne2000_ethernet(void) {
    int cards_found = 0;
    WORD ne_ports[] = {0x300, 0x320, 0x340, 0x360, 0x280, 0x2A0, 0x2C0, 0x2E0, 0};
    int i;
    
    printf("Scanning for NE2000 Ethernet cards...\n");
    
    if (!hardware_access_available) {
        printf("  Hardware access not available\n");
        return 0;
    }
    
    for (i = 0; ne_ports[i] != 0; i++) {
        WORD base = ne_ports[i];
        BYTE test1, test2;
        
        // Try NE2000 detection sequence
        port_out_byte(base + 0x1F, port_in_byte(base + 0x1F));  // Reset
        delay_ms(2);
        
        test1 = port_in_byte(base + 0x0D);
        port_out_byte(base, 0x21);    // Page 0, no DMA, stop
        delay_ms(2);
        test2 = port_in_byte(base + 0x0D);
        
        if (test1 != test2) {
            // Might be NE2000
            DETECTED_CARD *card = &detected_cards[num_detected];
            strcpy(card->name, "NE2000");
            sprintf(card->description, "NE2000 Ethernet at 0x%03X", base);
            card->base_port = base;
            card->pnp_card = FALSE;
            
            printf("  Found: %s\n", card->description);
            num_detected++;
            cards_found++;
        }
    }
    
    return cards_found;
}

// Serial Port Detection
int detect_serial_ports(void) {
    int cards_found = 0;
    WORD serial_ports[] = {0x3F8, 0x2F8, 0x3E8, 0x2E8, 0};
    char *port_names[] = {"COM1", "COM2", "COM3", "COM4"};
    int i;
    
    printf("Scanning for Serial ports...\n");
    
    if (!hardware_access_available) {
        printf("  Hardware access not available\n");
        return 0;
    }
    
    for (i = 0; serial_ports[i] != 0; i++) {
        WORD base = serial_ports[i];
        BYTE scratch_test;
        
        // Test scratch register
        port_out_byte(base + 7, 0x55);
        scratch_test = port_in_byte(base + 7);
        
        if (scratch_test == 0x55) {
            port_out_byte(base + 7, 0xAA);
            scratch_test = port_in_byte(base + 7);
            
            if (scratch_test == 0xAA) {
                DETECTED_CARD *card = &detected_cards[num_detected];
                strcpy(card->name, port_names[i]);
                sprintf(card->description, "Serial Port %s at 0x%03X", port_names[i], base);
                card->base_port = base;
                card->pnp_card = FALSE;
                
                printf("  Found: %s\n", card->description);
                num_detected++;
                cards_found++;
            }
        }
    }
    
    return cards_found;
}

void show_detected_cards(void) {
    int i;
    
    printf("\n=== ISA Card Detection Results ===\n");
    printf("Total cards detected: %d\n\n", num_detected);
    
    if (num_detected == 0) {
        printf("No ISA cards detected.\n");
        printf("This could mean:\n");
        printf("  - No ISA cards are installed\n");
        printf("  - Cards are not configured for standard addresses\n");
        printf("  - ISA bridge is not properly configured\n");
        printf("  - Hardware access is not available\n");
        return;
    }
    
    for (i = 0; i < num_detected; i++) {
        DETECTED_CARD *card = &detected_cards[i];
        printf("%d. %s\n", i + 1, card->description);
        printf("   Base Port: 0x%03X", card->base_port);
        if (card->secondary_port) {
            printf(", Secondary: 0x%03X", card->secondary_port);
        }
        if (card->pnp_card) {
            printf(" [PnP ID: %04X]", card->pnp_id);
        }
        printf("\n\n");
    }
}

void show_main_menu(void) {
    printf("\nISA Card Detection Utility\n");
    printf("==========================\n");
    printf("1. Scan all card types\n");
    printf("2. Scan ISA PnP cards only\n");
    printf("3. Scan sound cards only\n");
    printf("4. Show detection results\n");
    printf("5. Save results to file\n");
    printf("Q. Quit\n");
    printf("\nChoice: ");
}

int interactive_mode(void) {
    char choice;
    char filename[256];
    
    while (1) {
        show_main_menu();
        choice = _getch();
        printf("%c\n", choice);
        
        switch (toupper(choice)) {
            case '1':
                num_detected = 0;
                detect_isapnp_cards();
                detect_gravis_ultrasound();
                detect_sound_blaster();
                detect_adlib_opl();
                detect_ne2000_ethernet();
                detect_serial_ports();
                show_detected_cards();
                break;
                
            case '2':
                num_detected = 0;
                detect_isapnp_cards();
                show_detected_cards();
                break;
                
            case '3':
                num_detected = 0;
                detect_gravis_ultrasound();
                detect_sound_blaster();
                detect_adlib_opl();
                show_detected_cards();
                break;
                
            case '4':
                show_detected_cards();
                break;
                
            case '5':
                printf("Filename: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\r\n")] = 0;  // Remove newline
                save_detection_results(filename);
                break;
                
            case 'Q':
                return 1;
                
            default:
                printf("Invalid choice!\n");
        }
        
        printf("\nPress any key to continue...");
        _getch();
        printf("\n");
    }
}

int save_detection_results(char *filename) {
    FILE *f = fopen(filename, "w");
    int i;
    
    if (!f) {
        printf("ERROR: Cannot create file '%s'\n", filename);
        return 0;
    }
    
    fprintf(f, "ISA Card Detection Results\n");
    fprintf(f, "==========================\n");
    fprintf(f, "Total cards detected: %d\n\n", num_detected);
    
    for (i = 0; i < num_detected; i++) {
        DETECTED_CARD *card = &detected_cards[i];
        fprintf(f, "%d. %s\n", i + 1, card->description);
        fprintf(f, "   Base Port: 0x%03X", card->base_port);
        if (card->secondary_port) {
            fprintf(f, ", Secondary: 0x%03X", card->secondary_port);
        }
        if (card->pnp_card) {
            fprintf(f, " [PnP ID: %04X]", card->pnp_id);
        }
        fprintf(f, "\n\n");
    }
    
    fclose(f);
    printf("Results saved to '%s'\n", filename);
    return 1;
}

void show_usage(void) {
    printf("ISA Card Detection Utility v1.0\n");
    printf("Usage:\n");
    printf("  ISADETECT               - Interactive mode\n");
    printf("  ISADETECT -all          - Scan all card types\n");
    printf("  ISADETECT -pnp          - Scan PnP cards only\n");
    printf("  ISADETECT -sound        - Scan sound cards only\n");
    printf("  ISADETECT -save <file>  - Save results to file\n");
    printf("  ISADETECT -help         - Show this help\n");
    printf("\nSupported card types:\n");
    printf("  - ISA Plug and Play cards\n");
    printf("  - Gravis UltraSound\n");
    printf("  - Sound Blaster compatible\n");
    printf("  - AdLib/OPL2 compatible\n");
    printf("  - NE2000 Ethernet\n");
    printf("  - Serial ports\n");
}

int main(int argc, char *argv[]) {
    printf("ISA Card Detection Utility v1.0\n");
    printf("===============================\n\n");
    
    if (!init_hardware_access()) {
        printf("Hardware access initialization failed\n");
        printf("Note: This utility requires hardware port access\n");
        printf("On NT/2000/XP, ensure WinIo32.dll and WinIo32.sys are present\n");
    }
    
    if (argc == 1) {
        interactive_mode();
    }
    else if (argc >= 2) {
        if (strcmp(argv[1], "-all") == 0) {
            detect_isapnp_cards();
            detect_gravis_ultrasound();
            detect_sound_blaster();
            detect_adlib_opl();
            detect_ne2000_ethernet();
            detect_serial_ports();
            show_detected_cards();
        }
        else if (strcmp(argv[1], "-pnp") == 0) {
            detect_isapnp_cards();
            show_detected_cards();
        }
        else if (strcmp(argv[1], "-sound") == 0) {
            detect_gravis_ultrasound();
            detect_sound_blaster();
            detect_adlib_opl();
            show_detected_cards();
        }
        else if (strcmp(argv[1], "-save") == 0 && argc >= 3) {
            // Run detection first, then save
            detect_isapnp_cards();
            detect_gravis_ultrasound();
            detect_sound_blaster();
            detect_adlib_opl();
            detect_ne2000_ethernet();
            detect_serial_ports();
            save_detection_results(argv[2]);
        }
        else if (strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "/?") == 0) {
            show_usage();
        }
        else {
            printf("Invalid option: %s\n", argv[1]);
            show_usage();
        }
    }
    
    cleanup_hardware_access();
    return 0;
}
