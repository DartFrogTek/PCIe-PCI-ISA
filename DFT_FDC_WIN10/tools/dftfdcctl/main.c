#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "public.h"

static void usage(void)
{
    printf("dftfdcctl - pass-1 ISA 5.25 floppy diagnostics\n");
    printf("\n");
    printf("Commands:\n");
    printf("  version\n");
    printf("  probe-bridge\n");
    printf("  probe-fdc\n");
    printf("  geometry\n");
    printf("  geometry 360k|1200k\n");
    printf("  transfer pio|ddma\n");
    printf("  reset\n");
    printf("  recal\n");
    printf("  seek <cyl> [head]\n");
    printf("  read-id\n");
    printf("  read-sector <lba> <out.bin>\n");
    printf("  irq-count\n");
}

static HANDLE open_device(void)
{
    HANDLE h = CreateFileW(DFTFDC_USER_DEVICE_PATH_W,
                           GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Open %S failed: %lu\n", DFTFDC_USER_DEVICE_PATH_W, GetLastError());
    }
    return h;
}

static BOOL ioctl_simple(HANDLE h, DWORD code, void* inbuf, DWORD inlen, void* outbuf, DWORD outlen, DWORD* ret)
{
    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(h, code, inbuf, inlen, outbuf, outlen, &bytes, NULL);
    if (!ok) {
        fprintf(stderr, "DeviceIoControl 0x%08lx failed: gle=%lu\n", code, GetLastError());
    }
    if (ret) *ret = bytes;
    return ok;
}

static void dump_bytes(const UCHAR* p, ULONG n)
{
    ULONG i;
    for (i = 0; i < n; i++) {
        if ((i % 16) == 0) printf("\n  %04lx: ", i);
        printf("%02x ", p[i]);
    }
    printf("\n");
}

static void print_result(const DFTFDC_FDC_RESULT* r)
{
    printf("status=0x%08lx result_len=%lu", r->Status, r->ResultLength);
    if (r->ResultLength) {
        printf(" result:");
        for (ULONG i = 0; i < r->ResultLength && i < DFTFDC_MAX_FDC_RESULT_BYTES; i++) {
            printf(" %02x", r->Result[i]);
        }
    }
    printf("\n");
}

static int cmd_version(HANDLE h)
{
    DFTFDC_VERSION_INFO info;
    DWORD ret;
    ZeroMemory(&info, sizeof(info));
    if (!ioctl_simple(h, IOCTL_DFTFDC_GET_VERSION, NULL, 0, &info, sizeof(info), &ret)) return 1;
    printf("%s ABI=0x%08lx version=%lu.%lu.%lu build=%lu\n",
           info.Name, info.AbiVersion, info.Major, info.Minor, info.Patch, info.Build);
    return 0;
}

static int cmd_probe_bridge(HANDLE h)
{
    DFTFDC_BRIDGE_INFO info;
    DWORD ret;
    ZeroMemory(&info, sizeof(info));
    if (!ioctl_simple(h, IOCTL_DFTFDC_PROBE_BRIDGE, NULL, 0, &info, sizeof(info), &ret)) return 1;
    printf("status=0x%08lx bus_if=%lu vendor=%04x device=%04x rev=%02x class=%02x:%02x:%02x header=%02x cmdstat=%08lx\n",
           info.Status,
           info.BusInterfaceValid,
           info.VendorId,
           info.DeviceId,
           info.RevisionId,
           info.ClassCode,
           info.SubClass,
           info.ProgIf,
           info.HeaderType,
           info.CommandStatus);
    return 0;
}

static int cmd_probe_fdc(HANDLE h)
{
    DFTFDC_FDC_PROBE_INFO info;
    DWORD ret;
    ZeroMemory(&info, sizeof(info));
    if (!ioctl_simple(h, IOCTL_DFTFDC_PROBE_FDC, NULL, 0, &info, sizeof(info), &ret)) return 1;
    printf("status=0x%08lx base=0x%04x MSR=%02x DIR=%02x irq=%lu transfer=%s\n",
           info.Status,
           info.BasePort,
           info.MainStatus,
           info.DigitalInput,
           info.IrqCount,
           info.TransferMode == DFTFDC_TRANSFER_IT8888_DDMA ? "ddma" : "pio");
    return 0;
}

