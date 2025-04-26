EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_HANDLE DriverHandle;
    VOID *DriverBuffer;
    UINTN DriverSize;

    // Load the driver file into memory
    Status = LoadDriverFile(L"IsaIoForwardingDxe.efi", &DriverBuffer, &DriverSize);
    if (EFI_ERROR(Status))
    {
        return Status;
    }
    
    // Load the driver image
    Status = gBS->LoadImage(FALSE, ImageHandle, NULL, DriverBuffer, DriverSize, &DriverHandle);
    if (EFI_ERROR(Status))
    {
        FreePool(DriverBuffer);
        return Status;
    }

    // Start the driver
    Status = gBS->StartImage(DriverHandle, NULL, NULL);
    FreePool(DriverBuffer);

    return Status;
}