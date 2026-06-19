#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ntdddisk.h>
#include <ntddstor.h>
#include "public.h"
#include "trace.h"
#include "fdc_regs.h"

#define DFTFDC_NT_DEVICE_NAME   L"\\Device\\DftFdc0"
#define DFTFDC_DOS_DEVICE_NAME  L"\\DosDevices\\DftFdc0"

#define DFTFDC_DEFAULT_FDC_BASE FDC_PRIMARY_BASE
#define DFTFDC_DEFAULT_DRIVE    0u
#define DFTFDC_DMA_BUFFER_BYTES 4096u

typedef struct _DFTFDC_FDC_COMMAND_RESULT {
    ULONG Length;
    UCHAR Bytes[DFTFDC_MAX_FDC_RESULT_BYTES];
} DFTFDC_FDC_COMMAND_RESULT, *PDFTFDC_FDC_COMMAND_RESULT;

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE Queue;
    WDFWAITLOCK FdcLock;

    WDFINTERRUPT Interrupt;
    BOOLEAN InterruptCreated;
    KEVENT IrqEvent;
    volatile LONG IrqCount;

    BUS_INTERFACE_STANDARD BusInterface;
    BOOLEAN BusInterfaceValid;

    USHORT FdcBase;
    UCHAR Drive;
    UCHAR TransferMode;
    UCHAR LastDor;
    DFTFDC_MEDIA_GEOMETRY_INFO Geometry;

    PVOID DmaBuffer;
    PHYSICAL_ADDRESS DmaPhysical;
    SIZE_T DmaBufferSize;
    BOOLEAN DmaBufferValid;

    ULONG LastHwStatus;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DftFdcGetContext)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD DftFdcEvtDeviceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE DftFdcEvtDevicePrepareHardware;
EVT_WDF_DEVICE_RELEASE_HARDWARE DftFdcEvtDeviceReleaseHardware;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL DftFdcEvtIoDeviceControl;
EVT_WDF_INTERRUPT_ISR DftFdcEvtInterruptIsr;
EVT_WDF_INTERRUPT_DPC DftFdcEvtInterruptDpc;

NTSTATUS DftFdcCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);
NTSTATUS DftFdcCreateQueue(_In_ WDFDEVICE Device);
NTSTATUS DftFdcCreateInterrupt(_In_ WDFDEVICE Device);

NTSTATUS It8888QueryBusInterface(_In_ WDFDEVICE Device, _Inout_ PDEVICE_CONTEXT Ctx);
NTSTATUS It8888ReadPciConfig(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG Offset, _Out_writes_bytes_(Length) PVOID Buffer, _In_ ULONG Length);
NTSTATUS It8888ProbeBridge(_In_ PDEVICE_CONTEXT Ctx, _Out_ PDFTFDC_BRIDGE_INFO Info);
NTSTATUS It8888ConfigureFdcPath(_In_ PDEVICE_CONTEXT Ctx);
NTSTATUS It8888ProgramDmaChannel2(_In_ PDEVICE_CONTEXT Ctx, _In_ BOOLEAN DeviceToMemory, _In_ PHYSICAL_ADDRESS BufferPhysical, _In_ ULONG Length);
VOID It8888ClearDmaChannel2(_In_ PDEVICE_CONTEXT Ctx);

UCHAR FdcReadPort(_In_ PDEVICE_CONTEXT Ctx, _In_ USHORT Offset);
VOID FdcWritePort(_In_ PDEVICE_CONTEXT Ctx, _In_ USHORT Offset, _In_ UCHAR Value);
UCHAR FdcReadMsr(_In_ PDEVICE_CONTEXT Ctx);
UCHAR FdcReadDir(_In_ PDEVICE_CONTEXT Ctx);
VOID FdcWriteDor(_In_ PDEVICE_CONTEXT Ctx, _In_ UCHAR Value);
VOID FdcWriteCcr(_In_ PDEVICE_CONTEXT Ctx, _In_ UCHAR Value);
NTSTATUS FdcWaitRqm(_In_ PDEVICE_CONTEXT Ctx, _In_ BOOLEAN WantDio, _In_ ULONG TimeoutMs);
NTSTATUS FdcWriteFifo(_In_ PDEVICE_CONTEXT Ctx, _In_ UCHAR Value);
NTSTATUS FdcReadFifo(_In_ PDEVICE_CONTEXT Ctx, _Out_ PUCHAR Value);
NTSTATUS FdcDrainResult(_In_ PDEVICE_CONTEXT Ctx, _Out_ PDFTFDC_FDC_COMMAND_RESULT Result, _In_ ULONG TimeoutMs);
NTSTATUS FdcWaitForIrq(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG TimeoutMs);

NTSTATUS FdcSetGeometry(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG GeometryId);
NTSTATUS FdcGetGeometryById(_In_ ULONG GeometryId, _Out_ PDFTFDC_MEDIA_GEOMETRY_INFO Geometry);
NTSTATUS FdcLbaToChs(_In_ const DFTFDC_MEDIA_GEOMETRY_INFO* Geometry, _In_ ULONG Lba, _Out_ PULONG Cylinder, _Out_ PULONG Head, _Out_ PULONG Sector);

NTSTATUS FdcSpecify(_In_ PDEVICE_CONTEXT Ctx, _In_ BOOLEAN NonDmaMode);
NTSTATUS FdcSenseInterrupt(_In_ PDEVICE_CONTEXT Ctx, _Out_ PUCHAR St0, _Out_ PUCHAR Pcn);
NTSTATUS FdcReset(_In_ PDEVICE_CONTEXT Ctx);
NTSTATUS FdcMotorOn(_In_ PDEVICE_CONTEXT Ctx);
VOID FdcMotorOff(_In_ PDEVICE_CONTEXT Ctx);
NTSTATUS FdcRecalibrate(_In_ PDEVICE_CONTEXT Ctx, _Out_opt_ PDFTFDC_FDC_COMMAND_RESULT Result);
NTSTATUS FdcSeek(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG Cylinder, _In_ ULONG Head, _Out_opt_ PDFTFDC_FDC_COMMAND_RESULT Result);
NTSTATUS FdcReadId(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG Head, _Out_ PDFTFDC_FDC_COMMAND_RESULT Result);
NTSTATUS FdcReadSector(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG Lba, _In_ ULONG UseChs, _In_ ULONG Cylinder, _In_ ULONG Head, _In_ ULONG Sector, _Out_ PDFTFDC_READ_SECTOR_RESULT Result);
NTSTATUS FdcReadSectorPio(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG Cylinder, _In_ ULONG Head, _In_ ULONG Sector, _Out_writes_bytes_(DFTFDC_SECTOR_BYTES) PUCHAR Data, _Out_ PDFTFDC_FDC_COMMAND_RESULT FdcResult);
NTSTATUS FdcReadSectorDdma(_In_ PDEVICE_CONTEXT Ctx, _In_ ULONG Cylinder, _In_ ULONG Head, _In_ ULONG Sector, _Out_writes_bytes_(DFTFDC_SECTOR_BYTES) PUCHAR Data, _Out_ PDFTFDC_FDC_COMMAND_RESULT FdcResult);
