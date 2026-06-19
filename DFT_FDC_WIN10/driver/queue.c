#include "driver.h"

static
NTSTATUS
DftFdcCopyToRequest(
    _In_ WDFREQUEST Request,
    _In_ PVOID Source,
    _In_ size_t SourceSize,
    _Out_ size_t* BytesReturned
    )
{
    NTSTATUS status;
    PVOID outBuf;
    size_t outLen;

    status = WdfRequestRetrieveOutputBuffer(Request, SourceSize, &outBuf, &outLen);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    UNREFERENCED_PARAMETER(outLen);
    RtlCopyMemory(outBuf, Source, SourceSize);
    *BytesReturned = SourceSize;
    return STATUS_SUCCESS;
}

static
VOID
DftFdcCompleteFdcResult(
    _In_ WDFREQUEST Request,
    _In_ NTSTATUS CommandStatus,
    _In_opt_ PDFTFDC_FDC_COMMAND_RESULT FdcResult,
    _Inout_ size_t* BytesReturned
    )
{
    NTSTATUS status;
    DFTFDC_FDC_RESULT out;

    RtlZeroMemory(&out, sizeof(out));
    out.Status = (ULONG)CommandStatus;
    if (FdcResult != NULL) {
        out.ResultLength = FdcResult->Length;
        RtlCopyMemory(out.Result, FdcResult->Bytes, min(FdcResult->Length, DFTFDC_MAX_FDC_RESULT_BYTES));
    }

    status = DftFdcCopyToRequest(Request, &out, sizeof(out), BytesReturned);
    WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? *BytesReturned : 0);
}

NTSTATUS
DftFdcCreateQueue(
    _In_ WDFDEVICE Device
    )
{
    NTSTATUS status;
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_OBJECT_ATTRIBUTES attributes;
    PDEVICE_CONTEXT ctx = DftFdcGetContext(Device);

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.EvtIoDeviceControl = DftFdcEvtIoDeviceControl;
    queueConfig.PowerManaged = WdfTrue;

    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ExecutionLevel = WdfExecutionLevelPassive;

    status = WdfIoQueueCreate(Device,
                              &queueConfig,
                              &attributes,
                              &ctx->Queue);
    if (!NT_SUCCESS(status)) {
        DftFdcTrace("WdfIoQueueCreate failed %!STATUS!\n", status);
    }

    return status;
}

