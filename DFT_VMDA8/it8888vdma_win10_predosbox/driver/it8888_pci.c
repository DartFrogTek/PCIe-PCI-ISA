#include "it8888.h"

static NTSTATUS WidthToSize(UCHAR width, ULONG *size)
{
  if (width == 1 || width == 2 || width == 4)
  {
    *size = width;
    return STATUS_SUCCESS;
  }
  return STATUS_INVALID_PARAMETER;
}

NTSTATUS It8888PciRead(PDEVICE_CONTEXT ctx, USHORT offset, UCHAR width,
                       PULONG value)
{
  ULONG size;
  NTSTATUS st = WidthToSize(width, &size);
  ULONG v = 0;
  if (!NT_SUCCESS(st))
    return st;
  if (!ctx->BusInterfaceValid)
    return STATUS_DEVICE_NOT_READY;
  ULONG done = ctx->BusInterface.GetBusData(
      ctx->BusInterface.Context, PCI_WHICHSPACE_CONFIG, &v, offset, size);
  if (done != size)
    return STATUS_UNSUCCESSFUL;
  if (width == 1)
    v &= 0xFF;
  else if (width == 2)
    v &= 0xFFFF;
  *value = v;
  It8888Trace(ctx, IT8888_TRACE_CFG_READ, offset | ((ULONG)width << 16), v, 0);
  return STATUS_SUCCESS;
}

NTSTATUS It8888PciWrite(PDEVICE_CONTEXT ctx, USHORT offset, UCHAR width,
                        ULONG value)
{
  ULONG size;
  NTSTATUS st = WidthToSize(width, &size);
  if (!NT_SUCCESS(st))
    return st;
  if (!ctx->BusInterfaceValid)
    return STATUS_DEVICE_NOT_READY;
  ULONG v = value;
  ULONG done = ctx->BusInterface.SetBusData(
      ctx->BusInterface.Context, PCI_WHICHSPACE_CONFIG, &v, offset, size);
  if (done != size)
    return STATUS_UNSUCCESSFUL;
  It8888Trace(ctx, IT8888_TRACE_CFG_WRITE, offset | ((ULONG)width << 16), value,
              0);
  if (offset >= IT8888_CFG_CH01 && offset <= IT8888_CFG_CH67)
    It8888RefreshDdmaBases(ctx);
  return STATUS_SUCCESS;
}

NTSTATUS It8888EnableCommandBits(PDEVICE_CONTEXT ctx)
{
  ULONG cmd;
  NTSTATUS st = It8888PciRead(ctx, 0x04, 2, &cmd);
  if (!NT_SUCCESS(st))
    return st;
  cmd |=
      PCI_COMMAND_IO_SPACE | PCI_COMMAND_MEMORY_SPACE | PCI_COMMAND_BUS_MASTER;
  return It8888PciWrite(ctx, 0x04, 2, cmd);
}

VOID It8888RefreshDdmaBases(PDEVICE_CONTEXT ctx)
{
  ULONG v40 = 0, v44 = 0, v48 = 0, v4c = 0;
  if (NT_SUCCESS(It8888PciRead(ctx, IT8888_CFG_CH01, 4, &v40)))
  {
    ctx->DdmaBase[0] = (USHORT)(v40 & 0xFFF0);
    ctx->DdmaBase[1] = (USHORT)((v40 >> 16) & 0xFFF0);
  }
  if (NT_SUCCESS(It8888PciRead(ctx, IT8888_CFG_CH23, 4, &v44)))
  {
    ctx->DdmaBase[2] = (USHORT)(v44 & 0xFFF0);
    ctx->DdmaBase[3] = (USHORT)((v44 >> 16) & 0xFFF0);
  }
  if (NT_SUCCESS(It8888PciRead(ctx, IT8888_CFG_CH45, 4, &v48)))
  {
    ctx->DdmaBase[4] = (USHORT)(v48 & 0xFFF0);
    ctx->DdmaBase[5] = (USHORT)((v48 >> 16) & 0xFFF0);
  }
  if (NT_SUCCESS(It8888PciRead(ctx, IT8888_CFG_CH67, 4, &v4c)))
  {
    ctx->DdmaBase[6] = (USHORT)(v4c & 0xFFF0);
    ctx->DdmaBase[7] = (USHORT)((v4c >> 16) & 0xFFF0);
  }
}

