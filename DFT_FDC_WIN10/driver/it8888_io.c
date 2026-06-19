#include "driver.h"

NTSTATUS
It8888ConfigureFdcPath(
    _In_ PDEVICE_CONTEXT Ctx
    )
{
    /*
     * Pass 1 intentionally keeps bridge-specific decode programming isolated.
     *
     * On many bring-up setups the IT8888/PCI resource path has already been
     * configured enough for legacy ISA I/O cycles to 0x3F0-0x3F7 to reach the
     * FDC. If your board requires positive decode bits or subtractive routing
     * to be explicitly programmed, add those IT8888 config writes here.
     *
     * This function is the direct replacement point for the previous project’s
     * bridge-init pattern, but with floppy-specific decode targets:
     *   - FDC I/O window 0x3F0-0x3F7
     *   - IRQ6 routing to the PCI interrupt seen by KMDF
     *   - DDMA channel 2 path, later in it8888_ddma.c
     */

    UNREFERENCED_PARAMETER(Ctx);
    DftFdcTrace("It8888ConfigureFdcPath: pass-1 placeholder, assuming existing ISA I/O decode\n");
    return STATUS_SUCCESS;
}
