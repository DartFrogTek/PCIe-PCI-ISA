## IT8888F ISA Bus Signal Pinout (for 8/16-bit ISA Compatibility)
ISA Function | IT8888F Pin (QFP-160) | ISA Pin | Description
----- | ----- | ----- | -----
SA[19:0] | 60–67, 70–73, 75–77, 79–83, 85–87 | A0–A19 | ISA address bus
SD[15:0] | 88–95, 97–104 | D0–D15 | ISA data bus (D0–D7 always used, D8–D15 for 16-bit)
IOR# | 58 | B3 | ISA I/O read strobe
IOW# | 59 | B4 | ISA I/O write strobe
MEMR# | 56 | B5 | ISA memory read strobe
MEMW# | 57 | B6 | ISA memory write strobe
AEN | 55 | B7 | Address Enable – high during DMA
BALE | 54 | B8 | Latches SA during address phase
ISA_CLK (CLK) | 70 | B20 | 8.33 MHz clock (from IT8888F to ISA slot)
RESETDRV (RESET) | 35 | B2 | ISA Reset output
OSC (14.318 MHz) | 51 | XTAL2 | Optional oscillator input to IT8888F
DRQ[0,1,3,5,6,7] | 115, 116, 117, 118, 119, 120 | B21, B23, B25, B27, B29, B31 | ISA DMA request lines
DACK#[0,1,3,5,6,7] | 109, 110, 111, 112, 113, 114 | B22, B24, B26, B28, B30, B32 | ISA DMA acknowledge
IRQ[3–7, 9–12, 14, 15] | Pins 53-63 (see below) | B10–B19 | ISA IRQ lines

## ISA IRQ Pin Mapping (IT8888F to ISA)
IRQ Line | IT8888F Pin | ISA Pin
----- | ----- | ----- 
IRQ3 | 63 | B10
IRQ4 | 62 | B11
IRQ5 | 61 | B12
IRQ6 | 60 | B13
IRQ7 | 59 | B14
IRQ9 | 58 | B15
IRQ10 | 57 | B16
IRQ11 | 56 | B17
IRQ12 | 55 | B18
IRQ14 | 54 | B19
IRQ15 | 53 | (Alt)

## Notes for ISA Signal Use
- Pull-up resistors are typically needed on IRQ, DRQ, DACK#, and RESETDRV lines.
- Be sure to isolate 8-bit and 16-bit signals if only partial bus support is desired.
- Ensure SA and SD lines are properly latched using BALE.

## AT24C02 EEPROM Wiring / .BIN
EEPROM Pin | Connect To
----- | -----
SDA | IT8888F SDA (Pin 33)
SCL | IT8888F SCL (Pin 34)
WP | GND (disable write-protect)
A0–A2 | GND (for address 0xA0)
VCC | 3.3V
GND | GND
Pullups | 4.7kΩ on SDA and SCL to 3.3V

Make sure to enable the EEPROM boot mode in the IT8888F via BOOTSEL pin strap to use the EEPROM for config.

For enabling the EEPROM boot mode using the TC pin (pin 64) as shown in Table 5-5 on page 12. When TC is pulled up during reset, it will set the Cfg_50h<4> bit, which enables SM-bus Boot ROM Configuration.

## IT8888 CONFIG EEPROM (AT24C02) BIN
```
:1000000050F10700004080400000100154FF3F0000DA
:10001000700000F30040000000740000A200000000D9
:10002000780000C2000000007C0000E1000000000001
:10003000580000010200000100640000010200AC0242
:10004000AA00000000000000000000000000000000AE
:00000001FF
```

This configuration comprehensively sets up the IT8888F for ISA-to-PCI bridging according to the specifications in section 6 and the register details in section 7 of the document.

The BIN format described in section 6.11 (pages 16-17), where:
- Data is grouped in 5-byte blocks
- First byte serves as an index to indicate which PCI Configuration register
- The following 4 bytes are the 32-bit data for that register

This BIN enables:
- Both PPDMA and DDMA
- Allows IRQs and DMA to flow from ISA to PCI
- Enables PCI bus mastering and memory access

### Binary Configuration Breakdown

**Line 1:**

```
:1000000050F10700004080400000100154FF3F0000DA
```
**Register 0x50 (ROM/ISA Spaces and Timing Control)**
   - Value: 0x0007F1 (reversed as F10700 in little-endian)
   - From document section 7.3.10 (pages 32-33):
     - Enables Delayed Transaction (bit 1)
     - Enables Subtractive Decode (bit 0)
     - Enables POST code snoop (bit 5)
     - Sets I/O recovery timing bits (bits 8-11)
     - Configures memory top boundary (bits 12-15)