NTSTATUS It8888ApplyDefaultInit(PDEVICE_CONTEXT ctx)
{
  NTSTATUS st;
  WdfWaitLockAcquire(ctx->HwLock, NULL);
  st = It8888EnableCommandBits(ctx);
  if (!NT_SUCCESS(st))
    goto out;

  // Recovered from ITEXXX defaults. Odd low bits are decode/control bits; base
  // used by DDMA ops is &~0xF.
  st = It8888PciWrite(ctx, IT8888_CFG_CH01, 4, 0x83918381);
  if (!NT_SUCCESS(st))
    goto out;
  st = It8888PciWrite(ctx, IT8888_CFG_CH23, 4, 0x83B183A1);
  if (!NT_SUCCESS(st))
    goto out;
  st = It8888PciWrite(ctx, IT8888_CFG_CH45, 4, 0x83D30000);
  if (!NT_SUCCESS(st))
    goto out;
  st = It8888PciWrite(ctx, IT8888_CFG_CH67, 4, 0x83F383E3);
  if (!NT_SUCCESS(st))
    goto out;
  st = It8888PciWrite(ctx, IT8888_CFG_50, 4, 0x01FFF023);
  if (!NT_SUCCESS(st))
    goto out;
  st = It8888PciWrite(ctx, IT8888_CFG_54, 4, 0x8C003F3F);
  if (!NT_SUCCESS(st))
    goto out;
  It8888RefreshDdmaBases(ctx);
  It8888Trace(ctx, IT8888_TRACE_INFO, 0x200, 0x01FFF023, 0x8C003F3F);
out:
  WdfWaitLockRelease(ctx->HwLock);
  return st;
}

NTSTATUS It8888GetInfo(PDEVICE_CONTEXT ctx, PIT8888_INFO info)
{
  ULONG v = 0;
  RtlZeroMemory(info, sizeof(*info));
  info->VendorId = ctx->VendorId;
  info->DeviceId = ctx->DeviceId;
  info->RevisionId = ctx->RevisionId;
  info->Bus = ctx->BusNumber;
  info->Device = ctx->DeviceNumber;
  info->Function = ctx->FunctionNumber;
  info->Started = ctx->Started ? 1 : 0;
  It8888PciRead(ctx, 0x04, 4, &v);
  info->PciCommandStatus = v;
  info->LastPciStatus = v >> 16;
  It8888PciRead(ctx, IT8888_CFG_CH01, 4, &info->Cfg40);
  It8888PciRead(ctx, IT8888_CFG_CH23, 4, &info->Cfg44);
  It8888PciRead(ctx, IT8888_CFG_CH45, 4, &info->Cfg48);
  It8888PciRead(ctx, IT8888_CFG_CH67, 4, &info->Cfg4C);
  It8888PciRead(ctx, IT8888_CFG_50, 4, &info->Cfg50);
  It8888PciRead(ctx, IT8888_CFG_54, 4, &info->Cfg54);
  for (int i = 0; i < 8; i++)
    info->DdmaBase[i] = ctx->DdmaBase[i];
  info->DmaLogicalAddress = ctx->Dma.Logical.QuadPart;
  info->DmaSize = (ULONG)ctx->Dma.Size;
  info->IrqCount = ctx->IrqCount;
  return STATUS_SUCCESS;
}

NTSTATUS It8888ClearErrors(PDEVICE_CONTEXT ctx)
{
  // PCI status bits are write-1-to-clear in upper command/status word.
  ULONG cs = 0;
  NTSTATUS st = It8888PciRead(ctx, 0x04, 4, &cs);
  if (!NT_SUCCESS(st))
    return st;
  ULONG clear = cs | 0xFFFF0000u;
  st = It8888PciWrite(ctx, 0x04, 4, clear);
  It8888Trace(ctx, IT8888_TRACE_INFO, 0x201, cs, clear);
  return st;
}

