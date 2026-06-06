# IT8888 DOS bring-up utility

This is a DOS-side port of the current Windows/KMDF IT8888 VDMA/DDMA bring-up code.
It is intentionally a **plain DOS test utility/library**, not a resident driver yet.

## What was ported

From the Windows source, this DOS version keeps the parts that matter under DOS:

- raw PCI config access through `0xCF8/0xCFC`
- IT8888 vendor/device scan for `1283:8888`
- PCI command enable: I/O, memory, bus master
- IT8888 default DDMA config writes:
  - `cfg40 = 0x83918381`
  - `cfg44 = 0x83B183A1`
  - `cfg48 = 0x83D30000`
  - `cfg4C = 0x83F383E3`
  - `cfg50 = 0x01FFF023`
  - `cfg54 = 0x8C003F3F`
- DDMA base refresh from config `0x40/0x44/0x48/0x4C`
- direct port I/O read/write
- DDMA arm/start/poll/clear/probe
- virtual 8237 state capture helpers from the Win10 `vdma8237.c`
- PCI-to-PCI bridge I/O window programming helper, equivalent to the Windows `IOCTL_IT8888_BRIDGE_IOWIN` path

## What was removed

The Windows-only pieces are gone:

- KMDF/WDM device creation
- IOCTL queue
- WDF common buffers
- interrupt object / wait queue
- Windows resource mapping and bus interface
- kernel trace ring

In DOS, the process can directly touch PCI config space and I/O ports, and the DMA buffer can be conventional memory, so the design is much flatter.

## Build

after running `Build Environment.lnk` and inside the watcom build env:

original sequence:
```
build_watcom.bat
copy_to_dosfiles.bat
run_dosbox_it8dos.bat
```

rapid testing:
```
build_and_copy.bat
run_dosbox_it8dos.bat
```

or simply:
```
build_copy_run.bat
```

after dosbox is up and running execute 
`it8dos info`

Recommended compiler: Open Watcom C/C++.

---

```bat
build_watcom.bat
```

Equivalent command:

```bat
wcl -ml -3 -bt=dos -fe=it8dos.exe dosmain.c dos_pci.c dos_io.c dos_dma.c dos_ddma.c dos_vdma.c
```

Use real DOS or DOS 7/Win9x DOS mode first. DOSBox will not work for real hardware unless you are running on a DOSBox fork with real PCI/port passthrough.

## Basic test sequence

Find the bridge:

```bat
it8dos info
```

Apply the known working IT8888 default init:

```bat
it8dos init
```

Probe DDMA channel 1 register window:

```bat
it8dos ddmaprobe 1
```

Do a basic DDMA arm without starting hardware-side card activity:

```bat
it8dos ddmaarm 1 2 4096
```

Start/unmask after arming:

```bat
it8dos ddmastart 1 2 4096
```

Direction values:

- `1` = ISA device writes to RAM, 8237 write/device-to-memory-ish
- `2` = RAM writes to ISA device, 8237 read/memory-to-device-ish

Default flags for `ddmaarm/ddmastart` are:

```c
IT8888_DDMA_FLAG_MASTER_CLEAR | IT8888_DDMA_FLAG_UNMASK
```

## Bridge I/O window helper

If your PCIe-to-PCI bridge is not forwarding the high alias range, use:

```bat
it8dos bridge <bus> <dev> <fn> 8000 8fff
```

Example shape only:

```bat
it8dos bridge 00 1c 00 8000 8fff
```

Use the actual upstream PCI-to-PCI bridge BDF, not necessarily the IT8888 BDF.

## Important limitation

The command-line `ddmaarm/ddmastart` commands allocate a DOS buffer, arm/start, print status, then exit. That is useful for low-level register testing, but a real card transfer needs the ISA device programmed while the buffer still exists. For GUS/PicoGUS, the next step is adding a specific command that:

1. allocates the DOS DMA buffer,
2. fills it with sample data,
3. programs the GUS/PicoGUS GF1 registers,
4. arms IT8888 DDMA,
5. triggers the card-side DMA request,
6. polls/completes,
7. optionally verifies RAM/card contents.

This source is structured so that command can be added in `dosmain.c` without touching the low-level library.

---

- The PCI bus scan loop now uses `unsigned int` loop counters. The old `u8 bus; bus < 256` loop never terminated because an 8-bit value can never become 256. In DOSBox this made `it8dos init` appear to do nothing.
- The build uses `-3` because PCI config access uses 32-bit port I/O through `0xCF8/0xCFC`, so a 386+ real-mode target is the correct baseline.
- `dos_dma.c` includes `<malloc.h>` for `_fmalloc/_ffree`.
