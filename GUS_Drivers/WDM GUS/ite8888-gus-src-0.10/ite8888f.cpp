/*
 * ite8888f.cpp
 *
 * ITE8888F PCI-to-ISA Bridge implementation for PicoGUS support
 */

#include "gf1cmn.h"
#include "ite8888f.h"

#pragma code_seg("PAGE")

/* Find ITE8888F PCI-to-ISA Bridge
 * O: Context - Bridge context structure
 */
NTSTATUS ITE8888F_FindBridge(PITE8888F_CONTEXT Context)
{
    PAGED_CODE();
    
    NTSTATUS status = STATUS_SUCCESS;
    PCI_SLOT_NUMBER slot;
    ULONG bus, device, function;
    ULONG vendorDevice;
    PCI_COMMON_CONFIG pciConfig;
    
    RtlZeroMemory(Context, sizeof(ITE8888F_CONTEXT));
    
    // Search all PCI buses for ITE8888F bridge
    for (bus = 0; bus < 256; bus++) {
        for (device = 0; device < 32; device++) {
            for (function = 0; function < 8; function++) {
                slot.u.AsULONG = 0;
                slot.u.bits.DeviceNumber = device;
                slot.u.bits.FunctionNumber = function;
                
                // Read vendor/device ID
                ULONG bytesRead = HalGetBusData(
                    PCIConfiguration,
                    bus,
                    slot.u.AsULONG,
                    &vendorDevice,
                    sizeof(ULONG)
                );
                
                if (bytesRead == sizeof(ULONG)) {
                    if (vendorDevice == ((ITE8888F_DEVICE_ID << 16) | ITE8888F_VENDOR_ID)) {
                        // Found ITE8888F bridge!
                        Context->bus_number = bus;
                        Context->device_number = device;
                        Context->function_number = function;
                        Context->bridge_configured = FALSE;
                        
                        GF1_INFINT(L"ITE8888F Bridge Found - Bus", bus);
                        GF1_INFINT(L"ITE8888F Bridge Found - Device", device);
                        GF1_INFINT(L"ITE8888F Bridge Found - Function", function);
                        
                        return STATUS_SUCCESS;
                    }
                }
                
                // If function 0 doesn't exist, skip other functions
                if (function == 0 && vendorDevice == 0xFFFFFFFF) {
                    break;
                }
            }
        }
    }
    
    return STATUS_DEVICE_NOT_FOUND;
}

/* Read PCI Configuration Register
 * I: Context - Bridge context
 * I: Offset - Configuration register offset
 */
ULONG ITE8888F_ReadPciConfig(PITE8888F_CONTEXT Context, ULONG Offset)
{
    PCI_SLOT_NUMBER slot;
    ULONG value = 0;
    
    slot.u.AsULONG = 0;
    slot.u.bits.DeviceNumber = Context->device_number;
    slot.u.bits.FunctionNumber = Context->function_number;
    
    HalGetBusData(
        PCIConfiguration,
        Context->bus_number,
        slot.u.AsULONG,
        &value,
        sizeof(ULONG)
    );
    
    return value;
}

/* Write PCI Configuration Register  
 * I: Context - Bridge context
 * I: Offset - Configuration register offset
 * I: Value - Value to write
 */
