/*
 * ITE8888_config.c
 *
 * ITE8888 Bridge Configuration Implementation
 */

#include "ite8888_driver.h"

// Register offset tables
static const ULONG IoSpaceRegisters[6] = {
    ITE8888_IO_SPACE_0, ITE8888_IO_SPACE_1, ITE8888_IO_SPACE_2,
    ITE8888_IO_SPACE_3, ITE8888_IO_SPACE_4, ITE8888_IO_SPACE_5
};

static const ULONG MemSpaceRegisters[4] = {
    ITE8888_MEM_SPACE_0, ITE8888_MEM_SPACE_1, 
    ITE8888_MEM_SPACE_2, ITE8888_MEM_SPACE_3
};

/*
 * Configure ITE8888 bridge for ISA card operation
 */
NTSTATUS ConfigureBridge(
    IN PDEVICE_EXTENSION DeviceExtension
)
{
    NTSTATUS status;
    ULONG configValue;
    ULONG i;

    PAGED_CODE();

    DbgPrint(DBG_INFO, "Configuring ITE8888 bridge for %ws", CARD_NAME);

    // Step 1: Enable PCI Command Register (I/O Space, Memory Space, Bus Master)
    configValue = PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE | PCI_ENABLE_BUS_MASTER;
    status = WritePciConfig(DeviceExtension, PCI_COMMAND_REGISTER, configValue);
    if (!NT_SUCCESS(status)) {
        DbgPrint(DBG_ERROR, "Failed to enable PCI command register: 0x%08X", status);
        return status;
    }
    DbgPrint(DBG_VERBOSE, "PCI command register configured");

    // Step 2: Configure Miscellaneous Control Register
    configValue = 0;
    if (ENABLE_SUBTRACTIVE_DECODE) {
        configValue |= 0x00000001;  // Enable subtractive decode
    }
    if (ENABLE_DELAYED_TRANSACTION) {
        configValue |= 0x00000002;  // Enable delayed transaction
    }
    configValue |= (IO_RECOVERY_TIME << 8);  // I/O recovery time

    status = WritePciConfig(DeviceExtension, ITE8888_MISC_CONTROL, configValue);
    if (!NT_SUCCESS(status)) {
        DbgPrint(DBG_ERROR, "Failed to configure misc control register: 0x%08X", status);
        return status;
    }
    DbgPrint(DBG_VERBOSE, "Miscellaneous control register configured: 0x%08X", configValue);

    // Step 3: Configure I/O Spaces
    for (i = 0; i < 6; i++) {
        if (IoSpaceConfig[i].Enable) {
            configValue = 0;
            
            // Enable bit
            configValue |= IO_SPACE_ENABLE;
            
            // Speed (bits 30:29)
            configValue |= (IoSpaceConfig[i].Speed << IO_SPACE_SPEED_SHIFT);
            
            // Alias enable (bit 28)
            if (IoSpaceConfig[i].Alias) {
                configValue |= IO_SPACE_ALIAS;
            }
            
            // Size (bits 26:24)
            configValue |= (IoSpaceConfig[i].Size << IO_SPACE_SIZE_SHIFT);
            
            // Base address (bits 15:0)
            configValue |= (IoSpaceConfig[i].BaseAddress & 0xFFFF);

            status = WritePciConfig(DeviceExtension, IoSpaceRegisters[i], configValue);
            if (!NT_SUCCESS(status)) {
                DbgPrint(DBG_ERROR, "Failed to configure I/O space %d: 0x%08X", i, status);
                return status;
            }
            
            DbgPrint(DBG_INFO, "I/O Space %d configured: Base=0x%X, Size=%d bytes, Speed=%d",
                     i, IoSpaceConfig[i].BaseAddress, 
                     1 << IoSpaceConfig[i].Size, IoSpaceConfig[i].Speed);
        }
    }

    // Step 4: Configure Memory Spaces
    for (i = 0; i < 4; i++) {
        if (MemSpaceConfig[i].Enable) {
            configValue = 0;
            
            // Enable bit
            configValue |= MEM_SPACE_ENABLE;
            
            // Speed (bits 30:29)
            configValue |= (MemSpaceConfig[i].Speed << MEM_SPACE_SPEED_SHIFT);
            
            // Size (bits 26:24)
            configValue |= (MemSpaceConfig[i].Size << MEM_SPACE_SIZE_SHIFT);
            
            // Base address (bits 23:0)
            configValue |= (MemSpaceConfig[i].BaseAddress & 0xFFFFFF);

            status = WritePciConfig(DeviceExtension, MemSpaceRegisters[i], configValue);
            if (!NT_SUCCESS(status)) {
                DbgPrint(DBG_ERROR, "Failed to configure memory space %d: 0x%08X", i, status);
                return status;
            }
            
            DbgPrint(DBG_INFO, "Memory Space %d configured: Base=0x%X, Size=%dKB",
                     i, MemSpaceConfig[i].BaseAddress, 16 << MemSpaceConfig[i].Size);
        }
    }

    // Step 5: Configure DMA Channels
    if (DmaConfig[0].Enable) {
        // DMA Channel 1 (8-bit channels 0,1)
        configValue = DMA_ENABLE | DmaConfig[0].Channel;
        if (DmaConfig[0].TypeF) {
            configValue |= DMA_TYPE_F;
        }
        
        status = WritePciConfig(DeviceExtension, ITE8888_DMA_CHANNEL_01, configValue);
        if (!NT_SUCCESS(status)) {
            DbgPrint(DBG_ERROR, "Failed to configure DMA channels 0-1: 0x%08X", status);
            return status;
        }
        
        DbgPrint(DBG_INFO, "DMA Channel %d configured (8-bit, Type F=%d)", 
                 DmaConfig[0].Channel, DmaConfig[0].TypeF);
    }

    if (DmaConfig[1].Enable) {
        // DMA Channel 5 (16-bit channels 5-7)
        configValue = DMA_ENABLE | DmaConfig[1].Channel;
        if (DmaConfig[1].TypeF) {
            configValue |= DMA_TYPE_F;
        }
        
        status = WritePciConfig(DeviceExtension, ITE8888_DMA_CHANNEL_5, configValue);
        if (!NT_SUCCESS(status)) {
            DbgPrint(DBG_ERROR, "Failed to configure DMA channel 5: 0x%08X", status);
            return status;
        }
        
        DbgPrint(DBG_INFO, "DMA Channel %d configured (16-bit, Type F=%d)", 
                 DmaConfig[1].Channel, DmaConfig[1].TypeF);
    }

    // Step 6: Enable Serial IRQ if configured
    if (ENABLE_SERIAL_IRQ) {
        status = ReadPciConfig(DeviceExtension, ITE8888_RETRY_DISCARD, &configValue);
        if (NT_SUCCESS(status)) {
            // Enable Serial IRQ (implementation depends on specific register bits)
            configValue |= 0x01000000;  // Example bit - adjust based on datasheet
            status = WritePciConfig(DeviceExtension, ITE8888_RETRY_DISCARD, configValue);
            if (NT_SUCCESS(status)) {
                DbgPrint(DBG_INFO, "Serial IRQ enabled (mode %d)", SERIAL_IRQ_MODE);
            }
        }
    }

    // Step 7: Verify configuration by reading back key registers
    status = VerifyConfiguration(DeviceExtension);
    if (!NT_SUCCESS(status)) {
        DbgPrint(DBG_ERROR, "Bridge configuration verification failed: 0x%08X", status);
        return status;
    }

    DbgPrint(DBG_INFO, "ITE8888 bridge configuration completed successfully");
    return STATUS_SUCCESS;
}

