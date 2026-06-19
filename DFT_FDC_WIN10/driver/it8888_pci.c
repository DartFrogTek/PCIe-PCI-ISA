#include "driver.h"

/*
 * Keep this local instead of depending on wdmguid.h/Wdmguid.lib so the
 * direct WDK build stays self-contained like the IT8888 manual build path.
 * GUID_BUS_INTERFACE_STANDARD = {496B8280-6F25-11D0-BEAF-08002BE2092F}
 */
static const GUID DFTFDC_GUID_BUS_INTERFACE_STANDARD =
    { 0x496B8280, 0x6F25, 0x11D0, { 0xBE, 0xAF, 0x08, 0x00, 0x2B, 0xE2, 0x09, 0x2F } };

NTSTATUS
It8888QueryBusInterface(
    _In_ WDFDEVICE Device,
    _Inout_ PDEVICE_CONTEXT Ctx
    )
{
    NTSTATUS status;

    RtlZeroMemory(&Ctx->BusInterface, sizeof(Ctx->BusInterface));
    Ctx->BusInterfaceValid = FALSE;

    status = WdfFdoQueryForInterface(Device,
                                     &DFTFDC_GUID_BUS_INTERFACE_STANDARD,
                                     (PINTERFACE)&Ctx->BusInterface,
                                     sizeof(BUS_INTERFACE_STANDARD),
                                     1,
                                     NULL);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Ctx->BusInterfaceValid = TRUE;
    return STATUS_SUCCESS;
}

NTSTATUS
It8888ReadPciConfig(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG Offset,
    _Out_writes_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length
    )
{
    ULONG read;

    if (!Ctx->BusInterfaceValid || Ctx->BusInterface.GetBusData == NULL) {
        return STATUS_DEVICE_NOT_READY;
    }

    RtlZeroMemory(Buffer, Length);

    read = Ctx->BusInterface.GetBusData(Ctx->BusInterface.Context,
                                        PCI_WHICHSPACE_CONFIG,
                                        Buffer,
                                        Offset,
                                        Length);
    if (read != Length) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
It8888ProbeBridge(
    _In_ PDEVICE_CONTEXT Ctx,
    _Out_ PDFTFDC_BRIDGE_INFO Info
    )
{
    NTSTATUS status;
    UCHAR cfg[0x40];

    RtlZeroMemory(Info, sizeof(*Info));
    Info->BusInterfaceValid = Ctx->BusInterfaceValid ? 1u : 0u;

    status = It8888ReadPciConfig(Ctx, 0, cfg, sizeof(cfg));
    Info->Status = (ULONG)status;
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Info->VendorId = (USHORT)(cfg[0] | ((USHORT)cfg[1] << 8));
    Info->DeviceId = (USHORT)(cfg[2] | ((USHORT)cfg[3] << 8));
    Info->CommandStatus = (ULONG)(cfg[4] | ((ULONG)cfg[5] << 8) | ((ULONG)cfg[6] << 16) | ((ULONG)cfg[7] << 24));
    Info->RevisionId = cfg[8];
    Info->ProgIf = cfg[9];
    Info->SubClass = cfg[10];
    Info->ClassCode = cfg[11];
    Info->HeaderType = cfg[0x0E];

    return STATUS_SUCCESS;
}
