#pragma once

/*
 * Shared user/kernel ABI for the pass-1 DFT 5.25" floppy diagnostic driver.
 */

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#define DFTFDC_ABI_VERSION      0x00010000u
#define DFTFDC_VERSION_MAJOR    0u
#define DFTFDC_VERSION_MINOR    1u
#define DFTFDC_VERSION_PATCH    0u
#define DFTFDC_VERSION_BUILD    1u

#define DFTFDC_DEVICE_TYPE      0x8337u
#define DFTFDC_IOCTL(_index_)   CTL_CODE(DFTFDC_DEVICE_TYPE, (_index_), METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_DFTFDC_GET_VERSION      DFTFDC_IOCTL(0x800)
#define IOCTL_DFTFDC_PROBE_BRIDGE     DFTFDC_IOCTL(0x801)
#define IOCTL_DFTFDC_PROBE_FDC        DFTFDC_IOCTL(0x802)
#define IOCTL_DFTFDC_RESET_FDC        DFTFDC_IOCTL(0x803)
#define IOCTL_DFTFDC_RECALIBRATE      DFTFDC_IOCTL(0x804)
#define IOCTL_DFTFDC_SEEK             DFTFDC_IOCTL(0x805)
#define IOCTL_DFTFDC_READ_ID          DFTFDC_IOCTL(0x806)
#define IOCTL_DFTFDC_READ_SECTOR      DFTFDC_IOCTL(0x807)
#define IOCTL_DFTFDC_SET_GEOMETRY     DFTFDC_IOCTL(0x808)
#define IOCTL_DFTFDC_GET_GEOMETRY     DFTFDC_IOCTL(0x809)
#define IOCTL_DFTFDC_GET_IRQ_COUNT    DFTFDC_IOCTL(0x80A)
#define IOCTL_DFTFDC_SET_TRANSFER_MODE DFTFDC_IOCTL(0x80B)

#define DFTFDC_USER_DEVICE_PATH_A "\\\\.\\DftFdc0"
#define DFTFDC_USER_DEVICE_PATH_W L"\\\\.\\DftFdc0"

#define DFTFDC_MAX_FDC_RESULT_BYTES 16u
#define DFTFDC_SECTOR_BYTES         512u

#define DFTFDC_GEOMETRY_360K        1u
#define DFTFDC_GEOMETRY_1200K       2u

#define DFTFDC_TRANSFER_PIO         0u
#define DFTFDC_TRANSFER_IT8888_DDMA 1u

typedef struct _DFTFDC_VERSION_INFO {
    ULONG AbiVersion;
    ULONG Major;
    ULONG Minor;
    ULONG Patch;
    ULONG Build;
    CHAR  Name[32];
} DFTFDC_VERSION_INFO, *PDFTFDC_VERSION_INFO;

typedef struct _DFTFDC_BRIDGE_INFO {
    ULONG Status;
    USHORT VendorId;
    USHORT DeviceId;
    UCHAR RevisionId;
    UCHAR HeaderType;
    UCHAR ClassCode;
    UCHAR SubClass;
    UCHAR ProgIf;
    ULONG CommandStatus;
    ULONG BusInterfaceValid;
} DFTFDC_BRIDGE_INFO, *PDFTFDC_BRIDGE_INFO;

typedef struct _DFTFDC_FDC_PROBE_INFO {
    ULONG Status;
    USHORT BasePort;
    UCHAR MainStatus;
    UCHAR DigitalInput;
    UCHAR TransferMode;
    ULONG IrqCount;
} DFTFDC_FDC_PROBE_INFO, *PDFTFDC_FDC_PROBE_INFO;

typedef struct _DFTFDC_MEDIA_GEOMETRY_INFO {
    ULONG GeometryId;
    ULONG Cylinders;
    ULONG Heads;
    ULONG SectorsPerTrack;
    ULONG BytesPerSector;
    ULONG TotalSectors;
    ULONG DataRateKbps;
    UCHAR GapLength;
    UCHAR FormatGapLength;
    UCHAR RateSelect;
    UCHAR Reserved;
    CHAR Name[16];
} DFTFDC_MEDIA_GEOMETRY_INFO, *PDFTFDC_MEDIA_GEOMETRY_INFO;

typedef struct _DFTFDC_SEEK_REQUEST {
    ULONG Cylinder;
    ULONG Head;
} DFTFDC_SEEK_REQUEST, *PDFTFDC_SEEK_REQUEST;

typedef struct _DFTFDC_TRANSFER_MODE_REQUEST {
    ULONG TransferMode;
} DFTFDC_TRANSFER_MODE_REQUEST, *PDFTFDC_TRANSFER_MODE_REQUEST;

typedef struct _DFTFDC_FDC_RESULT {
    ULONG Status;
    ULONG ResultLength;
    UCHAR Result[DFTFDC_MAX_FDC_RESULT_BYTES];
} DFTFDC_FDC_RESULT, *PDFTFDC_FDC_RESULT;

typedef struct _DFTFDC_READ_SECTOR_REQUEST {
    ULONG UseChs;
    ULONG Lba;
    ULONG Cylinder;
    ULONG Head;
    ULONG Sector;      /* 1-based sector number when UseChs != 0 */
} DFTFDC_READ_SECTOR_REQUEST, *PDFTFDC_READ_SECTOR_REQUEST;

typedef struct _DFTFDC_READ_SECTOR_RESULT {
    ULONG Status;
    ULONG GeometryId;
    ULONG Lba;
    ULONG Cylinder;
    ULONG Head;
    ULONG Sector;
    ULONG ResultLength;
    UCHAR Result[DFTFDC_MAX_FDC_RESULT_BYTES];
    UCHAR Data[DFTFDC_SECTOR_BYTES];
} DFTFDC_READ_SECTOR_RESULT, *PDFTFDC_READ_SECTOR_RESULT;

typedef struct _DFTFDC_IRQ_COUNT_INFO {
    ULONG IrqCount;
} DFTFDC_IRQ_COUNT_INFO, *PDFTFDC_IRQ_COUNT_INFO;
