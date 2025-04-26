# Potential PCIe to PCI to ISA BUS pathway.
This repository contains information and files on how to potentially provide modern systems with an ISA bus including ISA DMA.

More information is available on the "dISAppointment - LPC to ISA adapter - ISA on modern motherboard" thread.

https://www.vogons.org/viewtopic.php?t=93291

## IT8888 based ISA8888 PCI to ISA Card
- This card/breakout board should allow a user-provided ISA Card to be translated into a PCI card via IT8888.
- ISA DMA is facilitated by PCI bus mastering through the IT8888 chip.
- Project is in KiCAD format.
- *WIP*

## ACPI SSDT 
- A way to configure motherboard bus bridges to allow I/O forwarding, given to me by an absolute legend.
- *Boilerplate implementation*

## DXE Driver
- Last resort, given to me by an absolute legend.
- *Boilerplate implementation*

## CHIPSEC
- Various LPC DMA tests with CHIPSEC
- Scripts get placed in 'chipsec-1.13.11\chipsec\modules\common'
- Tests are ran with:
  - "python chipsec_main.py -m common.lpc_dma_check"
  - "python chipsec_main.py -m common.lpc_dma_z390_test"
  - "python chipsec_main.py -m common.lpc_dma_h81_z390_test"
- *Tests work, but might crash your system.*

## Extras
- Datasheets, Documents, Pin Mappings.

## Thanks
- Everyone at Vogons, seriously thanks.
- Absolute Legend #1, #2, and #3. [Names redacted to protect their jobs]
- My wife.

Without my wife, everyone at Vogons, and the legends, none of this would likely exist. 
