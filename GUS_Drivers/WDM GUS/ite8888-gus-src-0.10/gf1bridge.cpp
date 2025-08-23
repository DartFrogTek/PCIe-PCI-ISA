/*
 * gf1bridge.cpp
 *
 * Extended GF1Common functionality for ITE8888F bridge support
 */

#include "gf1cmn.h"
#include "ite8888f.h"

// Add to CGF1Common class private members:
/*
    // ITE8888F Bridge support
    ITE8888F_CONTEXT    bridge_context;
    BOOLEAN             using_bridge;
    
    // Bridge-aware I/O functions
    NTSTATUS setup_bridge_support(PRESOURCELIST resources);
    void setup_bridge_port_mapping(void);
    BOOLEAN detect_picogus_via_bridge(void);
*/

#pragma code_seg("PAGE")

/* Setup ITE8888F bridge support for PicoGUS
 * I: resources - hardware resource list
 */
NTSTATUS CGF1Common::setup_bridge_support(PRESOURCELIST resources)
{
    PAGED_CODE();
    
    NTSTATUS status;
    
    // Initialize bridge context
    RtlZeroMemory(&bridge_context, sizeof(ITE8888F_CONTEXT));
    using_bridge = FALSE;
    
    // Try to find ITE8888F bridge
    status = ITE8888F_FindBridge(&bridge_context);
    if (!NT_SUCCESS(status)) {
        GF1_INFSTR(L"BridgeDetection", L"No ITE8888F bridge found - using direct ISA access");
        return STATUS_SUCCESS;  // Not an error - just no bridge present
    }
    
    // Get port configuration from resources
    ULONG base_port = resources->FindTranslatedPort(0)->u.Port.Start.LowPart;
    ULONG dma_ch1 = resources->FindTranslatedDma(0)->u.Dma.Channel;
    ULONG dma_ch2 = resources->NumberOfDmas() > 1 ? 
                    resources->FindTranslatedDma(1)->u.Dma.Channel : dma_ch1;
    ULONG irq_line = resources->FindTranslatedInterrupt(0)->u.Interrupt.Level;
    
    // Configure the bridge
    ULONG dma_config = dma_ch1 | (dma_ch2 << 8);
    status = ITE8888F_ConfigureBridge(&bridge_context, base_port, dma_config, irq_line);
    if (!NT_SUCCESS(status)) {
        GF1_INFSTR(L"BridgeConfiguration", L"Failed to configure ITE8888F bridge");
        return status;
    }
    
    // Validate configuration
    if (!ITE8888F_ValidateBridgeConfig(&bridge_context)) {
        GF1_INFSTR(L"BridgeValidation", L"Bridge configuration validation failed");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    
    // Setup port mapping for bridge access
    setup_bridge_port_mapping();
    
    using_bridge = TRUE;
    GF1_INFSTR(L"BridgeSetup", L"ITE8888F bridge configured successfully for PicoGUS");
    
    return STATUS_SUCCESS;
}

/* Setup port mapping table for bridge access
 */
void CGF1Common::setup_bridge_port_mapping(void)
{
    PAGED_CODE();
    
    ULONG base_port = bridge_context.gus_base_port;
    ULONG i;
    
    // Clear existing mapping
    clear_dreg_to_port_table();
    
    // Map 0x2xx ports (Main GUS registers)
    for (i = 0; i < 0x10; i++) {
        dreg_to_port_table[i + 0x00] = (PUCHAR)(base_port + i);
    }
    
    // Map 0x3xx ports (MIDI/Voice registers)  
    for (i = 0; i < 0x10; i++) {
        dreg_to_port_table[i + 0x10] = (PUCHAR)(base_port + 0x100 + i);
    }
    
    // Map 0x7x6 port (Revision register)
    dreg_to_port_table[0x20] = (PUCHAR)(base_port + GUS_REVISION_OFFSET);
    
    GF1_INFINT(L"BridgePortMapping", base_port);
}

/* Enhanced PicoGUS detection through bridge
 */
BOOLEAN CGF1Common::detect_picogus_via_bridge(void)
{
    PAGED_CODE();
    
    if (!using_bridge) {
        return check_port();  // Use standard detection
    }
    
    // Bridge-specific PicoGUS detection
    GF1_INFSTR(L"PicoGUSDetection", L"Detecting PicoGUS through ITE8888F bridge");
    
    // Reset GF1 through bridge
    iwrite8(GF1REG_RESET, 0);
    GF1_DELAY(10);  // Longer delay for bridge
    iwrite8(GF1REG_RESET, GF1RES_RESET);
    GF1_DELAY(10);
    
    // Test DRAM access through bridge with multiple patterns
    ULONG test_addresses[] = {0x00000, 0x00123, 0x01234, 0x12345};
    UCHAR test_patterns[] = {0xDE, 0xAD, 0xBE, 0xEF};
    
    for (int i = 0; i < 4; i++) {
        poke(test_addresses[i], test_patterns[i]);
        if (peek(test_addresses[i]) != test_patterns[i]) {
            GF1_INFSTR(L"PicoGUSDetection", L"DRAM test failed - PicoGUS not responding");
            return FALSE;
        }
    }
    
    // Additional bridge-specific validation
    UCHAR revision = dread(GF1R_REVISION);
    GF1_INFINT(L"PicoGUSRevision", revision);
    
    GF1_INFSTR(L"PicoGUSDetection", L"PicoGUS detected successfully through bridge");
    
    return TRUE;
}

/* Modified initialization to support bridge detection
 * Add this to the existing CGF1Common::init() method after resource parsing
 */
NTSTATUS CGF1Common::init_with_bridge_support(
    IN PRESOURCELIST resources, 
    IN PDEVICE_OBJECT physical_device, 
    IN PDEVICE_OBJECT device)
{
    PAGED_CODE();
    
    // ... existing initialization code ...
    
    // Try to setup bridge support first
    status = setup_bridge_support(resources);
    if (!NT_SUCCESS(status)) {
        GF1_INFSTR(L"ErrorMessage", L"Failed to setup ITE8888F bridge support");
        return status;
    }
    
    // Enhanced port detection for PicoGUS
    if (!detect_picogus_via_bridge()) {
        GF1_INFSTR(L"ErrorMessage", L"PicoGUS not found or not responding");
        return STATUS_DEVICE_NOT_FOUND;  
    }
    
    // ... continue with rest of existing initialization ...
    
    if (using_bridge) {
        GF1_INFSTR(L"HardwareInfo", L"PicoGUS via ITE8888F PCI-to-ISA Bridge");
    }
    
    return STATUS_SUCCESS;
}

#pragma code_seg()