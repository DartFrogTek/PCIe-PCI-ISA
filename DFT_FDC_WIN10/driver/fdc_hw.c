#include "driver.h"

UCHAR
FdcReadPort(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ USHORT Offset
    )
{
    return READ_PORT_UCHAR((PUCHAR)(ULONG_PTR)(Ctx->FdcBase + Offset));
}

VOID
FdcWritePort(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ USHORT Offset,
    _In_ UCHAR Value
    )
{
    WRITE_PORT_UCHAR((PUCHAR)(ULONG_PTR)(Ctx->FdcBase + Offset), Value);
}

UCHAR
FdcReadMsr(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    return FdcReadPort(Ctx, FDC_REG_MSR);
}

UCHAR
FdcReadDir(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    return FdcReadPort(Ctx, FDC_REG_DIR);
}

VOID
FdcWriteDor(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ UCHAR Value
    )
{
    Ctx->LastDor = Value;
    FdcWritePort(Ctx, FDC_REG_DOR, Value);
}

VOID
FdcWriteCcr(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ UCHAR Value
    )
{
    FdcWritePort(Ctx, FDC_REG_CCR, Value);
}

NTSTATUS
FdcWaitRqm(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ BOOLEAN WantDio,
    _In_ ULONG TimeoutMs
    )
{
    ULONG loops;

    /*
     * Data-phase PIO at 250/500 kbps cannot afford millisecond sleeps between
     * FIFO polls. Poll in 10 us slices. This is pass-1 diagnostic code and all
     * callers run at PASSIVE_LEVEL through the serialized KMDF queue.
     */
    loops = TimeoutMs * 100u;
    if (loops == 0) {
        loops = 1;
    }

    while (loops-- != 0) {
        UCHAR msr = FdcReadMsr(Ctx);
        if ((msr & FDC_MSR_RQM) != 0) {
            BOOLEAN dio = ((msr & FDC_MSR_DIO) != 0) ? TRUE : FALSE;
            if (dio == WantDio) {
                return STATUS_SUCCESS;
            }
        }
        KeStallExecutionProcessor(10);
    }

    return STATUS_IO_TIMEOUT;
}

NTSTATUS
FdcWriteFifo(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ UCHAR Value
    )
{
    NTSTATUS status;

    status = FdcWaitRqm(Ctx, FALSE, 1000);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    FdcWritePort(Ctx, FDC_REG_FIFO, Value);
    return STATUS_SUCCESS;
}

NTSTATUS
FdcReadFifo(
    _In_ PDEVICE_CONTEXT Ctx,
    _Out_ PUCHAR Value
    )
{
    NTSTATUS status;

    status = FdcWaitRqm(Ctx, TRUE, 1000);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    *Value = FdcReadPort(Ctx, FDC_REG_FIFO);
    return STATUS_SUCCESS;
}

NTSTATUS
FdcDrainResult(
    _In_ PDEVICE_CONTEXT Ctx,
    _Out_ PDFTFDC_FDC_COMMAND_RESULT Result,
    _In_ ULONG TimeoutMs
    )
{
    ULONG waited;
    LARGE_INTEGER delay;

    RtlZeroMemory(Result, sizeof(*Result));
    delay.QuadPart = -1000 * 10; /* 1 ms */

    for (waited = 0; waited < TimeoutMs; waited++) {
        UCHAR msr = FdcReadMsr(Ctx);

        if ((msr & FDC_MSR_RQM) != 0 && (msr & FDC_MSR_DIO) != 0) {
            if (Result->Length >= DFTFDC_MAX_FDC_RESULT_BYTES) {
                return STATUS_BUFFER_OVERFLOW;
            }
            Result->Bytes[Result->Length++] = FdcReadPort(Ctx, FDC_REG_FIFO);
            continue;
        }

        if ((msr & FDC_MSR_CB) == 0) {
            return STATUS_SUCCESS;
        }

        KeDelayExecutionThread(KernelMode, FALSE, &delay);
    }

    return STATUS_IO_TIMEOUT;
}

NTSTATUS
FdcWaitForIrq(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG TimeoutMs
    )
{
    LARGE_INTEGER timeout;
    NTSTATUS status;

    timeout.QuadPart = -((LONGLONG)TimeoutMs) * 10000ll;
    status = KeWaitForSingleObject(&Ctx->IrqEvent,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   &timeout);
    if (status == STATUS_WAIT_0) {
        KeClearEvent(&Ctx->IrqEvent);
        return STATUS_SUCCESS;
    }

    return STATUS_IO_TIMEOUT;
}