NTSTATUS ITE8888F_WritePciConfig(PITE8888F_CONTEXT Context, ULONG Offset, ULONG Value)
{
    PCI_SLOT_NUMBER slot;
    
    slot.u.AsULONG = 0;
    slot.u.bits.DeviceNumber = Context->device_number;
    slot.u.bits.FunctionNumber = Context->function_number;
    
    ULONG bytesWritten = HalSetBusData(
        PCIConfiguration,
        Context->bus_number,
        slot.u.AsULONG,
        &Value,
        sizeof(ULONG)
    );
    
    return (bytesWritten == sizeof(ULONG)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/* Configure I/O Address Spaces for PicoGUS
 * I: Context - Bridge context  
 * I: BasePort - GUS base port (0x220, 0x240, 0x260)
 */
NTSTATUS ITE8888F_SetupIoSpaces(PITE8888F_CONTEXT Context, ULONG BasePort)
{
    PAGED_CODE();
    
    NTSTATUS status;
    ULONG ioConfig;
    
    // Configure I/O Space 0: 0x2x0-0x2xF (16 bytes) - Main GUS registers
    ioConfig = ITE8888F_IO_ENABLE | ITE8888F_IO_FAST_DECODE | ITE8888F_IO_SIZE_16 | BasePort;
    status = ITE8888F_WritePciConfig(Context, ITE8888F_IO_SPACE_0, ioConfig);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Configure I/O Space 1: 0x3x0-0x3x7 (8 bytes) - MIDI/Voice registers
    ioConfig = ITE8888F_IO_ENABLE | ITE8888F_IO_FAST_DECODE | ITE8888F_IO_SIZE_8 | (BasePort + 0x100);
    status = ITE8888F_WritePciConfig(Context, ITE8888F_IO_SPACE_1, ioConfig);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Configure I/O Space 2: 0x7x6 (1 byte) - Revision register
    ioConfig = ITE8888F_IO_ENABLE | ITE8888F_IO_FAST_DECODE | ITE8888F_IO_SIZE_1 | (BasePort + GUS_REVISION_OFFSET);
    status = ITE8888F_WritePciConfig(Context, ITE8888F_IO_SPACE_2, ioConfig);
    
    Context->gus_base_port = BasePort;
    
    GF1_INFINT(L"ITE8888F I/O Space 0 (Main)", BasePort);
    GF1_INFINT(L"ITE8888F I/O Space 1 (MIDI)", BasePort + 0x100);
    GF1_INFINT(L"ITE8888F I/O Space 2 (Revision)", BasePort + GUS_REVISION_OFFSET);
    
    return status;
}

/* Configure DDMA Channels
 * I: Context - Bridge context
 * I: DmaChannels - DMA channel configuration
 */
NTSTATUS ITE8888F_SetupDmaChannels(PITE8888F_CONTEXT Context, ULONG DmaChannels)
{
    PAGED_CODE();
    
    NTSTATUS status;
    
    // Configure 8-bit DMA channels (0, 1)
    ULONG dmaConfig01 = ITE8888F_DDMA_ENABLE | ((DmaChannels & 0xFF) << 16) | (DmaChannels & 0xFF);
    status = ITE8888F_WritePciConfig(Context, ITE8888F_DDMA_CH_0_1, dmaConfig01);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Configure 16-bit DMA channel 5 with Type F timing
    ULONG dmaConfig5 = ITE8888F_DDMA_ENABLE | ITE8888F_DDMA_TYPE_F | ((DmaChannels >> 8) & 0xFF);
    status = ITE8888F_WritePciConfig(Context, ITE8888F_DDMA_CH_5, dmaConfig5);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Configure 16-bit DMA channels 6, 7
    ULONG dmaConfig67 = ITE8888F_DDMA_ENABLE | (((DmaChannels >> 12) & 0xFF) << 16) | ((DmaChannels >> 16) & 0xFF);
    status = ITE8888F_WritePciConfig(Context, ITE8888F_DDMA_CH_6_7, dmaConfig67);
    
    Context->dma_channels = DmaChannels;
    
    GF1_INFINT(L"ITE8888F DMA Channels Configured", DmaChannels);
    
    return status;
}

/* Enable and finalize bridge configuration
 * I: Context - Bridge context
 */
NTSTATUS ITE8888F_EnableBridge(PITE8888F_CONTEXT Context)
{
    PAGED_CODE();
    
    NTSTATUS status;
    
    // Enable PCI Command Register: I/O Space, Memory Space, Bus Master
    ULONG cmdReg = ITE8888F_CMD_IO_ENABLE | ITE8888F_CMD_MEM_ENABLE | ITE8888F_CMD_BUS_MASTER;
    status = ITE8888F_WritePciConfig(Context, ITE8888F_CMD_REG, cmdReg);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Configure Miscellaneous Control Register
    ULONG miscCtrl = ITE8888F_MISC_SUBTRACTIVE_DECODE | 
                     ITE8888F_MISC_DELAYED_TRANSACTION |
                     (4 << ITE8888F_MISC_IO_RECOVERY_SHIFT);  // 4 cycles recovery timing
    status = ITE8888F_WritePciConfig(Context, ITE8888F_MISC_CTRL, miscCtrl);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    // Set delayed transaction timers
    ULONG delayedTimer = (0x0F << 8) | 0x07;  // Discard: 15, Retry: 7
    status = ITE8888F_WritePciConfig(Context, ITE8888F_DELAYED_TIMER, delayedTimer);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    
    Context->bridge_configured = TRUE;
    
    GF1_INFSTR(L"ITE8888F Bridge", L"Configuration Complete");
    
    return STATUS_SUCCESS;
}

/* Complete bridge configuration for PicoGUS
 * I: Context - Bridge context
 * I: BasePort - GUS base port address
 * I: DmaChannels - DMA channel configuration  
 * I: IrqLine - IRQ line
 */
NTSTATUS ITE8888F_ConfigureBridge(PITE8888F_CONTEXT Context, ULONG BasePort, ULONG DmaChannels, ULONG IrqLine)
{
    PAGED_CODE();
    
    NTSTATUS status;
    
    // Setup I/O address spaces
    status = ITE8888F_SetupIoSpaces(Context, BasePort);
    if (!NT_SUCCESS(status)) {
        GF1_INFSTR(L"ITE8888F Error", L"Failed to setup I/O spaces");
        return status;
    }
    
    // Setup DMA channels
    status = ITE8888F_SetupDmaChannels(Context, DmaChannels);
    if (!NT_SUCCESS(status)) {
        GF1_INFSTR(L"ITE8888F Error", L"Failed to setup DMA channels");
        return status;
    }
    
    // Enable bridge operation
    status = ITE8888F_EnableBridge(Context);
    if (!NT_SUCCESS(status)) {
        GF1_INFSTR(L"ITE8888F Error", L"Failed to enable bridge");
        return status;
    }
    
    Context->irq_line = IrqLine;
    
    return STATUS_SUCCESS;
}

/* Validate bridge configuration
 * I: Context - Bridge context
 */
BOOLEAN ITE8888F_ValidateBridgeConfig(PITE8888F_CONTEXT Context)
{
    PAGED_CODE();
    
    if (!Context->bridge_configured) {
        return FALSE;
    }
    
    // Read back and verify key configuration registers
    ULONG cmdReg = ITE8888F_ReadPciConfig(Context, ITE8888F_CMD_REG);
    ULONG ioSpace0 = ITE8888F_ReadPciConfig(Context, ITE8888F_IO_SPACE_0);
    ULONG ioSpace1 = ITE8888F_ReadPciConfig(Context, ITE8888F_IO_SPACE_1);
    
    // Verify command register has required bits set
    if ((cmdReg & (ITE8888F_CMD_IO_ENABLE | ITE8888F_CMD_MEM_ENABLE)) == 0) {
        GF1_INFSTR(L"ITE8888F Validation", L"Command register validation failed");
        return FALSE;
    }
    
    // Verify I/O spaces are enabled
    if (!(ioSpace0 & ITE8888F_IO_ENABLE) || !(ioSpace1 & ITE8888F_IO_ENABLE)) {
        GF1_INFSTR(L"ITE8888F Validation", L"I/O space validation failed");
        return FALSE;
    }
    
    GF1_INFSTR(L"ITE8888F Validation", L"Bridge configuration validated successfully");
    
    return TRUE;
}

#pragma code_seg()