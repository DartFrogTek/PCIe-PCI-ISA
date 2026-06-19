#include "driver.h"

static
VOID
FdcSmallDelayMs(
    _In_ ULONG Ms
    )
{
    LARGE_INTEGER delay;
    delay.QuadPart = -((LONGLONG)Ms) * 10000ll;
    KeDelayExecutionThread(KernelMode, FALSE, &delay);
}

NTSTATUS
FdcSpecify(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ BOOLEAN NonDmaMode
    )
{
    NTSTATUS status;
    UCHAR b2;

    /* Conservative defaults: SRT=6ms-ish, HUT=16ms-ish, HLT=16ms-ish. */
    b2 = (UCHAR)(0x02u << 1);
    if (NonDmaMode) {
        b2 |= 0x01u;
    }

    status = FdcWriteFifo(Ctx, FDC_CMD_SPECIFY);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, 0xDF);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, b2);
    if (!NT_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

NTSTATUS
FdcSenseInterrupt(
    _In_ PDEVICE_CONTEXT Ctx,
    _Out_ PUCHAR St0,
    _Out_ PUCHAR Pcn
    )
{
    NTSTATUS status;

    status = FdcWriteFifo(Ctx, FDC_CMD_SENSE_INTERRUPT);
    if (!NT_SUCCESS(status)) return status;
    status = FdcReadFifo(Ctx, St0);
    if (!NT_SUCCESS(status)) return status;
    status = FdcReadFifo(Ctx, Pcn);
    if (!NT_SUCCESS(status)) return status;

    return STATUS_SUCCESS;
}

NTSTATUS
FdcReset(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    NTSTATUS status;
    ULONG i;

    DftFdcTrace("FDC reset, rate=%lu kbps\n", Ctx->Geometry.DataRateKbps);

    FdcWriteDor(Ctx, 0x00);
    FdcSmallDelayMs(20);
    KeClearEvent(&Ctx->IrqEvent);
    FdcWriteCcr(Ctx, Ctx->Geometry.RateSelect);
    FdcWritePort(Ctx, FDC_REG_DSR, Ctx->Geometry.RateSelect);
    FdcWriteDor(Ctx, FDC_DOR_RESET | FDC_DOR_DMA_IRQ | (Ctx->Drive & FDC_DOR_DRIVE_MASK));

    /* Reset normally raises an interrupt. Some bridge setups may not route it yet. */
    (VOID)FdcWaitForIrq(Ctx, 1000);

    for (i = 0; i < 4; i++) {
        UCHAR st0 = 0, pcn = 0;
        status = FdcSenseInterrupt(Ctx, &st0, &pcn);
        if (!NT_SUCCESS(status)) {
            break;
        }
        DftFdcTrace("Sense after reset[%lu]: ST0=%02x PCN=%02x\n", i, st0, pcn);
    }

    status = FdcSpecify(Ctx, Ctx->TransferMode == DFTFDC_TRANSFER_PIO ? TRUE : FALSE);
    return status;
}

NTSTATUS
FdcMotorOn(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    UCHAR motorBit;
    UCHAR dor;

    motorBit = (UCHAR)(FDC_DOR_MOTOR0 << (Ctx->Drive & 0x03));
    dor = (UCHAR)(FDC_DOR_RESET | FDC_DOR_DMA_IRQ | motorBit | (Ctx->Drive & FDC_DOR_DRIVE_MASK));
    FdcWriteDor(Ctx, dor);
    FdcSmallDelayMs(500);
    return STATUS_SUCCESS;
}

VOID
FdcMotorOff(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    FdcWriteDor(Ctx, (UCHAR)(FDC_DOR_RESET | FDC_DOR_DMA_IRQ | (Ctx->Drive & FDC_DOR_DRIVE_MASK)));
}

NTSTATUS
FdcRecalibrate(
    _In_ PDEVICE_CONTEXT Ctx,
    _Out_opt_ PDFTFDC_FDC_COMMAND_RESULT Result
    )
{
    NTSTATUS status;
    UCHAR st0 = 0;
    UCHAR pcn = 0;

    KeClearEvent(&Ctx->IrqEvent);
    status = FdcMotorOn(Ctx);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWriteFifo(Ctx, FDC_CMD_RECALIBRATE);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, Ctx->Drive & 0x03u);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWaitForIrq(Ctx, 3000);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcSenseInterrupt(Ctx, &st0, &pcn);
    if (!NT_SUCCESS(status)) goto Done;

    if (Result != NULL) {
        RtlZeroMemory(Result, sizeof(*Result));
        Result->Length = 2;
        Result->Bytes[0] = st0;
        Result->Bytes[1] = pcn;
    }

    if ((st0 & FDC_ST0_INTERRUPT_CODE_MASK) != 0 || pcn != 0) {
        status = STATUS_UNSUCCESSFUL;
    }

Done:
    return status;
}

NTSTATUS
FdcSeek(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG Cylinder,
    _In_ ULONG Head,
    _Out_opt_ PDFTFDC_FDC_COMMAND_RESULT Result
    )
{
    NTSTATUS status;
    UCHAR st0 = 0;
    UCHAR pcn = 0;

    if (Cylinder >= Ctx->Geometry.Cylinders || Head >= Ctx->Geometry.Heads) {
        return STATUS_INVALID_PARAMETER;
    }

    KeClearEvent(&Ctx->IrqEvent);
    status = FdcMotorOn(Ctx);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWriteFifo(Ctx, FDC_CMD_SEEK);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)(((Head & 1u) << 2) | (Ctx->Drive & 0x03u)));
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)Cylinder);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWaitForIrq(Ctx, 3000);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcSenseInterrupt(Ctx, &st0, &pcn);
    if (!NT_SUCCESS(status)) goto Done;

    if (Result != NULL) {
        RtlZeroMemory(Result, sizeof(*Result));
        Result->Length = 2;
        Result->Bytes[0] = st0;
        Result->Bytes[1] = pcn;
    }

    if ((st0 & FDC_ST0_INTERRUPT_CODE_MASK) != 0 || pcn != (UCHAR)Cylinder) {
        status = STATUS_UNSUCCESSFUL;
    }

Done:
    return status;
}

NTSTATUS
FdcReadId(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG Head,
    _Out_ PDFTFDC_FDC_COMMAND_RESULT Result
    )
{
    NTSTATUS status;

    if (Head >= Ctx->Geometry.Heads) {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(Result, sizeof(*Result));
    KeClearEvent(&Ctx->IrqEvent);

    status = FdcMotorOn(Ctx);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWriteFifo(Ctx, FDC_CMD_READ_ID);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)(((Head & 1u) << 2) | (Ctx->Drive & 0x03u)));
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWaitForIrq(Ctx, 3000);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcDrainResult(Ctx, Result, 1000);
    if (!NT_SUCCESS(status)) goto Done;

    if (Result->Length < 7) {
        status = STATUS_UNSUCCESSFUL;
    }

Done:
    return status;
}