/*
 * Reset bridge to default state
 */
NTSTATUS ResetBridge(
    IN PDEVICE_EXTENSION DeviceExtension
)
{
    NTSTATUS status;
    ULONG i;

    PAGED_CODE();

    DbgPrint(DBG_INFO, "Resetting ITE8888 bridge configuration");

    // Disable all I/O spaces
    for (i = 0; i < 6; i++) {
        status = WritePciConfig(DeviceExtension, IoSpaceRegisters[i], 0);
        if (!NT_SUCCESS(status)) {
            DbgPrint(DBG_WARN, "Failed to reset I/O space %d: 0x%08X", i, status);
        }
    }

    // Disable all memory spaces
    for (i = 0; i < 4; i++) {
        status = WritePciConfig(DeviceExtension, MemSpaceRegisters[i], 0);
        if (!NT_SUCCESS(status)) {
            DbgPrint(DBG_WARN, "Failed to reset memory space %d: 0x%08X", i, status);
        }
    }

    // Reset DMA channels
    WritePciConfig(DeviceExtension, ITE8888_DMA_CHANNEL_01, 0);
    WritePciConfig(DeviceExtension, ITE8888_DMA_CHANNEL_5, 0);

    // Reset miscellaneous control
    WritePciConfig(DeviceExtension, ITE8888_MISC_CONTROL, 0);

    DbgPrint(DBG_INFO, "ITE8888 bridge reset completed");
    return STATUS_SUCCESS;
}

