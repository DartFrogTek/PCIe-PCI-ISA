/**
  Example function demonstrating how to use the Extended Legacy I/O Protocol for DMA.

  @param None

  @retval EFI_SUCCESS The operation completed successfully
  @retval Others An error occurred
*/
EFI_STATUS
DemoUseDmaProtocol(
    VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    LEGACY_IO_PROTOCOL_EX *LegacyIoEx;
    LEGACY_DMA_BUFFER DmaBuffer;
    UINT16 BytesRemaining;
    BOOLEAN IsActive;
    UINT8 Channel;

    // Find all instances of the Extended Legacy I/O Protocol
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gLegacyIoProtocolExGuid,
        NULL,
        &HandleCount,
        &HandleBuffer);

    if (EFI_ERROR(Status) || HandleCount == 0)
    {
        DEBUG((DEBUG_ERROR, "Failed to locate Extended Legacy I/O Protocol: %r\n", Status));
        return Status;
    }

    // Get the first instance of the protocol
    Status = gBS->HandleProtocol(
        HandleBuffer[0],
        &gLegacyIoProtocolExGuid,
        (VOID **)&LegacyIoEx);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to open Extended Legacy I/O Protocol: %r\n", Status));
        gBS->FreePool(HandleBuffer);
        return Status;
    }

    // Allocate a DMA buffer (4KB)
    Status = LegacyIoEx->AllocateDmaBuffer(
        LegacyIoEx,
        4096,
        &DmaBuffer);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to allocate DMA buffer: %r\n", Status));
        gBS->FreePool(HandleBuffer);
        return Status;
    }

    DEBUG((DEBUG_INFO, "DMA buffer allocated at physical address 0x%x\n",
           (UINT32)DmaBuffer.PhysicalAddress));

    // Fill the buffer with a test pattern
    for (UINTN i = 0; i < DmaBuffer.Length; i++)
    {
        ((UINT8 *)DmaBuffer.Buffer)[i] = (UINT8)(i & 0xFF);
    }

    // Choose a DMA channel - for this example, use channel 1 (8-bit)
    Channel = 1;

    // Program the DMA channel for a memory-to-I/O operation (read from memory)
    Status = LegacyIoEx->ProgramDmaChannel(
        LegacyIoEx,
        Channel,
        DMA_MODE_READ, // Read from memory = write to device
        &DmaBuffer,
        256,  // Transfer 256 bytes
        FALSE // No auto-initialize
    );

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to program DMA channel: %r\n", Status));
        LegacyIoEx->FreeDmaBuffer(LegacyIoEx, &DmaBuffer);
        gBS->FreePool(HandleBuffer);
        return Status;
    }

    // Start the DMA transfer
    Status = LegacyIoEx->StartDma(
        LegacyIoEx,
        Channel);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to start DMA transfer: %r\n", Status));
        LegacyIoEx->FreeDmaBuffer(LegacyIoEx, &DmaBuffer);
        gBS->FreePool(HandleBuffer);
        return Status;
    }

    // In a real application, you would typically wait for the device to signal
    // that the DMA transfer is complete, but for this example, we'll just
    // check the status a few times

    for (UINTN i = 0; i < 10; i++)
    {
        // Wait a bit
        gBS->Stall(10000); // 10ms

        // Check the DMA status
        Status = LegacyIoEx->GetDmaStatus(
            LegacyIoEx,
            Channel,
            &BytesRemaining,
            &IsActive);

        if (EFI_ERROR(Status))
        {
            DEBUG((DEBUG_ERROR, "Failed to get DMA status: %r\n", Status));
            break;
        }

        DEBUG((DEBUG_INFO, "DMA transfer: %d bytes remaining, %a\n",
               BytesRemaining, IsActive ? "active" : "inactive"));

        // If the transfer is no longer active, we're done
        if (!IsActive)
        {
            break;
        }
    }

    // Stop the DMA transfer (even if it's already done)
    Status = LegacyIoEx->StopDma(
        LegacyIoEx,
        Channel);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to stop DMA transfer: %r\n", Status));
    }

    // Free the DMA buffer
    Status = LegacyIoEx->FreeDmaBuffer(
        LegacyIoEx,
        &DmaBuffer);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to free DMA buffer: %r\n", Status));
    }

    gBS->FreePool(HandleBuffer);
    return EFI_SUCCESS;
}