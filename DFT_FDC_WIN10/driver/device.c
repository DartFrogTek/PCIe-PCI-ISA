#include "driver.h"

static
NTSTATUS
DftFdcAllocateDmaBuffer(
    _Inout_ PDEVICE_CONTEXT Ctx
    )
{
    PHYSICAL_ADDRESS low;
    PHYSICAL_ADDRESS high;
    PHYSICAL_ADDRESS boundary;

    low.QuadPart = 0;
    high.QuadPart = 0xFFFFFFFFull;
    boundary.QuadPart = 0x10000ull;

    Ctx->DmaBuffer = MmAllocateContiguousMemorySpecifyCache(DFTFDC_DMA_BUFFER_BYTES,
                                                            low,
                                                            high,
                                                            boundary,
                                                            MmCached);
    if (Ctx->DmaBuffer == NULL) {
        DftFdcTrace("DMA bounce buffer allocation failed\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Ctx->DmaBuffer, DFTFDC_DMA_BUFFER_BYTES);
    Ctx->DmaPhysical = MmGetPhysicalAddress(Ctx->DmaBuffer);
    Ctx->DmaBufferSize = DFTFDC_DMA_BUFFER_BYTES;
    Ctx->DmaBufferValid = TRUE;

    DftFdcTrace("DMA bounce buffer VA=%p PA=%I64x bytes=%Iu\n",
                Ctx->DmaBuffer,
                Ctx->DmaPhysical.QuadPart,
                Ctx->DmaBufferSize);

    return STATUS_SUCCESS;
}

static
VOID
DftFdcFreeDmaBuffer(
    _Inout_ PDEVICE_CONTEXT Ctx
    )
{
    if (Ctx->DmaBuffer != NULL) {
        MmFreeContiguousMemory(Ctx->DmaBuffer);
        Ctx->DmaBuffer = NULL;
    }
    Ctx->DmaPhysical.QuadPart = 0;
    Ctx->DmaBufferSize = 0;
    Ctx->DmaBufferValid = FALSE;
}

NTSTATUS
DftFdcCreateDevice(
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS status;
    WDF_OBJECT_ATTRIBUTES attributes;
    WDFDEVICE device;
    PDEVICE_CONTEXT ctx;
    UNICODE_STRING ntName;
    UNICODE_STRING symLink;
    WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;

    WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_UNKNOWN);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

    RtlInitUnicodeString(&ntName, DFTFDC_NT_DEVICE_NAME);
    status = WdfDeviceInitAssignName(DeviceInit, &ntName);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("WdfDeviceInitAssignName failed %!STATUS!\n", status);
        return status;
    }

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
    pnpCallbacks.EvtDevicePrepareHardware = DftFdcEvtDevicePrepareHardware;
    pnpCallbacks.EvtDeviceReleaseHardware = DftFdcEvtDeviceReleaseHardware;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);
    attributes.ExecutionLevel = WdfExecutionLevelPassive;

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("WdfDeviceCreate failed %!STATUS!\n", status);
        return status;
    }

    ctx = DftFdcGetContext(device);
    RtlZeroMemory(ctx, sizeof(*ctx));
    ctx->Device = device;
    ctx->FdcBase = DFTFDC_DEFAULT_FDC_BASE;
    ctx->Drive = DFTFDC_DEFAULT_DRIVE;
    ctx->TransferMode = DFTFDC_TRANSFER_PIO;
    KeInitializeEvent(&ctx->IrqEvent, NotificationEvent, FALSE);

    status = WdfWaitLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &ctx->FdcLock);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("WdfWaitLockCreate failed %!STATUS!\n", status);
        return status;
    }

    status = FdcSetGeometry(ctx, DFTFDC_GEOMETRY_360K);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = DftFdcCreateQueue(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = DftFdcCreateInterrupt(device);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("WdfInterruptCreate failed %!STATUS!; continuing without connected interrupt until PnP resources are available\n", status);
        status = STATUS_SUCCESS;
    }

    RtlInitUnicodeString(&symLink, DFTFDC_DOS_DEVICE_NAME);
    status = WdfDeviceCreateSymbolicLink(device, &symLink);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("WdfDeviceCreateSymbolicLink failed %!STATUS!\n", status);
        return status;
    }

    DftFdcTrace("Device created: \\.\\DftFdc0\n");
    return STATUS_SUCCESS;
}

NTSTATUS
DftFdcEvtDevicePrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PDEVICE_CONTEXT ctx = DftFdcGetContext(Device);
    NTSTATUS status;

    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    DftFdcTrace("PrepareHardware\n");

    if (!ctx->DmaBufferValid) {
        status = DftFdcAllocateDmaBuffer(ctx);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    status = It8888QueryBusInterface(Device, ctx);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("IT8888 PCI bus interface query failed %!STATUS!\n", status);
    }

    status = It8888ConfigureFdcPath(ctx);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("It8888ConfigureFdcPath returned %!STATUS!\n", status);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
DftFdcEvtDeviceReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated
    )
{
    PDEVICE_CONTEXT ctx = DftFdcGetContext(Device);

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    DftFdcTrace("ReleaseHardware\n");
    FdcMotorOff(ctx);

    if (ctx->BusInterfaceValid && ctx->BusInterface.InterfaceDereference != NULL) {
        ctx->BusInterface.InterfaceDereference(ctx->BusInterface.Context);
    }
    ctx->BusInterfaceValid = FALSE;

    DftFdcFreeDmaBuffer(ctx);
    return STATUS_SUCCESS;
}