/*
 * Verify bridge configuration
 */
NTSTATUS VerifyConfiguration(
    IN PDEVICE_EXTENSION DeviceExtension
)
{
    NTSTATUS status;
    ULONG configValue;
    ULONG i;
    BOOLEAN allGood = TRUE;

    PAGED_CODE();

    DbgPrint(DBG_VERBOSE, "Verifying ITE8888 bridge configuration");

    // Verify PCI command register
    status = ReadPciConfig(DeviceExtension, PCI_COMMAND_REGISTER, &configValue);
    if (NT_SUCCESS(status)) {
        if ((configValue & (PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE)) == 0) {
            DbgPrint(DBG_WARN, "PCI command register not properly enabled: 0x%08X", configValue);
            allGood = FALSE;
        }
    }

    // Verify enabled I/O spaces
    for (i = 0; i < 6; i++) {
        if (IoSpaceConfig[i].Enable) {
            status = ReadPciConfig(DeviceExtension, IoSpaceRegisters[i], &configValue);
            if (NT_SUCCESS(status)) {
                if ((configValue & IO_SPACE_ENABLE) == 0) {
                    DbgPrint(DBG_WARN, "I/O space %d not properly enabled: 0x%08X", i, configValue);
                    allGood = FALSE;
                } else {
                    ULONG baseAddr = configValue & 0xFFFF;
                    if (baseAddr != IoSpaceConfig[i].BaseAddress) {
                        DbgPrint(DBG_WARN, "I/O space %d base address mismatch: expected 0x%X, got 0x%X", 
                                 i, IoSpaceConfig[i].BaseAddress, baseAddr);
                        allGood = FALSE;
                    }
                }
            }
        }
    }

    return allGood ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/*
 * Read PCI configuration register
 */
NTSTATUS ReadPciConfig(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG Offset,
    OUT PULONG Value
)
{
    PCI_SLOT_NUMBER slotNumber;
    ULONG bytesRead;

    if (!Value) {
        return STATUS_INVALID_PARAMETER;
    }

    slotNumber.u.AsULONG = 0;
    slotNumber.u.bits.DeviceNumber = DeviceExtension->DeviceNumber;
    slotNumber.u.bits.FunctionNumber = DeviceExtension->FunctionNumber;

    bytesRead = HalGetBusData(
        PCIConfiguration,
        DeviceExtension->BusNumber,
        slotNumber.u.AsULONG,
        Value,
        sizeof(ULONG)
    );

    return (bytesRead == sizeof(ULONG)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

/*
 * Write PCI configuration register
 */
NTSTATUS WritePciConfig(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN ULONG Offset,
    IN ULONG Value
)
{
    PCI_SLOT_NUMBER slotNumber;
    ULONG bytesWritten;

    slotNumber.u.AsULONG = 0;
    slotNumber.u.bits.DeviceNumber = DeviceExtension->DeviceNumber;
    slotNumber.u.bits.FunctionNumber = DeviceExtension->FunctionNumber;

    bytesWritten = HalSetBusData(
        PCIConfiguration,
        DeviceExtension->BusNumber,
        slotNumber.u.AsULONG,
        &Value,
        sizeof(ULONG)
    );

    return (bytesWritten == sizeof(ULONG)) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}