NTSTATUS It8888PciDumpCfgBdf(PIT8888_PCI_CFG_DUMP dump)
{
    if (dump->Device > 31 || dump->Function > 7)
        return STATUS_INVALID_PARAMETER;

    PCI_SLOT_NUMBER slot;
    RtlZeroMemory(&slot, sizeof(slot));
    slot.u.bits.DeviceNumber = dump->Device;
    slot.u.bits.FunctionNumber = dump->Function;

    RtlZeroMemory(dump->Data, sizeof(dump->Data));
    dump->BytesRead = 0;
    dump->Status = 0;

#pragma warning(push)
#pragma warning(disable:4996)
    ULONG got = HalGetBusDataByOffset(PCIConfiguration,
                                      dump->Bus,
                                      slot.u.AsULONG,
                                      dump->Data,
                                      0,
                                      sizeof(dump->Data));
#pragma warning(pop)

    dump->BytesRead = got;

    if (got == 0) {
        dump->Status = 1;
        return STATUS_NOT_FOUND;
    }

    return STATUS_SUCCESS;
}

static NTSTATUS It8888PciMakeSlot(UCHAR device, UCHAR function, PPCI_SLOT_NUMBER slot)
{
    if (device > 31 || function > 7) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(slot, sizeof(*slot));
    slot->u.bits.DeviceNumber = device;
    slot->u.bits.FunctionNumber = function;
    return STATUS_SUCCESS;
}


static ULONG It8888PciCfgAddress(UCHAR bus, UCHAR device, UCHAR function, UCHAR offset)
{
    return 0x80000000u |
           ((ULONG)bus << 16) |
           ((ULONG)device << 11) |
           ((ULONG)function << 8) |
           ((ULONG)offset & 0xFCu);
}

static ULONG It8888PciCfgRawRead(UCHAR bus, UCHAR device, UCHAR function, UCHAR offset, UCHAR width)
{
    ULONG address = It8888PciCfgAddress(bus, device, function, offset);
    UCHAR shift = (UCHAR)((offset & 3u) * 8u);
    ULONG dwordValue;

    WRITE_PORT_ULONG((PULONG)(ULONG_PTR)0xCF8, address);
    dwordValue = READ_PORT_ULONG((PULONG)(ULONG_PTR)0xCFC);

    if (width == 1)
        return (dwordValue >> shift) & 0xFFu;
    if (width == 2)
        return (dwordValue >> shift) & 0xFFFFu;
    return dwordValue;
}

static VOID It8888PciCfgRawWrite(UCHAR bus, UCHAR device, UCHAR function, UCHAR offset, UCHAR width, ULONG value)
{
    ULONG address = It8888PciCfgAddress(bus, device, function, offset);
    UCHAR shift = (UCHAR)((offset & 3u) * 8u);
    ULONG dwordValue;
    ULONG mask;

    WRITE_PORT_ULONG((PULONG)(ULONG_PTR)0xCF8, address);
    dwordValue = READ_PORT_ULONG((PULONG)(ULONG_PTR)0xCFC);

    if (width == 1) {
        mask = 0xFFu << shift;
        dwordValue = (dwordValue & ~mask) | ((value & 0xFFu) << shift);
    } else if (width == 2) {
        mask = 0xFFFFu << shift;
        dwordValue = (dwordValue & ~mask) | ((value & 0xFFFFu) << shift);
    } else {
        dwordValue = value;
    }

    WRITE_PORT_ULONG((PULONG)(ULONG_PTR)0xCF8, address);
    WRITE_PORT_ULONG((PULONG)(ULONG_PTR)0xCFC, dwordValue);
}
NTSTATUS It8888PciCfgRwBdf(PIT8888_PCI_CFG_RW rw)
{
    if (rw->Device > 31 || rw->Function > 7)
        return STATUS_INVALID_PARAMETER;

    if (!(rw->Width == 1 || rw->Width == 2 || rw->Width == 4))
        return STATUS_INVALID_PARAMETER;

    if ((ULONG)rw->Offset + rw->Width > 256)
        return STATUS_INVALID_PARAMETER;

    if ((rw->Width == 2 && (rw->Offset & 1)) || (rw->Width == 4 && (rw->Offset & 3)))
        return STATUS_INVALID_PARAMETER;

    if (rw->Write) {
        It8888PciCfgRawWrite(rw->Bus, rw->Device, rw->Function, rw->Offset, rw->Width, rw->Value);
    } else {
        rw->Value = It8888PciCfgRawRead(rw->Bus, rw->Device, rw->Function, rw->Offset, rw->Width);
        if (rw->Width == 1)
            rw->Value &= 0xFFu;
        else if (rw->Width == 2)
            rw->Value &= 0xFFFFu;
    }

    rw->Bytes = rw->Width;
    rw->Status = 0xCF8;
    return STATUS_SUCCESS;
}

