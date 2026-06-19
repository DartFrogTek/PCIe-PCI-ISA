#include "driver.h"

static const DFTFDC_MEDIA_GEOMETRY_INFO g_Geometries[] = {
    {
        DFTFDC_GEOMETRY_360K,
        40, 2, 9, 512,
        40 * 2 * 9,
        250,
        0x2A,
        0x50,
        0x02,       /* CCR/DSR rate select: 250 kbps */
        0,
        "5.25_360K"
    },
    {
        DFTFDC_GEOMETRY_1200K,
        80, 2, 15, 512,
        80 * 2 * 15,
        500,
        0x1B,
        0x54,
        0x00,       /* CCR/DSR rate select: 500 kbps */
        0,
        "5.25_1200K"
    }
};

NTSTATUS
FdcGetGeometryById(
    _In_ ULONG GeometryId,
    _Out_ PDFTFDC_MEDIA_GEOMETRY_INFO Geometry
    )
{
    ULONG i;

    for (i = 0; i < RTL_NUMBER_OF(g_Geometries); i++) {
        if (g_Geometries[i].GeometryId == GeometryId) {
            *Geometry = g_Geometries[i];
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
FdcSetGeometry(
    _In_ PDEVICE_CONTEXT Ctx,
    _In_ ULONG GeometryId
    )
{
    NTSTATUS status;
    DFTFDC_MEDIA_GEOMETRY_INFO geometry;

    status = FdcGetGeometryById(GeometryId, &geometry);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    Ctx->Geometry = geometry;
    DftFdcTrace("Geometry set to %s: C=%lu H=%lu SPT=%lu rate=%lu kbps\n",
                Ctx->Geometry.Name,
                Ctx->Geometry.Cylinders,
                Ctx->Geometry.Heads,
                Ctx->Geometry.SectorsPerTrack,
                Ctx->Geometry.DataRateKbps);

    return STATUS_SUCCESS;
}

NTSTATUS
FdcLbaToChs(
    _In_ const DFTFDC_MEDIA_GEOMETRY_INFO* Geometry,
    _In_ ULONG Lba,
    _Out_ PULONG Cylinder,
    _Out_ PULONG Head,
    _Out_ PULONG Sector
    )
{
    ULONG sectorsPerCylinder;
    ULONG rem;

    if (Geometry == NULL || Cylinder == NULL || Head == NULL || Sector == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (Lba >= Geometry->TotalSectors) {
        return STATUS_INVALID_PARAMETER;
    }

    sectorsPerCylinder = Geometry->Heads * Geometry->SectorsPerTrack;
    *Cylinder = Lba / sectorsPerCylinder;
    rem = Lba % sectorsPerCylinder;
    *Head = rem / Geometry->SectorsPerTrack;
    *Sector = (rem % Geometry->SectorsPerTrack) + 1;

    return STATUS_SUCCESS;
}
