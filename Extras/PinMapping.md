Signal Function | IT8888F Pin (QFP-160) | PEX8114 Pin | Notes
| ----- | ----- | ----- | ----- |
Address/Data Bus | AD[31:0] (Pins 13–32, 140–147, 149, 152–159) | AD[31:0] | Bidirectional multiplexed bus
Command/Byte En | C/BE[3:0]# (Pins 2, 12, 23, 150) | C/BE[3:0]# | Controls bus commands
FRAME# | 3 | FRAME# | Transaction start
IRDY# | 4 | IRDY# | Initiator ready
TRDY# | 5 | TRDY# | Target ready
STOP# | 7 | STOP# | Transaction termination
DEVSEL# | 6 | DEVSEL# | Device select
IDSEL | 151 | IDSEL | Config address decode
PAR | 11 | PAR | Parity
PERR# | 9 | PERR# | Parity error
SERR# | 10 | SERR# | System error
LOCK# | 8 | LOCK# | Atomic cycle
PCI Clock | PCICLK - 137 | PCI_CLK | Must be 33 MHz
Reset | PCIRST# - 135 | PCI_RST# | Shared reset
Power (3.3V) | VCC3 (Pins 1, 22, 130, 146) | VCC3 | 3.3V PCI supply
GND | Pins 18, 28, 40, 52... | GND | Shared ground

DMA Signal | IT8888F Pin | PEX8114 Pin | Notes
| ----- | ----- | ----- | -----|
PPDREQ# (PC/PCI) | 132 | REQ# | Connect to REQ# on PEX8114
PPDGNT# (PC/PCI) | 131 | GNT# | Connect to GNT# from PEX8114
IREQ# (DDMA) | 139 | REQ# | Used if DDMA is employed directly
IGNT# (DDMA) | 138 | GNT# | PCI grant for DDMA

IRQ Handling | IT8888F Pin | PEX8114 Pin | Notes
| ----- | ----- | ----- | -----|
SERIRQ (ISA IRQ) | 134 | INTA# | Connect to INTA# of PEX8114 (INTB–D# if cascaded devices exist)

Other Function | IT8888F Pin | PEX8114 Pin | Notes
| ----- | ----- | ----- | -----|
NOGO/CLKRUN# | 136 | N/A (Optional GPIO) | For controlling subtractive decode or clock management