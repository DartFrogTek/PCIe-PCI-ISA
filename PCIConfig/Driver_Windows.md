# **Configuring IT8888 in Windows Post-Boot**

## **Implementation Approaches**

### **1. Windows Kernel Driver** (Recommended Approach)
```c
// Kernel driver, use standard PCI config access
ULONG ConfigData;
PCI_SLOT_NUMBER SlotNumber;

// Read current config
HalGetBusDataByOffset(PCIConfiguration, 
                     BusNumber, SlotNumber, 
                     &ConfigData, 0x50, 4);

// Configure IT8888 (example: enable subtractive decode)
ConfigData |= 0x01;  // Set Cfg_50h<0> = 1
HalSetBusDataByOffset(PCIConfiguration, 
                     BusNumber, SlotNumber, 
                     &ConfigData, 0x50, 4);
```

### **2. User-Mode with Helper Libraries**
**For development/testing:**
- **WinIo library**: Provides user-mode access to I/O ports (requires admin rights)
- **DirectIO**: Similar functionality
- **Custom kernel service**: Write minimal kernel service just for PCI config access

```c
// Using WinIo library requires admin privileges
#include "winio.h"

bool ConfigureIT8888(UCHAR bus, UCHAR device, UCHAR function) {
    if (!InitializeWinIo()) return false;
    
    // Standard PCI config mechanism
    DWORD configAddr = 0x80000000 | 
                      (bus << 16) | 
                      (device << 11) | 
                      (function << 8) | 
                      0x50;  // Cfg_50h register
    
    // Write config address
    SetPortVal(0xCF8, configAddr, 4);
    
    // Read current value
    DWORD currentVal = GetPortVal(0xCFC, 4);
    
    // Enable subtractive decode and other settings
    DWORD newVal = currentVal | 0x01;  // Enable subtractive decode
    
    // Write back
    SetPortVal(0xCF8, configAddr, 4);
    SetPortVal(0xCFC, newVal, 4);
    
    ShutdownWinIo();
    return true;
}
```

## **Configuration Sequence for ISA Card Support**

**Typical initialization sequence:**
```c
void InitializeIT8888ForISACards() {
    // 1. Enable basic PCI functions
    WritePCIConfig(bus, dev, func, 0x04, 0x0007);  // Command register
    
    // 2. Configure main control register
    DWORD cfg50h = 0x00000001;  // Enable subtractive decode
    cfg50h |= 0x00000020;       // Enable Port 80h snoop
    WritePCIConfig(bus, dev, func, 0x50, cfg50h);
    
    // 3. Set up I/O decode windows for specific ISA cards
    // Example: Sound card at 0x220-0x22F
    DWORD io_space0 = 0x80000000 |  // Enable
                      (0x02 << 29) | // Medium speed
                      (0x04 << 24) | // 16 bytes
                      0x0220;        // Base address
    WritePCIConfig(bus, dev, func, 0x58, io_space0);
    
    // 4. Set up memory decode if needed
    // Example: VGA memory window
    DWORD mem_space0 = 0x80000000 |  // Enable
                       (0x02 << 29) | // Medium speed  
                       (0x02 << 24) | // 64KB
                       0xA000;        // VGA memory
    WritePCIConfig(bus, dev, func, 0x70, mem_space0);
    
    // 5. Configure timing and other parameters
    DWORD cfg54h = 0x00000000;  // Default retry/discard timers
    WritePCIConfig(bus, dev, func, 0x54, cfg54h);
}
```

## **Detection and Enumeration**

**Finding the IT8888 on the PCI bus:**
```c
bool FindIT8888(UCHAR* bus, UCHAR* device, UCHAR* function) {
    for (int b = 0; b < 256; b++) {
        for (int d = 0; d < 32; d++) {
            for (int f = 0; f < 8; f++) {
                DWORD vendorDevice = ReadPCIConfig(b, d, f, 0x00);
                if (vendorDevice == 0x88881283) {  // IT8888 + ITE vendor
                    *bus = b; *device = d; *function = f;
                    return true;
                }
            }
        }
    }
    return false;
}
```

## **Example Driver Structure**

```c
// Driver entry point
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, 
                    PUNICODE_STRING RegistryPath) {
    // Find IT8888 device
    UCHAR bus, device, function;
    if (!FindIT8888(&bus, &device, &function)) {
        return STATUS_DEVICE_NOT_FOUND;
    }
    
    // Configure for ISA card support
    InitializeIT8888ForISACards();
    
    // Create device object for user interaction
    CreateDeviceObject(DriverObject);
    
    return STATUS_SUCCESS;
}
```

---