static int cmd_get_geometry(HANDLE h)
{
    DFTFDC_MEDIA_GEOMETRY_INFO g;
    DWORD ret;
    ZeroMemory(&g, sizeof(g));
    if (!ioctl_simple(h, IOCTL_DFTFDC_GET_GEOMETRY, NULL, 0, &g, sizeof(g), &ret)) return 1;
    printf("geometry=%s id=%lu C=%lu H=%lu SPT=%lu BPS=%lu total=%lu rate=%lu kbps gap=%02x fmtgap=%02x rateSel=%02x\n",
           g.Name, g.GeometryId, g.Cylinders, g.Heads, g.SectorsPerTrack, g.BytesPerSector,
           g.TotalSectors, g.DataRateKbps, g.GapLength, g.FormatGapLength, g.RateSelect);
    return 0;
}

static int cmd_set_geometry(HANDLE h, const char* arg)
{
    DFTFDC_MEDIA_GEOMETRY_INFO g;
    DWORD ret;
    ZeroMemory(&g, sizeof(g));

    if (_stricmp(arg, "360k") == 0 || _stricmp(arg, "360") == 0) {
        g.GeometryId = DFTFDC_GEOMETRY_360K;
    } else if (_stricmp(arg, "1200k") == 0 || _stricmp(arg, "1.2m") == 0 || _stricmp(arg, "1200") == 0) {
        g.GeometryId = DFTFDC_GEOMETRY_1200K;
    } else {
        fprintf(stderr, "unknown geometry: %s\n", arg);
        return 1;
    }

    if (!ioctl_simple(h, IOCTL_DFTFDC_SET_GEOMETRY, &g, sizeof(g), &g, sizeof(g), &ret)) return 1;
    printf("set geometry: %s\n", g.Name);
    return 0;
}

static int cmd_transfer(HANDLE h, const char* arg)
{
    DFTFDC_TRANSFER_MODE_REQUEST req;
    ZeroMemory(&req, sizeof(req));

    if (_stricmp(arg, "pio") == 0) {
        req.TransferMode = DFTFDC_TRANSFER_PIO;
    } else if (_stricmp(arg, "ddma") == 0) {
        req.TransferMode = DFTFDC_TRANSFER_IT8888_DDMA;
    } else {
        fprintf(stderr, "unknown transfer mode: %s\n", arg);
        return 1;
    }

    if (!ioctl_simple(h, IOCTL_DFTFDC_SET_TRANSFER_MODE, &req, sizeof(req), NULL, 0, NULL)) return 1;
    printf("set transfer mode: %s\n", arg);
    return 0;
}

static int cmd_fdc_result(HANDLE h, DWORD code)
{
    DFTFDC_FDC_RESULT r;
    DWORD ret;
    ZeroMemory(&r, sizeof(r));
    if (!ioctl_simple(h, code, NULL, 0, &r, sizeof(r), &ret)) return 1;
    print_result(&r);
    return r.Status == 0 ? 0 : 2;
}

static int cmd_seek(HANDLE h, int argc, char** argv)
{
    DFTFDC_SEEK_REQUEST req;
    DFTFDC_FDC_RESULT r;
    DWORD ret;
    ZeroMemory(&req, sizeof(req));
    ZeroMemory(&r, sizeof(r));

    if (argc < 3) {
        fprintf(stderr, "seek requires cylinder [head]\n");
        return 1;
    }

    req.Cylinder = strtoul(argv[2], NULL, 0);
    req.Head = argc >= 4 ? strtoul(argv[3], NULL, 0) : 0;

    if (!ioctl_simple(h, IOCTL_DFTFDC_SEEK, &req, sizeof(req), &r, sizeof(r), &ret)) return 1;
    print_result(&r);
    return r.Status == 0 ? 0 : 2;
}

