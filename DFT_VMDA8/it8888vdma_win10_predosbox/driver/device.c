#include "it8888.h"

NTSTATUS It8888EvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit) {
  UNREFERENCED_PARAMETER(Driver);
  NTSTATUS status;
  WDF_OBJECT_ATTRIBUTES attrs;
  WDFDEVICE device;
  PDEVICE_CONTEXT ctx;
  WDF_IO_QUEUE_CONFIG qcfg;
  WDF_INTERRUPT_CONFIG icfg;
  WDF_DMA_ENABLER_CONFIG dmacfg;
  UNICODE_STRING sym;

  WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_IT8888VDMA);
  WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoBuffered);

  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, DEVICE_CONTEXT);
  status = WdfDeviceCreate(&DeviceInit, &attrs, &device);
  if (!NT_SUCCESS(status))
    return status;

  ctx = DeviceGetContext(device);
  RtlZeroMemory(ctx, sizeof(*ctx));
  ctx->Device = device;
  ctx->VendorId = IT8888_VENDOR_ID_DEFAULT;
  ctx->DeviceId = IT8888_DEVICE_ID_DEFAULT;
  KeInitializeEvent(&ctx->IrqEvent, NotificationEvent, FALSE);

  WDF_OBJECT_ATTRIBUTES lockAttrs;
  WDF_OBJECT_ATTRIBUTES_INIT(&lockAttrs);
  lockAttrs.ParentObject = device;
  status = WdfWaitLockCreate(&lockAttrs, &ctx->HwLock);
  if (!NT_SUCCESS(status))
    return status;

  It8888TraceInit(ctx);
  Vdma8237Reset(ctx);

  RtlInitUnicodeString(&sym, IT8888_DOS_DEVICE_NAME);
  status = WdfDeviceCreateSymbolicLink(device, &sym);
  if (!NT_SUCCESS(status))
    return status;

  WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&qcfg, WdfIoQueueDispatchSequential);
  qcfg.EvtIoDeviceControl = It8888EvtIoDeviceControl;
  status =
      WdfIoQueueCreate(device, &qcfg, WDF_NO_OBJECT_ATTRIBUTES, &ctx->Queue);
  if (!NT_SUCCESS(status))
    return status;

  WDF_DMA_ENABLER_CONFIG_INIT(
      &dmacfg, WdfDmaProfilePacket64,
      0x1000000); // 16MB max transfer cap for diagnostics
  status = WdfDmaEnablerCreate(device, &dmacfg, WDF_NO_OBJECT_ATTRIBUTES,
                               &ctx->DmaEnabler);
  if (!NT_SUCCESS(status)) {
    // Keep driver usable for cfg/PIO even if DMA enabler fails; dma-alloc will
    // report failure.
    ctx->DmaEnabler = NULL;
    It8888Trace(ctx, IT8888_TRACE_ERROR, 0xD001, (ULONGLONG)status, 0);
  }

  WDF_INTERRUPT_CONFIG_INIT(&icfg, It8888EvtInterruptIsr,
                            It8888EvtInterruptDpc);
  status = WdfInterruptCreate(device, &icfg, WDF_NO_OBJECT_ATTRIBUTES,
                              &ctx->Interrupt);
  if (!NT_SUCCESS(status)) {
    ctx->Interrupt = NULL;
    It8888Trace(ctx, IT8888_TRACE_ERROR, 0xD002, (ULONGLONG)status, 0);
    status =
        STATUS_SUCCESS; // IRQ can be added later; keep diagnostic driver alive.
  }

  return status;
}

NTSTATUS It8888EvtPrepareHardware(WDFDEVICE Device, WDFCMRESLIST Raw,
                                  WDFCMRESLIST Translated) {
  UNREFERENCED_PARAMETER(Raw);
  UNREFERENCED_PARAMETER(Translated);
  PDEVICE_CONTEXT ctx = DeviceGetContext(Device);
  NTSTATUS status;
  ULONG val;

  status = WdfFdoQueryForInterface(Device, &GUID_BUS_INTERFACE_STANDARD,
                                   (PINTERFACE)&ctx->BusInterface,
                                   sizeof(BUS_INTERFACE_STANDARD), 1, NULL);
  if (NT_SUCCESS(status)) {
    ctx->BusInterfaceValid = TRUE;
    It8888Trace(ctx, IT8888_TRACE_INFO, 0x100, 1, 0);
  } else {
    ctx->BusInterfaceValid = FALSE;
    It8888Trace(ctx, IT8888_TRACE_ERROR, 0x100, (ULONGLONG)status, 0);
    return status;
  }

  It8888PciRead(ctx, 0x00, 2, &val);
  ctx->VendorId = (USHORT)val;
  It8888PciRead(ctx, 0x02, 2, &val);
  ctx->DeviceId = (USHORT)val;
  It8888PciRead(ctx, 0x08, 1, &val);
  ctx->RevisionId = (UCHAR)val;
  ctx->Started = TRUE;
  It8888RefreshDdmaBases(ctx);
  It8888EnableCommandBits(ctx);
  It8888Trace(ctx, IT8888_TRACE_INFO, 0x101,
              ((ULONGLONG)ctx->DeviceId << 16) | ctx->VendorId,
              ctx->RevisionId);
  return STATUS_SUCCESS;
}

NTSTATUS It8888EvtReleaseHardware(WDFDEVICE Device, WDFCMRESLIST Translated) {
  UNREFERENCED_PARAMETER(Translated);
  PDEVICE_CONTEXT ctx = DeviceGetContext(Device);
  It8888DmaFree(ctx);
  if (ctx->BusInterfaceValid && ctx->BusInterface.InterfaceDereference) {
    ctx->BusInterface.InterfaceDereference(ctx->BusInterface.Context);
  }
  ctx->BusInterfaceValid = FALSE;
  ctx->Started = FALSE;
  return STATUS_SUCCESS;
}

BOOLEAN It8888EvtInterruptIsr(WDFINTERRUPT Interrupt, ULONG MessageID) {
  PDEVICE_CONTEXT ctx = DeviceGetContext(WdfInterruptGetDevice(Interrupt));
  UNREFERENCED_PARAMETER(MessageID);
  InterlockedIncrement(&ctx->IrqPending);
  ctx->IrqCount++;
  ctx->LastIrqVector = MessageID;
  ctx->LastIrqStatus = 0;
  It8888Trace(ctx, IT8888_TRACE_IRQ, ctx->IrqCount, MessageID, 0);
  KeSetEvent(&ctx->IrqEvent, IO_NO_INCREMENT, FALSE);
  return TRUE;
}

VOID It8888EvtInterruptDpc(WDFINTERRUPT Interrupt, WDFOBJECT AssociatedObject) {
  UNREFERENCED_PARAMETER(Interrupt);
  UNREFERENCED_PARAMETER(AssociatedObject);
}
