#include <stdarg.h>
#include "driver.h"

VOID
DftFdcTrace(
    _In_z_ _Printf_format_string_ PCSTR Format,
    ...
    )
{
    va_list args;
    va_start(args, Format);
    vDbgPrintExWithPrefix("[dftfdc] ", DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, Format, args);
    va_end(args);
}
