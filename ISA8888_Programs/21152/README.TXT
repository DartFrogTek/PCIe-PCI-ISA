Key Address Ranges for These Audio Chips:

AU8830 (Aureal Vortex) typically uses:
	I/O 0x0200-0x03FF: Main audio registers and control
	I/O 0x0200-0x0207: Gameport interface
	I/O 0x0330-0x0331: MIDI interface
	Memory 0xA0000-0xFFFFF: DMA buffers and memory-mapped registers

YMF744 (Yamaha DS-XG) typically uses:
	I/O 0x0200-0x03FF: Main audio registers
	I/O 0x0220-0x022F: Sound Blaster compatibility
	I/O 0x0330-0x0331: MIDI interface
	I/O 0x0388-0x038F: OPL3 synthesis
	I/O 0x0200-0x0207: Gameport
	Memory 0xA0000-0xFFFFF: Audio buffers and wavetable