## **Complete DMA Configuration Code**

```c
// DMA Configuration Structure
typedef struct {
    UCHAR  bus, device, function;
    BOOLEAN ddma_enabled;
    BOOLEAN ppdma_enabled;
    BOOLEAN type_f_timing;
} IT8888_DMA_CONFIG;

void ConfigureIT8888_DMA(IT8888_DMA_CONFIG* config) {
    
    //  Step 1: Configure DDMA Channels 0 & 1 (Cfg_40h) 
    DWORD cfg_40h = 0;
    
    // Channel 0 (8-bit): Base address 0x1000, enabled, extended addressing
    cfg_40h |= (0x100 << 4);    // Base address A[15:4] = 0x100 (0x1000)
    cfg_40h |= (1 << 3);        // Extended addressing enable
    cfg_40h |= (1 << 0);        // Channel 0 enable
    
    // Channel 1 (8-bit): Base address 0x2000, enabled, extended addressing  
    cfg_40h |= (0x200 << 20);   // Base address A[15:4] = 0x200 (0x2000)
    cfg_40h |= (1 << 19);       // Extended addressing enable
    cfg_40h |= (1 << 16);       // Channel 1 enable
    
    WritePCIConfig(config->bus, config->device, config->function, 0x40, cfg_40h);
    
    //  Step 2: Configure DDMA Channels 2 & 3 (Cfg_44h) 
    DWORD cfg_44h = 0;
    
    // Channel 2 (8-bit): Base address 0x3000, enabled
    cfg_44h |= (0x300 << 4);    // Base address A[15:4] = 0x300 (0x3000)
    cfg_44h |= (1 << 3);        // Extended addressing enable
    cfg_44h |= (1 << 0);        // Channel 2 enable
    
    // Channel 3 (8-bit): Base address 0x4000, enabled
    cfg_44h |= (0x400 << 20);   // Base address A[15:4] = 0x400 (0x4000)
    cfg_44h |= (1 << 19);       // Extended addressing enable  
    cfg_44h |= (1 << 16);       // Channel 3 enable
    
    WritePCIConfig(config->bus, config->device, config->function, 0x44, cfg_44h);
    
    //  Step 3: Configure Channel 5, Type-F Timing, PPDMA (Cfg_48h) 
    DWORD cfg_48h = 0;
    
    // PC/PCI DMA Control (bits 7:0)
    cfg_48h |= 0xF0;            // Enable DREQ for channels 7,6,5 (bits 7:5)
    cfg_48h |= (1 << 4);        // PPDMA Global Enable
    cfg_48h |= 0x0F;            // Enable DREQ for channels 3,2,1,0 (bits 3:0)
    
    // Type-F DMA Timing (bits 15:8) - Faster, non-ISA compatible timing
    if (config->type_f_timing) {
        cfg_48h |= 0xFF00;      // Enable Type-F for all channels
    }
    
    // Channel 5 (16-bit): Base address 0x5000, enabled (bits 31:16)
    cfg_48h |= (0x500 << 20);   // Base address A[15:4] = 0x500 (0x5000)
    cfg_48h |= (1 << 19);       // Extended addressing enable
    cfg_48h |= (1 << 16);       // Channel 5 enable
    
    WritePCIConfig(config->bus, config->device, config->function, 0x48, cfg_48h);
    
    //  Step 4: Configure DDMA Channels 6 & 7 (Cfg_4Ch) 
    DWORD cfg_4ch = 0;
    
    // Channel 6 (16-bit): Base address 0x6000, enabled
    cfg_4ch |= (0x600 << 4);    // Base address A[15:4] = 0x600 (0x6000)
    cfg_4ch |= (1 << 3);        // Extended addressing enable
    cfg_4ch |= (1 << 0);        // Channel 6 enable
    
    // Channel 7 (16-bit): Base address 0x7000, enabled
    cfg_4ch |= (0x700 << 20);   // Base address A[15:4] = 0x700 (0x7000)
    cfg_4ch |= (1 << 19);       // Extended addressing enable
    cfg_4ch |= (1 << 16);       // Channel 7 enable
    
    WritePCIConfig(config->bus, config->device, config->function, 0x4C, cfg_4ch);
}
```

## **Practical DMA Setup Functions**

