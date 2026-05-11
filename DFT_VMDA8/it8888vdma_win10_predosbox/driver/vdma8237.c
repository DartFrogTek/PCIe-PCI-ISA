#include "it8888.h"

static UCHAR ChannelFromMaskPort(UCHAR val) { return val & 3; }
static BOOLEAN MaskBit(UCHAR val) { return (val & 4) ? TRUE : FALSE; }
static UCHAR DirFromMode(UCHAR mode)
{
    switch (mode & DMA_MODE_TRANSFER_MASK) {
    case DMA_TRANSFER_READ: return IT8888_DIR_RAM_TO_ISA;
    case DMA_TRANSFER_WRITE: return IT8888_DIR_ISA_TO_RAM;
    default: return IT8888_DIR_VERIFY;
    }
}
static UINT32 LegacyAddr(PDEVICE_CONTEXT ctx, UCHAR ch)
{
    UINT32 a = ((UINT32)ctx->Vdma.Ch[ch].Page << 16) | ctx->Vdma.Ch[ch].CurAddr;
    if (ch >= 5 && ch <= 7) a <<= 1;
    return a;
}
static UINT32 ByteCount(PDEVICE_CONTEXT ctx, UCHAR ch)
{
    UINT32 c = (UINT32)ctx->Vdma.Ch[ch].CurCount + 1;
    if (ch >= 5 && ch <= 7) c <<= 1;
    return c;
}
static UCHAR *PageReg(PDEVICE_CONTEXT ctx, USHORT port)
{
    switch (port) {
    case 0x87: return &ctx->Vdma.Ch[0].Page;
    case 0x83: return &ctx->Vdma.Ch[1].Page;
    case 0x81: return &ctx->Vdma.Ch[2].Page;
    case 0x82: return &ctx->Vdma.Ch[3].Page;
    case 0x8B: return &ctx->Vdma.Ch[5].Page;
    case 0x89: return &ctx->Vdma.Ch[6].Page;
    case 0x8A: return &ctx->Vdma.Ch[7].Page;
    default: return NULL;
    }
}

VOID Vdma8237Reset(PDEVICE_CONTEXT ctx)
{
    RtlZeroMemory(&ctx->Vdma, sizeof(ctx->Vdma));
    for (int i=0;i<8;i++) ctx->Vdma.Ch[i].Masked = 1;
    ctx->Vdma.Mask0 = 0x0F;
    ctx->Vdma.Mask1 = 0x0E; // channel 4 cascade-ish, keep unavailable
    It8888Trace(ctx, IT8888_TRACE_VDMA, 0x1000, 0, 0);
}

NTSTATUS Vdma8237Out(PDEVICE_CONTEXT ctx, USHORT port, UCHAR value)
{
    VDMA8237_STATE *s = &ctx->Vdma;
    UCHAR *pg = PageReg(ctx, port);
    if (pg) { *pg = value; It8888Trace(ctx, IT8888_TRACE_VDMA, port, value, 0); return STATUS_SUCCESS; }

    if (port <= 0x0F) {
        UCHAR ch = (UCHAR)(port / 2);
        if (port <= 0x07) {
            if (port & 1) {
                if (!s->FlipFlop0) s->Ch[ch].BaseCount = (s->Ch[ch].BaseCount & 0xFF00) | value;
                else { s->Ch[ch].BaseCount = (s->Ch[ch].BaseCount & 0x00FF) | ((USHORT)value << 8); s->Ch[ch].CurCount = s->Ch[ch].BaseCount; }
                s->FlipFlop0 ^= 1;
            } else {
                if (!s->FlipFlop0) s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0xFF00) | value;
                else { s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0x00FF) | ((USHORT)value << 8); s->Ch[ch].CurAddr = s->Ch[ch].BaseAddr; }
                s->FlipFlop0 ^= 1;
            }
        } else switch (port) {
            case 0x0A: { UCHAR c = ChannelFromMaskPort(value); s->Ch[c].Masked = MaskBit(value); if (s->Ch[c].Masked) s->Mask0 |= (1<<c); else s->Mask0 &= ~(1<<c); break; }
            case 0x0B: { UCHAR c = value & 3; s->Ch[c].Mode = value; break; }
            case 0x0C: s->FlipFlop0 = 0; break;
            case 0x0D: Vdma8237Reset(ctx); break;
            case 0x0E: for (int i=0;i<4;i++){s->Ch[i].Masked=0;} s->Mask0=0; break;
            case 0x0F: for (int i=0;i<4;i++){s->Ch[i].Masked=(value>>i)&1;} s->Mask0=value&0x0F; break;
            default: break;
        }
        It8888Trace(ctx, IT8888_TRACE_VDMA, port, value, 0); return STATUS_SUCCESS;
    }

    if (port >= 0xC0 && port <= 0xDF) {
        UCHAR ch = 4 + (UCHAR)((port - 0xC0) / 4);
        if (ch > 7) return STATUS_INVALID_PARAMETER;
        USHORT rel = port - 0xC0;
        if ((rel % 8) == 0) {
            if (!s->FlipFlop1) s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0xFF00) | value;
            else { s->Ch[ch].BaseAddr = (s->Ch[ch].BaseAddr & 0x00FF) | ((USHORT)value << 8); s->Ch[ch].CurAddr = s->Ch[ch].BaseAddr; }
            s->FlipFlop1 ^= 1;
        } else if ((rel % 8) == 2) {
            if (!s->FlipFlop1) s->Ch[ch].BaseCount = (s->Ch[ch].BaseCount & 0xFF00) | value;
            else { s->Ch[ch].BaseCount = (s->Ch[ch].BaseCount & 0x00FF) | ((USHORT)value << 8); s->Ch[ch].CurCount = s->Ch[ch].BaseCount; }
            s->FlipFlop1 ^= 1;
        } else switch (port) {
            case 0xD4: { UCHAR c = 4 + ChannelFromMaskPort(value); s->Ch[c].Masked = MaskBit(value); if (s->Ch[c].Masked) s->Mask1 |= (1<<(c-4)); else s->Mask1 &= ~(1<<(c-4)); break; }
            case 0xD6: { UCHAR c = 4 + (value & 3); if (c < 8) s->Ch[c].Mode = value; break; }
            case 0xD8: s->FlipFlop1 = 0; break;
            case 0xDA: Vdma8237Reset(ctx); break;
            case 0xDC: for (int i=5;i<8;i++){s->Ch[i].Masked=0;} s->Mask1 &= ~0x0E; break;
            case 0xDE: for (int i=5;i<8;i++){s->Ch[i].Masked=(value>>(i-4))&1;} s->Mask1=value&0x0F; break;
            default: break;
        }
        It8888Trace(ctx, IT8888_TRACE_VDMA, port, value, 0); return STATUS_SUCCESS;
    }
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS Vdma8237In(PDEVICE_CONTEXT ctx, USHORT port, PUCHAR value)
{
    UCHAR *pg = PageReg(ctx, port);
    if (pg) { *value = *pg; return STATUS_SUCCESS; }
    *value = 0xFF;
    if (port == 0x08) { *value = ctx->Vdma.Status0; ctx->Vdma.Status0 = 0; }
    else if (port == 0xD0) { *value = ctx->Vdma.Status1; ctx->Vdma.Status1 = 0; }
    else if (port == 0x0F) *value = ctx->Vdma.Mask0;
    else if (port == 0xDE) *value = ctx->Vdma.Mask1;
    It8888Trace(ctx, IT8888_TRACE_VDMA, 0x80000000u | port, *value, 0);
    return STATUS_SUCCESS;
}

