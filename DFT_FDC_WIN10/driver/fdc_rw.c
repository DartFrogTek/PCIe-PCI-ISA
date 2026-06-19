#include "driver.h"

NTSTATUS
FdcReadSectorPio(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG Cylinder,
    _In_ ULONG Head,
    _In_ ULONG Sector,
    _Out_writes_bytes_(DFTFDC_SECTOR_BYTES) PUCHAR Data,
    _Out_ PDFTFDC_FDC_COMMAND_RESULT FdcResult
    )
{
    NTSTATUS status;
    ULONG i;

    RtlZeroMemory(Data, DFTFDC_SECTOR_BYTES);
    RtlZeroMemory(FdcResult, sizeof(*FdcResult));
    KeClearEvent(&Ctx->IrqEvent);

    status = FdcSpecify(Ctx, TRUE);
    if (!NT_SUCCESS(status)) return status;

    status = FdcWriteFifo(Ctx, FDC_CMD_READ_DATA);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, (UCHAR)(((Head & 1u) << 2) | (Ctx->Drive & 0x03u)));
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, (UCHAR)Cylinder);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, (UCHAR)Head);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, (UCHAR)Sector);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, FDC_SECTOR_SIZE_CODE_512);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, (UCHAR)Ctx->Geometry.SectorsPerTrack);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, Ctx->Geometry.GapLength);
    if (!NT_SUCCESS(status)) return status;
    status = FdcWriteFifo(Ctx, 0xFF);
    if (!NT_SUCCESS(status)) return status;

    for (i = 0; i < DFTFDC_SECTOR_BYTES; i++) {
        status = FdcReadFifo(Ctx, &Data[i]);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    (VOID)FdcWaitForIrq(Ctx, 5000);

    status = FdcDrainResult(Ctx, FdcResult, 1000);
    if (!NT_SUCCESS(status)) return status;

    if (FdcResult->Length < 7) {
        return STATUS_UNSUCCESSFUL;
    }

    if ((FdcResult->Bytes[0] & FDC_ST0_INTERRUPT_CODE_MASK) != 0 ||
        FdcResult->Bytes[1] != 0 ||
        FdcResult->Bytes[2] != 0) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
FdcReadSector(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG Lba,
    _In_ ULONG UseChs,
    _In_ ULONG Cylinder,
    _In_ ULONG Head,
    _In_ ULONG Sector,
    _Out_ PDFTFDC_READ_SECTOR_RESULT Result
    )
{
    NTSTATUS status;
    DFTFDC_FDC_COMMAND_RESULT fdcResult;

    RtlZeroMemory(Result, sizeof(*Result));

    if (!UseChs) {
        status = FdcLbaToChs(&Ctx->Geometry, Lba, &Cylinder, &Head, &Sector);
        if (!NT_SUCCESS(status)) {
            Result->Status = (ULONG)status;
            return status;
        }
    } else {
        if (Cylinder >= Ctx->Geometry.Cylinders ||
            Head >= Ctx->Geometry.Heads ||
            Sector == 0 ||
            Sector > Ctx->Geometry.SectorsPerTrack) {
            status = STATUS_INVALID_PARAMETER;
            Result->Status = (ULONG)status;
            return status;
        }
        Lba = ((Cylinder * Ctx->Geometry.Heads + Head) * Ctx->Geometry.SectorsPerTrack) + (Sector - 1);
    }

    Result->GeometryId = Ctx->Geometry.GeometryId;
    Result->Lba = Lba;
    Result->Cylinder = Cylinder;
    Result->Head = Head;
    Result->Sector = Sector;

    status = FdcSeek(Ctx, Cylinder, Head, NULL);
    if (!NT_SUCCESS(status)) {
        Result->Status = (ULONG)status;
        return status;
    }

    if (Ctx->TransferMode == DFTFDC_TRANSFER_IT8888_DDMA) {
        status = FdcReadSectorDdma(Ctx, Cylinder, Head, Sector, Result->Data, &fdcResult);
    } else {
        status = FdcReadSectorPio(Ctx, Cylinder, Head, Sector, Result->Data, &fdcResult);
    }

    Result->Status = (ULONG)status;
    Result->ResultLength = fdcResult.Length;
    RtlCopyMemory(Result->Result, fdcResult.Bytes, min(fdcResult.Length, DFTFDC_MAX_FDC_RESULT_BYTES));

    return status;
}
