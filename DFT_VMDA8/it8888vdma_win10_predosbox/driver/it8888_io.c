#include "it8888.h"

static BOOLEAN It8888PortAllowed(USHORT port, UCHAR width)
{
    if (!(width == 1 || width == 2 || width == 4))
        return FALSE;

    /*
        Diagnostic allowlist. Keep narrow, but include the whole high alias
        bridge test window.
    */

    if (port >= 0x8000 && port <= 0x8FFF) return TRUE; /* high alias / bridge window */
    if (port >= 0x8380 && port <= 0x83FF) return TRUE; /* IT8888 DDMA windows */

    if (port >= 0x220 && port <= 0x22F) return TRUE;   /* SB 220 */
    if (port >= 0x240 && port <= 0x24F) return TRUE;   /* SB alt */
    if (port >= 0x260 && port <= 0x26F) return TRUE;   /* SB alt */
    if (port >= 0x280 && port <= 0x28F) return TRUE;   /* SB alt */

    if (port >= 0x300 && port <= 0x31F) return TRUE;   /* GUS-ish */
    if (port >= 0x330 && port <= 0x337) return TRUE;   /* MPU-401 */
    if (port >= 0x388 && port <= 0x38B) return TRUE;   /* OPL */

    return FALSE;
}

NTSTATUS It8888PortRead(PDEVICE_CONTEXT ctx, USHORT port, UCHAR width,
                        PULONG value)
{
  UNREFERENCED_PARAMETER(ctx);
  if (!It8888PortAllowed(port, width))
    return STATUS_ACCESS_DENIED;
  ULONG v;
  if (width == 1)
    v = READ_PORT_UCHAR((PUCHAR)(ULONG_PTR)port);
  else if (width == 2)
    v = READ_PORT_USHORT((PUSHORT)(ULONG_PTR)port);
  else
    v = READ_PORT_ULONG((PULONG)(ULONG_PTR)port);
  *value = v;
  It8888Trace(ctx, IT8888_TRACE_PORT_READ, port | ((ULONG)width << 16), v, 0);
  return STATUS_SUCCESS;
}

NTSTATUS It8888PortWrite(PDEVICE_CONTEXT ctx, USHORT port, UCHAR width,
                         ULONG value)
{
  UNREFERENCED_PARAMETER(ctx);
  if (!It8888PortAllowed(port, width))
    return STATUS_ACCESS_DENIED;
  if (width == 1)
    WRITE_PORT_UCHAR((PUCHAR)(ULONG_PTR)port, (UCHAR)value);
  else if (width == 2)
    WRITE_PORT_USHORT((PUSHORT)(ULONG_PTR)port, (USHORT)value);
  else
    WRITE_PORT_ULONG((PULONG)(ULONG_PTR)port, value);
  It8888Trace(ctx, IT8888_TRACE_PORT_WRITE, port | ((ULONG)width << 16), value,
              0);
  return STATUS_SUCCESS;
}



