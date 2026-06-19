#pragma once

#include <ntddk.h>

VOID
DftFdcTrace(
    _In_z_ _Printf_format_string_ PCSTR Format,
    ...
    );