VOID
DftFdcEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
    )
{
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT ctx = DftFdcGetContext(device);
    NTSTATUS status = STATUS_SUCCESS;
    size_t bytesReturned = 0;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    switch (IoControlCode) {
    case IOCTL_DFTFDC_GET_VERSION:
    {
        DFTFDC_VERSION_INFO info;
        RtlZeroMemory(&info, sizeof(info));
        info.AbiVersion = DFTFDC_ABI_VERSION;
        info.Major = DFTFDC_VERSION_MAJOR;
        info.Minor = DFTFDC_VERSION_MINOR;
        info.Patch = DFTFDC_VERSION_PATCH;
        info.Build = DFTFDC_VERSION_BUILD;
        RtlCopyMemory(info.Name, "dftfdc-pass1", sizeof("dftfdc-pass1"));
        status = DftFdcCopyToRequest(Request, &info, sizeof(info), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_PROBE_BRIDGE:
    {
        DFTFDC_BRIDGE_INFO info;
        status = It8888ProbeBridge(ctx, &info);
        if (!NT_SUCCESS(status)) {
            info.Status = (ULONG)status;
        }
        status = DftFdcCopyToRequest(Request, &info, sizeof(info), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_PROBE_FDC:
    {
        DFTFDC_FDC_PROBE_INFO info;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        RtlZeroMemory(&info, sizeof(info));
        info.Status = STATUS_SUCCESS;
        info.BasePort = ctx->FdcBase;
        info.MainStatus = FdcReadMsr(ctx);
        info.DigitalInput = FdcReadDir(ctx);
        info.IrqCount = (ULONG)ctx->IrqCount;
        info.TransferMode = ctx->TransferMode;
        WdfWaitLockRelease(ctx->FdcLock);
        status = DftFdcCopyToRequest(Request, &info, sizeof(info), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_RESET_FDC:
    {
        DFTFDC_FDC_RESULT out;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        status = FdcReset(ctx);
        WdfWaitLockRelease(ctx->FdcLock);
        RtlZeroMemory(&out, sizeof(out));
        out.Status = (ULONG)status;
        status = DftFdcCopyToRequest(Request, &out, sizeof(out), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_RECALIBRATE:
    {
        DFTFDC_FDC_COMMAND_RESULT result;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        status = FdcRecalibrate(ctx, &result);
        WdfWaitLockRelease(ctx->FdcLock);
        DftFdcCompleteFdcResult(Request, status, &result, &bytesReturned);
        return;
    }

    case IOCTL_DFTFDC_SEEK:
    {
        PDFTFDC_SEEK_REQUEST seek;
        DFTFDC_FDC_COMMAND_RESULT result;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(*seek), (PVOID*)&seek, NULL);
        if (!NT_SUCCESS(status)) break;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        status = FdcSeek(ctx, seek->Cylinder, seek->Head, &result);
        WdfWaitLockRelease(ctx->FdcLock);
        DftFdcCompleteFdcResult(Request, status, &result, &bytesReturned);
        return;
    }

    case IOCTL_DFTFDC_READ_ID:
    {
        DFTFDC_FDC_COMMAND_RESULT result;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        status = FdcReadId(ctx, 0, &result);
        WdfWaitLockRelease(ctx->FdcLock);
        DftFdcCompleteFdcResult(Request, status, &result, &bytesReturned);
        return;
    }

    case IOCTL_DFTFDC_SET_GEOMETRY:
    {
        PDFTFDC_MEDIA_GEOMETRY_INFO inGeo;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(*inGeo), (PVOID*)&inGeo, NULL);
        if (!NT_SUCCESS(status)) break;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        status = FdcSetGeometry(ctx, inGeo->GeometryId);
        WdfWaitLockRelease(ctx->FdcLock);
        if (!NT_SUCCESS(status)) break;
        status = DftFdcCopyToRequest(Request, &ctx->Geometry, sizeof(ctx->Geometry), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_GET_GEOMETRY:
    {
        status = DftFdcCopyToRequest(Request, &ctx->Geometry, sizeof(ctx->Geometry), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_READ_SECTOR:
    {
        PDFTFDC_READ_SECTOR_REQUEST in;
        DFTFDC_READ_SECTOR_RESULT out;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(*in), (PVOID*)&in, NULL);
        if (!NT_SUCCESS(status)) break;
        WdfWaitLockAcquire(ctx->FdcLock, NULL);
        status = FdcReadSector(ctx, in->Lba, in->UseChs, in->Cylinder, in->Head, in->Sector, &out);
        WdfWaitLockRelease(ctx->FdcLock);
        if (!NT_SUCCESS(status)) {
            /* Return the result structure anyway; it contains the failing NTSTATUS. */
        }
        status = DftFdcCopyToRequest(Request, &out, sizeof(out), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_GET_IRQ_COUNT:
    {
        DFTFDC_IRQ_COUNT_INFO info;
        info.IrqCount = (ULONG)ctx->IrqCount;
        status = DftFdcCopyToRequest(Request, &info, sizeof(info), &bytesReturned);
        WdfRequestCompleteWithInformation(Request, status, NT_SUCCESS(status) ? bytesReturned : 0);
        return;
    }

    case IOCTL_DFTFDC_SET_TRANSFER_MODE:
    {
        PDFTFDC_TRANSFER_MODE_REQUEST in;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(*in), (PVOID*)&in, NULL);
        if (!NT_SUCCESS(status)) break;
        if (in->TransferMode != DFTFDC_TRANSFER_PIO && in->TransferMode != DFTFDC_TRANSFER_IT8888_DDMA) {
            status = STATUS_INVALID_PARAMETER;
            break;
        }
        ctx->TransferMode = (UCHAR)in->TransferMode;
        status = STATUS_SUCCESS;
        WdfRequestComplete(Request, status);
        return;
    }

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    WdfRequestCompleteWithInformation(Request, status, 0);
}
