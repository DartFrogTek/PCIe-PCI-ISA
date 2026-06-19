#include "driver.h"

NTSTATUS
DftFdcCreateInterrupt(
    _In_ WDFDEVICE Device
    )
{
    NTSTATUS status;
    WDF_INTERRUPT_CONFIG config;
    PDEVICE_CONTEXT ctx = DftFdcGetContext(Device);

    WDF_INTERRUPT_CONFIG_INIT(&config, DftFdcEvtInterruptIsr, DftFdcEvtInterruptDpc);
    config.PassiveHandling = FALSE;

    status = WdfInterruptCreate(Device,
                                &config,
                                WDF_NO_OBJECT_ATTRIBUTES,
                                &ctx->Interrupt);
    if (NT_SUCCESS(status)) {
        ctx->InterruptCreated = TRUE;
        DftFdcTrace("Interrupt object created\n");
    }

    return status;
}

BOOLEAN
DftFdcEvtInterruptIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ ULONG MessageID
    )
{
    WDFDEVICE device = WdfInterruptGetDevice(Interrupt);
    PDEVICE_CONTEXT ctx = DftFdcGetContext(device);

    UNREFERENCED_PARAMETER(MessageID);

    InterlockedIncrement(&ctx->IrqCount);
    KeSetEvent(&ctx->IrqEvent, IO_NO_INCREMENT, FALSE);
    WdfInterruptQueueDpcForIsr(Interrupt);

    return TRUE;
}

VOID
DftFdcEvtInterruptDpc(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT AssociatedObject
    )
{
    UNREFERENCED_PARAMETER(Interrupt);
    UNREFERENCED_PARAMETER(AssociatedObject);
}