static int cmd_read_sector(HANDLE h, int argc, char** argv)
{
    DFTFDC_READ_SECTOR_REQUEST req;
    DFTFDC_READ_SECTOR_RESULT out;
    DWORD ret;
    HANDLE f;
    DWORD wrote;

    if (argc < 4) {
        fprintf(stderr, "read-sector requires <lba> <out.bin>\n");
        return 1;
    }

    ZeroMemory(&req, sizeof(req));
    ZeroMemory(&out, sizeof(out));
    req.UseChs = 0;
    req.Lba = strtoul(argv[2], NULL, 0);

    if (!ioctl_simple(h, IOCTL_DFTFDC_READ_SECTOR, &req, sizeof(req), &out, sizeof(out), &ret)) return 1;

    printf("status=0x%08lx geo=%lu lba=%lu C/H/S=%lu/%lu/%lu result_len=%lu\n",
           out.Status, out.GeometryId, out.Lba, out.Cylinder, out.Head, out.Sector, out.ResultLength);
    if (out.ResultLength) {
        printf("result:");
        for (ULONG i = 0; i < out.ResultLength && i < DFTFDC_MAX_FDC_RESULT_BYTES; i++) printf(" %02x", out.Result[i]);
        printf("\n");
    }

    if (out.Status != 0) {
        return 2;
    }

    f = CreateFileA(argv[3], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFile %s failed: %lu\n", argv[3], GetLastError());
        return 1;
    }

    if (!WriteFile(f, out.Data, DFTFDC_SECTOR_BYTES, &wrote, NULL) || wrote != DFTFDC_SECTOR_BYTES) {
        fprintf(stderr, "WriteFile failed: %lu wrote=%lu\n", GetLastError(), wrote);
        CloseHandle(f);
        return 1;
    }
    CloseHandle(f);

    printf("wrote %s (%u bytes)\n", argv[3], DFTFDC_SECTOR_BYTES);
    dump_bytes(out.Data, 128);
    return 0;
}

static int cmd_irq_count(HANDLE h)
{
    DFTFDC_IRQ_COUNT_INFO info;
    DWORD ret;
    ZeroMemory(&info, sizeof(info));
    if (!ioctl_simple(h, IOCTL_DFTFDC_GET_IRQ_COUNT, NULL, 0, &info, sizeof(info), &ret)) return 1;
    printf("irq_count=%lu\n", info.IrqCount);
    return 0;
}

int main(int argc, char** argv)
{
    HANDLE h;
    int rc = 0;

    if (argc < 2) {
        usage();
        return 1;
    }

    h = open_device();
    if (h == INVALID_HANDLE_VALUE) return 1;

    if (_stricmp(argv[1], "version") == 0) rc = cmd_version(h);
    else if (_stricmp(argv[1], "probe-bridge") == 0) rc = cmd_probe_bridge(h);
    else if (_stricmp(argv[1], "probe-fdc") == 0) rc = cmd_probe_fdc(h);
    else if (_stricmp(argv[1], "geometry") == 0 && argc == 2) rc = cmd_get_geometry(h);
    else if (_stricmp(argv[1], "geometry") == 0 && argc >= 3) rc = cmd_set_geometry(h, argv[2]);
    else if (_stricmp(argv[1], "transfer") == 0 && argc >= 3) rc = cmd_transfer(h, argv[2]);
    else if (_stricmp(argv[1], "reset") == 0) rc = cmd_fdc_result(h, IOCTL_DFTFDC_RESET_FDC);
    else if (_stricmp(argv[1], "recal") == 0) rc = cmd_fdc_result(h, IOCTL_DFTFDC_RECALIBRATE);
    else if (_stricmp(argv[1], "seek") == 0) rc = cmd_seek(h, argc, argv);
    else if (_stricmp(argv[1], "read-id") == 0) rc = cmd_fdc_result(h, IOCTL_DFTFDC_READ_ID);
    else if (_stricmp(argv[1], "read-sector") == 0) rc = cmd_read_sector(h, argc, argv);
    else if (_stricmp(argv[1], "irq-count") == 0) rc = cmd_irq_count(h);
    else {
        usage();
        rc = 1;
    }

    CloseHandle(h);
    return rc;
}