VOID Vdma8237Snapshot(PDEVICE_CONTEXT ctx, PIT8888_8237_SNAPSHOT snap)
{
    RtlZeroMemory(snap, sizeof(*snap));
    for (int i=0;i<8;i++) {
        snap->Ch[i].BaseAddr = ctx->Vdma.Ch[i].BaseAddr;
        snap->Ch[i].CurAddr = ctx->Vdma.Ch[i].CurAddr;
        snap->Ch[i].BaseCount = ctx->Vdma.Ch[i].BaseCount;
        snap->Ch[i].CurCount = ctx->Vdma.Ch[i].CurCount;
        snap->Ch[i].Page = ctx->Vdma.Ch[i].Page;
        snap->Ch[i].Mode = ctx->Vdma.Ch[i].Mode;
        snap->Ch[i].Masked = ctx->Vdma.Ch[i].Masked;
        snap->Ch[i].TerminalCount = ctx->Vdma.Ch[i].TerminalCount;
        snap->Ch[i].LegacyAddress = LegacyAddr(ctx, (UCHAR)i);
        snap->Ch[i].ByteCount = ByteCount(ctx, (UCHAR)i);
        snap->Ch[i].Direction = DirFromMode(ctx->Vdma.Ch[i].Mode);
        snap->Ch[i].Is16Bit = (i >= 5 && i <= 7) ? 1 : 0;
    }
    snap->Command0=ctx->Vdma.Command0; snap->Command1=ctx->Vdma.Command1;
    snap->Status0=ctx->Vdma.Status0; snap->Status1=ctx->Vdma.Status1;
    snap->Mask0=ctx->Vdma.Mask0; snap->Mask1=ctx->Vdma.Mask1;
    snap->FlipFlop0=ctx->Vdma.FlipFlop0; snap->FlipFlop1=ctx->Vdma.FlipFlop1;
}

NTSTATUS Vdma8237Prepare(PDEVICE_CONTEXT ctx, UCHAR ch, PIT8888_8237_PREPARE prep)
{
    if (ch > 7 || ch == 4) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(prep, sizeof(*prep));
    prep->Channel = ch;
    prep->LegacyAddress = LegacyAddr(ctx, ch);
    prep->ByteCount = ByteCount(ctx, ch);
    prep->Direction = DirFromMode(ctx->Vdma.Ch[ch].Mode);
    prep->Mode = ctx->Vdma.Ch[ch].Mode;
    prep->Masked = ctx->Vdma.Ch[ch].Masked;
    prep->Is16Bit = (ch >= 5 && ch <= 7) ? 1 : 0;
    return STATUS_SUCCESS;
}
