## Sound Cards  (UNTESTED)
## PicoGUS
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x220,16,fast" \
  --claim-io "0x330,8,fast" \
  --claim-io "0x388,4,fast" \
  --enable-subtractive \
  --enable-delayed-tx \
  -o gus_config.bin
```

- I/O Space 0: 0x220-0x22F (16 bytes, fast decode)
- I/O Space 1: 0x330-0x337 (8 bytes, fast decode)
- I/O Space 2: 0x388-0x38B (4 bytes, fast decode)

---
---
---

# Floppy Drive Controllers (UNTESTED)
## Standard FDC Configuration
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x3F0,8,fast" \
  --enable-subtractive \
  --enable-delayed-tx \
  -o fdc_config.bin
```
Primary I/O Range: 0x3F0-0x3F7 (8 bytes)
- 0x3F0: Status Register A (SRA)
- 0x3F1: Status Register B (SRB)
- 0x3F2: Digital Output Register (DOR)
- 0x3F3: Tape Drive Register (TDR)
- 0x3F4: Main Status Register (MSR) / Data Rate Select (DSR)
- 0x3F5: Data Register (FIFO)
- 0x3F6: Reserved
- 0x3F7: Digital Input Register (DIR) / Configuration Control Register (CCR)

Hardware Resources:
- IRQ 6 (handled by system, not bridge)
- DMA Channel 2 (handled by bridge's DDMA controller)

## Dual FDC Configuration
If you need both primary and secondary controllers:
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x3F0,8,fast" \
  --claim-io "0x370,8,fast" \
  --enable-subtractive \
  --enable-delayed-tx \
  -o dual_fdc_config.bin
```
- Secondary I/O Range: 0x370-0x377 (8 bytes)

## FDC Legacy Support
If you want maximum compatibility with older FDC detection routines:
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x3F0,8,medium" \
  --enable-subtractive \
  --enable-delayed-tx \
  -o legacy_fdc_config.bin
```
- Using "medium" speed instead of "fast" provides more conservative timing that some older software expects.

---
---
---

# Potential configurations for several other ISA expansion cards:

# SCSI Controllers (UNTESTED)

**Adaptec AHA-154x Series:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x330,4,fast" \
  --claim-io "0x140,16,fast" \
  --enable-subtractive \
  -o adaptec_scsi_config.bin
```

**NCR 5380-based SCSI:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x280,16,fast" \
  --enable-subtractive \
  -o ncr5380_scsi_config.bin
```

**Trantor T128 SCSI:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x350,16,fast" \
  --enable-subtractive \
  -o trantor_scsi_config.bin
```

---
---
---

# CNC/Industrial Control Cards  (UNTESTED)

**Parallel Port CNC Controller:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x378,8,medium" \
  --claim-io "0x278,8,medium" \
  --enable-subtractive \
  -o parallel_cnc_config.bin
```

**Dedicated CNC Motion Controller:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x300,32,fast" \
  --enable-subtractive \
  -o motion_controller_config.bin
```

**Multi-Axis Stepper Controller:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x200,32,medium" \
  --claim-io "0x240,16,medium" \
  --enable-subtractive \
  -o stepper_controller_config.bin
```

---
---
---

# Network Cards (UNTESTED)

**NE2000 Compatible Ethernet:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x300,32,fast" \
  --enable-subtractive \
  -o ne2000_config.bin
```

**3Com 3C509 EtherLink III:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x300,16,fast" \
  --enable-subtractive \
  -o 3c509_config.bin
```

---
---
---

# Multi-Serial Cards (UNTESTED)

**4-Port Serial Controller:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x2F8,8,fast" \
  --claim-io "0x3E8,8,fast" \
  --claim-io "0x2E8,8,fast" \
  --claim-io "0x3F8,8,fast" \
  --enable-subtractive \
  -o quad_serial_config.bin
```

---
---
---

# Data Acquisition Cards (UNTESTED)

**ADC/DAC Card:**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x280,16,medium" \
  --claim-io "0x290,16,medium" \
  --enable-subtractive \
  -o adc_dac_config.bin
```

---
---
---

# Multi-Function Industrial Card (UNTESTED)

**Combined I/O Card (Digital I/O + Analog + Serial):**
```bash
python IT8888F_ConfigTool.py \
  --claim-io "0x200,32,medium" \
  --claim-io "0x240,16,medium" \
  --claim-io "0x2E8,8,fast" \
  --claim-io "0x2F8,8,fast" \
  --enable-subtractive \
  -o industrial_io_config.bin
```

# Important Considerations

**I/O Address Conflicts:** Many cards use overlapping ranges.

Common conflicts:
- 0x300-0x31F: NE2000 vs some CNC cards
- 0x330-0x337: SCSI vs MIDI
- 0x378-0x37F: Parallel port vs some CNC cards

**Speed Settings:**
- **Fast**: For modern cards with fast response times
- **Medium**: For older cards or those requiring setup/hold time
- **Slow**: For very old or temperamental hardware

**DMA Channels:** 

Industrial cards often need specific DMA channels:
- CNC controllers: Usually DMA 1 or 3
- SCSI controllers: Often DMA 5, 6, or 7
- Network cards: Typically DMA 1 or 3

The IT8888F's DDMA controller can handle these, but you may need to configure the specific DMA channel assignments in the bridge's DDMA registers depending on your card's requirements.