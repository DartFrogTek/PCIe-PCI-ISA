# Bring-up sequence

## Phase A — PCI path

1. Confirm Device Manager shows `PCI\\VEN_1283&DEV_8888`.
2. Install test-signed driver.
3. Run:

```bat
it8888ctl info
it8888ctl dumpcfg
```

Record config `0x04`, `0x40`, `0x44`, `0x48`, `0x4C`, `0x50`, `0x54`.

## Phase B — ITEXXX-like init

```bat
it8888ctl init
it8888ctl info
it8888ctl trace
```

Expected recovered defaults:

```text
cfg40 = 0x83918381
cfg44 = 0x83B183A1
cfg48 = 0x83D30000
cfg4C = 0x83F383E3
cfg50 = 0x01FFF023
cfg54 = 0x8C003F3F
```

DDMA bases derived by masking low nibble:

```text
ch0 0x8380
ch1 0x8390
ch2 0x83A0
ch3 0x83B0
ch5 0x83D0
ch6 0x83E0
ch7 0x83F0
```

## Phase C — PIO probe

Use a simple safe target first, ideally OPL or a known diagnostic card.

```bat
it8888ctl out 0x388 1 0x20
it8888ctl out 0x389 1 0x01
it8888ctl in 0x388 1
```

Watch with the TLA714 to distinguish:

- config writes reach IT8888
- DDMA-window I/O reaches IT8888
- normal ISA I/O windows reach ISA side
- some accesses get swallowed by platform/bridge decode

## Phase D — DMA buffer

```bat
it8888ctl dma-alloc 65536
it8888ctl dma-info
```

Record the logical address. This is the device-visible address that layer 3 feeds into IT8888 DDMA.

## Phase E — DDMA register write trace

```bat
it8888ctl ddma-arm 1 2 0 4096 0xC0
it8888ctl trace
```

For channel 1, the bus should show writes around:

```text
0x839D master clear
0x8390 addr0
0x8391 addr1
0x8392 addr2
0x8393 addr3
0x8394 count0
0x8395 count1
0x8398 command/status
0x839B mode
0x839F mask
```

## Phase F — IRQ

If an ISA card can trigger IRQ through the bridge:

```bat
it8888ctl irq-status
it8888ctl wait-irq 5000
it8888ctl irq-status
it8888ctl irq-ack
```

## Phase G — real DMA proof

Use a card/test fixture that can assert DREQ and transfer known bytes.

1. Fill DMA buffer with known pattern.
2. Arm channel.
3. Trigger ISA device transfer.
4. Poll status.
5. Check buffer mutation.
6. Check PCI status errors.
7. Correlate TLA714 DREQ/DACK/TC.

The package does not include a DOSBox bridge. Once this phase works, the DOSBox bridge can consume this driver API.