```
0040804000001001
```
**Register 0x54 (Retry/Discard Timers, Misc. Control)**
   - Value: 0x00103F00 (reversed in little-endian)
   - From document section 7.3.11 (pages 36-37):
     - Enables DDMA-Concurrent mode (bit 31)
     - Sets retry timer to a reasonable value (bits 0-5)
     - Sets discard timer (bits 8-13)
     - Enables ISA Bus Refresh Timer (bit 26)


```
54FF3F0000
```
**Register 0x70 (Positively Decoded Memory_Space_0)**
   - Value: 0x0000F300 (reversed in little-endian)
   - From document section 7.3.18 (page 41):
     - Configures memory space for typical ISA region
     - Sets up base address at 0xF3000
     - Sets medium decoding speed (bits 29-30)
     - Enables this memory space (bit 31)


```
DA
```
**Checksum**


```
700000F30040000000
```
**Registers 0x74, 0x78, 0x7C (Memory Spaces 1-3)**
   - Values configure additional memory spaces
   - From document sections 7.3.19, 7.3.20, 7.3.21 (pages 41-42):
     - Each configures different memory ranges for ISA devices
     - Enables extended ISA memory addressing

```
740000A200000000
780000C200000000
7C0000E100000000
```
**Register 0x58 (Positively Decoded IO_Space_0)**
   - Value configures I/O space mapping
   - From document section 7.3.12 (page 38):
     - Sets up I/O space for ISA devices
     - Configures decoding speed and timing

```
580000010200000100
```
**Register 0x64 (Positively Decoded IO_Space_3)**
   - Value: 0x02AC0201
   - From document section 7.3.15 (page 39):
     - Maps additional I/O space for ISA devices
     - Typical setting for standard ISA I/O

```
640000010200AC02
```

**Termination Index 0xAA**
   - From document section 6.11 (page 16):
     - "If it reads an Index value as AAhex, then it will stop I²C Sequential Read Operation and clear the SMB_In_Progress status bit."
     - This tells the IT8888F to stop reading configuration data

```
10004000AA00000000000000000000000000000000AE
```

The Intel HEX format (:10000000...) is simply a container for the binary data, with each line containing: We use the last line for an example.
- Record length: 10 = 16 bytes
- Address offset: Starting address 0x4000
- Record type: First data byte (0xAA)
- The actual data: The remaining 16 data bytes (all zeros)
- Checksum: AE

**Final line**
```
:00000001FF
```
End-of-file record in the Intel HEX format.

---

## IT8888F PCI Bus Signal Pinout

| PCI Function | IT8888F Pin (QFP-160) | Description |
| ------------ | --------------------- | ----------- |
| AD[31:0]     | 13-17, 19-21, 24-27, 29-32, 140-145, 147, 149, 152-159 | PCI Multiplexed Address/Data Bus |
| C/BE[3:0]#   | 2, 12, 23, 150 | Command/Byte Enable signals |
| FRAME#       | 3 | Indicates beginning and duration of PCI access |
| IRDY#        | 4 | Initiator Ready signal |
| TRDY#        | 5 | Target Ready signal |
| DEVSEL#      | 6 | Device Select signal |
| STOP#        | 7 | Stop signal for current transaction |
| LOCK#        | 8 | Indicates atomic operation requiring multiple transactions |
| PERR#        | 9 | Parity Error reporting |
| SERR#        | 10 | System Error reporting |
| PAR          | 11 | Parity signal for AD and C/BE lines |
| IDSEL        | 151 | Initialization Device Select |
| IREQ#        | 139 | PCI Bus Request (for DDMA) |
| IGNT#        | 138 | PCI Bus Grant (for DDMA) |
| PPDREQ#      | 132 | PC/PCI DMA Request |
| PPDGNT#      | 131 | PC/PCI DMA Grant |
| PCICLK       | 137 | 33 MHz PCI Clock input |
| PCIRST#      | 135 | PCI Bus Reset |
| VCC3         | 1, 22, 130, 146 | 3.3V PCI Interface power |

## Notes for PCI Signal Use
- The IT8888F's PCI interface operates at 3.3V with 5V tolerant I/O buffers
- Supports 32-bit PCI bus at up to 33 MHz
- The chip complies with PCI Specification Version 2.1
- Supports both PCI Bus Master and Slave operations
- The NOGO/CLKRUN# pin (133) can be configured for either function through Register Cfg_54h<20>
- SERIRQ (pin 134) supports Serialized IRQ protocol for PCI systems

Information was compiled from Table 5-1 (PCI Bus Interface Signals) on page 9 and the pin configuration section on pages 7-8.