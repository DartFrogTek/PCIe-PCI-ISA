#include "driver.h"

NTSTATUS
It8888ProgramDmaChannel2(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ BOOLEAN DeviceToMemory,
    _In_ PHYSICAL_ADDRESS BufferPhysical,
    _In_ ULONG Length
    )
{
    /*
     * Isolated IT8888 DDMA hook for FDC channel 2.
     *
     * This pass-1 project includes a working FDC polling/PIO sector path first,
     * so raw FDC behavior can be proven before enabling DDMA. The final DDMA
     * version belongs here and should program the same class of IT8888/DDMA
     * registers we used in the previous KMDF work, but targeted at classic
     * floppy channel 2:
     *
     *   direction: FDC -> memory for READ DATA, memory -> FDC for WRITE DATA
     *   channel:   2
     *   width:     8-bit
     *   length:    usually 512 bytes for a single sector
     *   address:   BufferPhysical
     *
     * The caller already provides a contiguous 64K-boundary-safe bounce buffer.
     */

    UNREFERENCED_PARAMETER(Ctx);
    UNREFERENCED_PARAMETER(DeviceToMemory);
    UNREFERENCED_PARAMETER(BufferPhysical);
    UNREFERENCED_PARAMETER(Length);

    return STATUS_NOT_SUPPORTED;
}

VOID
It8888ClearDmaChannel2(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    UNREFERENCED_PARAMETER(Ctx);
}