static NTSTATUS It8888PciCfgReadSimple(UCHAR bus, UCHAR dev, UCHAR fn, UCHAR off, UCHAR width, PULONG value)
{
    IT8888_PCI_CFG_RW rw; RtlZeroMemory(&rw, sizeof(rw));
    rw.Bus=bus; rw.Device=dev; rw.Function=fn; rw.Offset=off; rw.Width=width;
    NTSTATUS st = It8888PciCfgRwBdf(&rw);
    if (NT_SUCCESS(st)) *value = rw.Value;
    return st;
}

static NTSTATUS It8888PciCfgWriteSimple(UCHAR bus, UCHAR dev, UCHAR fn, UCHAR off, UCHAR width, ULONG value)
{
    IT8888_PCI_CFG_RW rw; RtlZeroMemory(&rw, sizeof(rw));
    rw.Bus=bus; rw.Device=dev; rw.Function=fn; rw.Offset=off; rw.Width=width; rw.Write=1; rw.Value=value;
    return It8888PciCfgRwBdf(&rw);
}

NTSTATUS It8888BridgeSetIoWindow(PIT8888_BRIDGE_IOWIN win)
{
    ULONG oldCmd = 0;
    ULONG oldBase = 0;
    ULONG oldLimit = 0;
    ULONG oldBaseUpper = 0;
    ULONG oldLimitUpper = 0;

    if ((win->Base & 0xFFFu) != 0)
        return STATUS_INVALID_PARAMETER;

    if ((win->Limit & 0xFFFu) != 0xFFFu)
        return STATUS_INVALID_PARAMETER;

    if (win->Base > win->Limit)
        return STATUS_INVALID_PARAMETER;

    if (win->Base > 0xFFFFu || win->Limit > 0xFFFFu)
        return STATUS_INVALID_PARAMETER;

    oldCmd        = It8888PciCfgRawRead(win->Bus, win->Device, win->Function, 0x04, 2);
    oldBase       = It8888PciCfgRawRead(win->Bus, win->Device, win->Function, 0x1C, 1);
    oldLimit      = It8888PciCfgRawRead(win->Bus, win->Device, win->Function, 0x1D, 1);
    oldBaseUpper  = It8888PciCfgRawRead(win->Bus, win->Device, win->Function, 0x30, 2);
    oldLimitUpper = It8888PciCfgRawRead(win->Bus, win->Device, win->Function, 0x32, 2);

    win->OldCommand = (USHORT)oldCmd;
    win->OldIoBase = (UCHAR)oldBase;
    win->OldIoLimit = (UCHAR)oldLimit;
    win->OldIoBaseUpper = (USHORT)oldBaseUpper;
    win->OldIoLimitUpper = (USHORT)oldLimitUpper;

    win->NewIoBase = (UCHAR)((win->Base >> 8) & 0xF0u);
    win->NewIoLimit = (UCHAR)((win->Limit >> 8) & 0xF0u);
    win->NewIoBaseUpper = (USHORT)((win->Base >> 16) & 0xFFFFu);
    win->NewIoLimitUpper = (USHORT)((win->Limit >> 16) & 0xFFFFu);

    /*
        Disable I/O forwarding, program window, then re-enable I/O forwarding.
        For 0x8000-0x8FFF this writes:
          0x1C = 0x80
          0x1D = 0x80
          0x30 = 0
          0x32 = 0
          command |= 0x0001
    */
    It8888PciCfgRawWrite(win->Bus, win->Device, win->Function, 0x04, 2, win->OldCommand & ~0x0001u);
    It8888PciCfgRawWrite(win->Bus, win->Device, win->Function, 0x30, 2, win->NewIoBaseUpper);
    It8888PciCfgRawWrite(win->Bus, win->Device, win->Function, 0x32, 2, win->NewIoLimitUpper);
    It8888PciCfgRawWrite(win->Bus, win->Device, win->Function, 0x1C, 1, win->NewIoBase);
    It8888PciCfgRawWrite(win->Bus, win->Device, win->Function, 0x1D, 1, win->NewIoLimit);

    win->NewCommand = (USHORT)(win->OldCommand | 0x0001u);
    It8888PciCfgRawWrite(win->Bus, win->Device, win->Function, 0x04, 2, win->NewCommand);

    win->Status = 0xCF8;
    return STATUS_SUCCESS;
}

