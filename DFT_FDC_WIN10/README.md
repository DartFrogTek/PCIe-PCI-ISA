# DFT_FDC_WIN10 — Pass 1 KMDF ISA 5.25" Floppy Bring-up Driver

This is **Pass 1** of the two-pass driver plan.

Pass 1 is a hardware bring-up and diagnostic driver:

- load a clean KMDF driver named `dftfdc.sys`
- expose a control device at `\\.\DftFdc0`
- query the IT8888 PCI config interface when available
- talk to a standard PC-compatible ISA FDC at `0x3F0-0x3F7`
- reset the FDC
- set the data rate for 360K or 1.2M 5.25" media
- recalibrate the drive
- seek
- run `READ ID`
- read sector 0 using a polling/PIO fallback path
- keep the IT8888 DDMA channel-2 hook isolated for the real DDMA path

Pass 2 will wrap the sector engine as a removable block device so Windows Mount Manager can assign any available drive letter.

## Build

From a normal Developer Command Prompt or PowerShell/CMD window:

```bat
build_all_direct.bat Debug x64
```

The script stages files into:

```text
dist\Debug_x64
```

Expected output:

```text
dftfdc.sys
dftfdc.inf
dftfdc.pdb
dftfdcctl.exe
```

## Install / test signing

Enable test signing on the target machine if needed:

```bat
bcdedit /set testsigning on
```

Reboot, then install from an elevated command prompt:

```bat
pnputil /add-driver dist\Debug_x64\dftfdc.inf /install
```

The INF matches the IT8888 PCI ID by default:

```text
PCI\VEN_1283&DEV_8888
```

If your bridge enumerates with a different hardware ID, edit `driver\dftfdc.inf`.

## User-mode diagnostic tool

Open the device:

```bat
dftfdcctl version
```

Suggested pass-1 test sequence:

```bat
dftfdcctl version
dftfdcctl probe-bridge
dftfdcctl probe-fdc
dftfdcctl geometry 360k
dftfdcctl reset
dftfdcctl recal
dftfdcctl seek 0
dftfdcctl read-id
dftfdcctl read-sector 0 boot360.bin
```

For 1.2M media:

```bat
dftfdcctl geometry 1200k
dftfdcctl reset
dftfdcctl recal
dftfdcctl read-id
dftfdcctl read-sector 0 boot1200.bin
```

## Notes

The first sector-read path uses the FDC non-DMA/polling mode so that FDC port access, motor control, seek, result phase handling, and CHS translation can be proven before the DDMA path is finalized.

The real ISA DDMA channel-2 hook is present in `driver\it8888_ddma.c` and is intentionally isolated. Fill in the IT8888-specific DDMA register sequence there once you want to flip sector I/O from polling/PIO fallback to IT8888-backed DDMA.

The project intentionally does **not** copy the old IT8888/GUS diagnostic driver files. It reuses the build/layout/IOCTL pattern with new names and a new floppy-centered implementation.

## Build note: no WindowsKernelModeDriver10.0 MSBuild toolset required

`build_all_direct.bat` intentionally uses the same practical pattern as the working
IT8888 driver bring-up: it calls VS `vcvars64.bat`, discovers the installed WDK
version, adds the WDK/KMDF include and lib paths explicitly, compiles the kernel
objects with `cl.exe`, links `dftfdc.sys` with `link.exe`, then builds the user
control tool.

This avoids depending on the Visual Studio WDK MSBuild integration/toolset name
`WindowsKernelModeDriver10.0`, which is not always installed even when the WDK
headers and libraries are present.


## pass1_compile_fix_2026_06_18

This tree uses the direct vcvars64 + explicit WDK include/lib build path and avoids the VS `WindowsKernelModeDriver10.0` MSBuild toolset dependency. The bridge PCI bus-interface GUID is defined locally in `it8888_pci.c`, and `trace.c` includes `stdarg.h` for the kernel debug trace helper.