```c
// Configure DMA for specific ISA card scenarios
void SetupDMAForISACards() {
    IT8888_DMA_CONFIG config = {0};
    
    // Find IT8888 device
    if (!FindIT8888(&config.bus, &config.device, &config.function)) {
        return;
    }
    
    // Enable both DMA systems
    config.ddma_enabled = TRUE;
    config.ppdma_enabled = TRUE;
    config.type_f_timing = FALSE;  // Use ISA-compatible timing initially
    
    ConfigureIT8888_DMA(&config);
    
    printf("IT8888 DMA Configuration Complete:\n");
    printf("- DDMA Channels 0-3,5-7: Enabled\n");
    printf("- PC/PCI DMA: Enabled\n");
    printf("- Base addresses: 0x1000-0x7000 (16-byte aligned)\n");
}

// Configure for Sound Blaster compatible card
void SetupDMA_SoundBlaster() {
    IT8888_DMA_CONFIG config = {0};
    FindIT8888(&config.bus, &config.device, &config.function);
    
    // Sound Blaster typically uses DMA 1 (8-bit) and DMA 5 (16-bit)
    DWORD cfg_40h = 0;
    cfg_40h |= (0x100 << 20);   // Channel 1: Base 0x1000
    cfg_40h |= (1 << 19);       // Extended addressing
    cfg_40h |= (1 << 16);       // Enable channel 1
    WritePCIConfig(config.bus, config.device, config.function, 0x40, cfg_40h);
    
    DWORD cfg_48h = ReadPCIConfig(config.bus, config.device, config.function, 0x48);
    cfg_48h |= (0x500 << 20);   // Channel 5: Base 0x5000  
    cfg_48h |= (1 << 19);       // Extended addressing
    cfg_48h |= (1 << 16);       // Enable channel 5
    cfg_48h |= 0xF0 | (1 << 4) | 0x0F;  // Enable PPDMA
    WritePCIConfig(config.bus, config.device, config.function, 0x48, cfg_48h);
}

// Enable Type-F timing for better performance
void EnableTypeFDMA() {
    IT8888_DMA_CONFIG config = {0};
    FindIT8888(&config.bus, &config.device, &config.function);
    
    DWORD cfg_48h = ReadPCIConfig(config.bus, config.device, config.function, 0x48);
    cfg_48h |= 0xFF00;  // Enable Type-F timing for all channels
    WritePCIConfig(config.bus, config.device, config.function, 0x48, cfg_48h);
    
    printf("Type-F DMA timing enabled - faster but non-ISA compatible\n");
}
```

## **DMA Channel Status and Diagnostics**

```c
void DisplayDMAStatus() {
    IT8888_DMA_CONFIG config = {0};
    FindIT8888(&config.bus, &config.device, &config.function);
    
    printf("IT8888 DMA Configuration Status:\n\n");
    
    // Read all DMA configuration registers
    DWORD cfg_40h = ReadPCIConfig(config.bus, config.device, config.function, 0x40);
    DWORD cfg_44h = ReadPCIConfig(config.bus, config.device, config.function, 0x44);
    DWORD cfg_48h = ReadPCIConfig(config.bus, config.device, config.function, 0x48);
    DWORD cfg_4ch = ReadPCIConfig(config.bus, config.device, config.function, 0x4C);
    
    // Display channel status
    printf("Channel 0 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_40h & 0x01) ? "Enabled" : "Disabled",
           ((cfg_40h >> 4) & 0xFFF) << 4);
           
    printf("Channel 1 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_40h & 0x10000) ? "Enabled" : "Disabled",
           ((cfg_40h >> 20) & 0xFFF) << 4);
           
    printf("Channel 2 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_44h & 0x01) ? "Enabled" : "Disabled",
           ((cfg_44h >> 4) & 0xFFF) << 4);
           
    printf("Channel 3 (8-bit):  %s, Base: 0x%04X\n",
           (cfg_44h & 0x10000) ? "Enabled" : "Disabled",
           ((cfg_44h >> 20) & 0xFFF) << 4);
           
    printf("Channel 5 (16-bit): %s, Base: 0x%04X\n",
           (cfg_48h & 0x10000) ? "Enabled" : "Disabled",
           ((cfg_48h >> 20) & 0xFFF) << 4);
           
    printf("Channel 6 (16-bit): %s, Base: 0x%04X\n",
           (cfg_4ch & 0x01) ? "Enabled" : "Disabled",
           ((cfg_4ch >> 4) & 0xFFF) << 4);
           
    printf("Channel 7 (16-bit): %s, Base: 0x%04X\n",
           (cfg_4ch & 0x10000) ? "Enabled" : "Disabled",
           ((cfg_4ch >> 20) & 0xFFF) << 4);
    
    // PC/PCI DMA status
    printf("\nPC/PCI DMA: %s\n", (cfg_48h & 0x10) ? "Enabled" : "Disabled");
    printf("Type-F Timing: 0x%02X\n", (cfg_48h >> 8) & 0xFF);
}
```