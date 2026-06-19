#include "driver.h"

NTSTATUS
FdcReadSectorDdma(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG Cylinder,
    _In_ ULONG Head,
    _In_ ULONG Sector,
    _Out_writes_bytes_(DFTFDC_SECTOR_BYTES) PUCHAR Data,
    _Out_ PDFTFDC_FDC_COMMAND_RESULT FdcResult
    )
{
    NTSTATUS status;

    if (!Ctx->DmaBufferValid || Ctx->DmaBufferSize < DFTFDC_SECTOR_BYTES) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Ctx->DmaBuffer, DFTFDC_SECTOR_BYTES);
    RtlZeroMemory(FdcResult, sizeof(*FdcResult));
    KeClearEvent(&Ctx->IrqEvent);

    status = FdcSpecify(Ctx, FALSE);
    if (!NT_SUCCESS(status)) return status;

    status = It8888ProgramDmaChannel2(Ctx, TRUE, Ctx->DmaPhysical, DFTFDC_SECTOR_BYTES);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = FdcWriteFifo(Ctx, FDC_CMD_READ_DATA);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)(((Head & 1u) << 2) | (Ctx->Drive & 0x03u)));
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)Cylinder);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)Head);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)Sector);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, FDC_SECTOR_SIZE_CODE_512);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, (UCHAR)Ctx->Geometry.SectorsPerTrack);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, Ctx->Geometry.GapLength);
    if (!NT_SUCCESS(status)) goto Done;
    status = FdcWriteFifo(Ctx, 0xFF);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcWaitForIrq(Ctx, 5000);
    if (!NT_SUCCESS(status)) goto Done;

    status = FdcDrainResult(Ctx, FdcResult, 1000);
    if (!NT_SUCCESS(status)) goto Done;

    if (FdcResult->Length < 7) {
        status = STATUS_UNSUCCESSFUL;
        goto Done;
    }

    if ((FdcResult->Bytes[0] & FDC_ST0_INTERRUPT_CODE_MASK) != 0 ||
        FdcResult->Bytes[1] != 0 ||
        FdcResult->Bytes[2] != 0) {
        status = STATUS_UNSUCCESSFUL;
        goto Done;
    }

    RtlCopyMemory(Data, Ctx->DmaBuffer, DFTFDC_SECTOR_BYTES);

Done:
    It8888ClearDmaChannel2(Ctx);
    return status;
}
