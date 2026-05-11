# IT8888VDMA Win10 pre-DOSBox stack

This package is the Windows 10 stack up to, but not including, a DOSBox bridge.

It includes:

- `driver/` — KMDF PCI diagnostic/DDMA driver for `PCI\\VEN_1283&DEV_8888`.
- `tools/it8888ctl/` — user-mode CLI for config, I/O, virtual 8237, DDMA, IRQ, trace, and reset testing.
- `docs/` — bring-up notes and test sequence.

## Implemented layers

1. PCI/KMDF binding and config-space access.
2. ITEXXX-like default init for IT8888 DDMA windows.
3. Controlled ISA I/O port access allowlist.
4. KMDF common DMA buffer allocation and logical-address reporting.
5. VDMA8-like virtual 8237 shadow state machine.
6. IT8888 DDMA slave-window programming from driver-owned DMA buffer.
7. IRQ event plumbing and user wait/ack/status commands.
8. Driver trace ring and panic reset/PCI error clear commands.

This is still a hardware-research prototype. The DDMA register semantics are based on the recovered ITEXXX pattern and IT8888G-style DDMA slave windows. Use the trace output and TLA714 capture to verify every write.

## First test sequence

```bat
it8888ctl.exe info
it8888ctl.exe dumpcfg
it8888ctl.exe init
it8888ctl.exe info
it8888ctl.exe dma-alloc 65536
it8888ctl.exe dma-info
it8888ctl.exe trace
```

## VDMA channel 1 test sequence

```bat
it8888ctl.exe vreset
it8888ctl.exe vout 0x0C 0x00
it8888ctl.exe vout 0x02 0x00
it8888ctl.exe vout 0x02 0x34
it8888ctl.exe vout 0x03 0xFF
it8888ctl.exe vout 0x03 0x0F
it8888ctl.exe vout 0x83 0x12
it8888ctl.exe vout 0x0B 0x49
it8888ctl.exe vout 0x0A 0x01
it8888ctl.exe vsnap
it8888ctl.exe vprepare 1
```

Expected reconstructed legacy request:

- channel 1
- address around `0x123400`
- count `4096`
- direction derived from mode `0x49`

## DDMA hardware programming test

Dry run:

```bat
it8888ctl.exe ddma-arm 1 2 0 4096 0x1
it8888ctl.exe ddma-status
```

Program DDMA regs without software request:

```bat
it8888ctl.exe ddma-arm 1 2 0 4096 0xC0
it8888ctl.exe ddma-status
```

Program and issue software request:

```bat
it8888ctl.exe ddma-arm 1 2 0 4096 0x1C0
it8888ctl.exe ddma-poll
```

Flags:

- `0x01` dry run
- `0x40` master clear before programming
- `0x80` unmask after programming
- `0x100` software request after programming
- `0x200` skip ITEXXX-like cfg init
- `0x400` poll after arm

## Safety commands

```bat
it8888ctl.exe panic-reset
it8888ctl.exe clear-errors
it8888ctl.exe trace-clear
it8888ctl.exe trace
```

## Build notes

It is intentionally written as a WDK/VS2022 starting point and may require small project-setting or API nits on first build.

Recommended:

- Windows 10 x64 development machine
- Visual Studio 2022
- Windows 10 WDK
- Test signing enabled

```bat
bcdedit /set testsigning on
```

Then build the driver project and install the INF with `pnputil`.

## Build/install scripts

See `BUILD.md` for the Windows 10 build flow. The short version:

```bat
build_all.bat Debug x64
scripts\enable_testsigning_admin.bat
REM reboot
scripts\make_test_cert_and_sign_admin.bat Debug x64
scripts\install_driver_admin.bat Debug x64
dist\Debug_x64\it8888ctl.exe info
```
