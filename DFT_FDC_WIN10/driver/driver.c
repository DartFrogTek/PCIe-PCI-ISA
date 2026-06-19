#include "driver.h"

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG config;

    DftFdcTrace("DriverEntry\n");

    WDF_DRIVER_CONFIG_INIT(&config, DftFdcEvtDeviceAdd);
    config.DriverPoolTag = 'cFdD';

    return WdfDriverCreate(DriverObject,
                           RegistryPath,
                           WDF_NO_OBJECT_ATTRIBUTES,
                           &config,
                           WDF_NO_HANDLE);
}

NTSTATUS
DftFdcEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    UNREFERENCED_PARAMETER(Driver);
    return DftFdcCreateDevice(DeviceInit);
}
