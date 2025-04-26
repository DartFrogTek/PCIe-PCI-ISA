EFI_STATUS
UseIsaIoProtocol(
    VOID)
{
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    LEGACY_IO_PROTOCOL *LegacyIo;
    UINT8 Data8;
    UINT16 Data16;

    // Find all instances of the Legacy I/O Protocol
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gLegacyIoProtocolGuid,
        NULL,
        &HandleCount,
        &HandleBuffer);

    if (EFI_ERROR(Status) || HandleCount == 0)
    {
        DEBUG((DEBUG_ERROR, "Failed to locate Legacy I/O Protocol: %r\n", Status));
        return Status;
    }

    // Get the first instance of the protocol
    Status = gBS->HandleProtocol(
        HandleBuffer[0],
        &gLegacyIoProtocolGuid,
        (VOID **)&LegacyIo);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to open Legacy I/O Protocol: %r\n", Status));
        gBS->FreePool(HandleBuffer);
        return Status;
    }

    // Example: Read from port 0x60 (keyboard data port)
    Status = LegacyIo->Read(
        LegacyIo,
        0x60,
        1,
        &Data8);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to read from port 0x60: %r\n", Status));
    }
    else
    {
        DEBUG((DEBUG_INFO, "Read value 0x%02x from port 0x60\n", Data8));
    }

    // Example: Write to port 0x80 (POST diagnostic port)
    Data8 = 0xAA; // Arbitrary test value
    Status = LegacyIo->Write(
        LegacyIo,
        0x80,
        1,
        &Data8);

    if (EFI_ERROR(Status))
    {
        DEBUG((DEBUG_ERROR, "Failed to write to port 0x80: %r\n", Status));
    }
    else
    {
        DEBUG((DEBUG_INFO, "Wrote value 0xAA to port 0x80\n"));
    }

    gBS->FreePool(HandleBuffer);
    return EFI_SUCCESS;
}