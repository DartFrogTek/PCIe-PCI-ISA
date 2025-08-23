/*
 * gf1regs.h
 *
 * GF1 registers
 *

   Copyright (C) 2000 contributors of the Gravis UltraSound WDM Driver project
   Please see the file "AUTHORS" for a list of contributors

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to:

        Free Software Foundation, Inc.
        59 Temple Place - Suite 330
        Boston, MA  02111-1307, USA

 *
 */


#ifndef _GF1REGS_H_
#define _GF1REGS_H_


/* FIXME:
 ICS mixer description missing (see source/icsmix.c in GUS SDK)
 Mark self-modifying registers/bits
 */



/************************************ GF1 ports (direct regs) *************************************/

// Indices into Direct register to port address mapping table
// (CGF1Common::dreg_to_port_table) table.

// 2xR
#define GF1R_MIX_CTRL		0x000	// Mix control				8	w
#define GF1R_IRQ_STATUS		0x006	// IRQ status				8	r
#define GF1R_TIMER_CTRL		0x008	// AdLib timer control		8	rw
#define GF1R_TIMER_DATA		0x009	// AdLib timer data			8	rw
#define GF1R_CONTROL		0x00b	// IRQ/DMA control / other	8	w
#define GF1R_REG_CTRL		0x00f	// 2xb register control		8	w
									//							(r InterWave only && IVERI[3]==1)

// 3xR
#define GF1R_MIDI_CTRL		0x010	// MIDI control				8	w
#define GF1R_MIDI_STATUS	0x010	// MIDI status				8	r
#define GF1R_MIDI_DATA		0x011	// MIDI data				8	rw
#define GF1R_VSELECT		0x012	// Voice select (0 - 31)	8	rw
#define GF1R_RSELECT		0x013	// Select indirect reg.		8	rw
#define GF1R_DATALOW		0x014	// Lower 8b of 16b reg.		8/16 rw
#define GF1R_DATA16			GF1R_DATALOW
#define GF1R_DATAHI			0x015	// Higher 8b / 8bit reg.	8	rw
#define GF1R_DATA8			GF1R_DATAHI
#define GF1R_MAX_CTRL		0x016	// MAX control				8	w
#define GF1R_ICSMIX			0x016	// ICS mixer				8	w
#define GF1R_DRAM			0x017	// DRAM I/O					8	rw

// 7x6
#define GF1R_REVISION		0x020	// Revision (3.7+ only)		8	r


// Mix control (2x0)
#define GF1MIX_NOLINEIN		0x01    // 0 = enable line-in
#define GF1MIX_NOLINEOUT	0x02    // 0 = enable line-out
#define GF1MIX_MICIN		0x04    // 1 = enable mic-in
#define GF1MIX_LATCHES		0x08	// Enable IRQ/DMA latches
#define GF1MIX_COMBINE		0x10    // Combine chan1 & chan2 IRQs
#define GF1MIX_LOOPBACK		0x20    // Enable MIDI loopback
#define GF1MIX_CONTROL		0x40    // Control reg. select (0 = DMA, 1 = IRQ)


// IRQ status (2x6)
#define GF1IRQS_MIDI_XMIT	0x01	// Midi transmit
#define GF1IRQS_MIDI_RECV	0x02	// Midi receive
#define GF1IRQS_TIMER1		0x04	// Timer1 tick
#define GF1IRQS_TIMER2		0x08	// Timer2 tick
#define GF1IRQS_TIMER_MASK	0x0c
#define GF1IRQS_SB			0x10	// SB/AdLib emulation stuff
#define GF1IRQS_WAVE		0x20	// Wave (rollover)
#define GF1IRQS_VOLUME		0x40	// Volume ramp
#define GF1IRQS_DMA			0x80	// Dma terminal count


// Timer control (2x8)
/* GUS SDK says:
 Writing a 4 here selects the timer stuff.
 Bit 6 will be set if timer #1 has expired.
 Bit 5 will be set if timer #2 has expired.
 */
#define GF1TCTL_T2EXPIRED	0x20
#define GF1TCTL_T1EXPIRED	0x40
#define GF1TCTL_WRITETHIS	4


// Timer data (2x9)
#define GF1TDATA_T1START	0x01
#define GF1TDATA_T2START	0x02
#define GF1TDATA_T2MASK		0x20
#define GF1TDATA_T1MASK		0x40
#define GF1TDATA_CLEAR_IRQ	0x80


// Register control (2xf)
//  Note: controls which regs are mapped to 2xb, write to 2x0 can also change this
//  Note: not available on pre-3.4 boards
#define GF1REGCTL_CLRINT	5		// Write to 2xb to clear all pending interrupts
#define GF1REGCTL_JUMPER	6		// See GF1JUMP_*


// Jumper register (2xb)
#define GF1JUMP_MIDI		0x02	// Enable MIDI
#define GF1JUMP_JOYSTICK	0x04	// Enable joystick


// MIDI control (3x0)
#define GF1MIDIC_RESET		0x03	// Write this followed by zero to reset MIDI port
#define GF1MIDIC_XMIT_IRQ	0x20	// Enable transmit IRQ
#define GF1MIDIC_RECV_IRQ	0x80	// Enable receive IRQ


// MIDI status (3x0)
#define GF1MIDIS_RECV_FULL	0x01	// Data available
#define GF1MIDIS_XMIT_EMPTY	0x02	// Ready to accept data
#define GF1MIDIS_FRAMING_ERR 0x10	// Framing error
#define GF1MIDIS_OVERRUN	0x20	// Receive FIFO overrun
#define GF1MIDIS_IRQ_PENDING 0x80	// MIDI IRQ pending


// Max control (3x6 if 7x6 >= 10), not on Interwave
#define GF1MAX_ADDR_MASK	0x0f	// x for 3xc port (usually the same as GUS port x)
#define GF1MAX_REC16		0x10	// Recording DMA is 16bit
#define GF1MAX_PLAY16		0x20	// Playback DMA is 16bit
#define GF1MAX_ENABLE		0x40	// Enable CS4231 Codec


// Revision level (7x6)
#define GF1REV_PRE37		0xff
#define GF1REV_ICSMIN       0x05	// 3.7 with ICS mixer (r/l flips)
#define GF1REV_ICSMAX		0x08	// Latest board with ICS mixer
#define GF1REV_MAXMIN		0x09	// First UltraMAX (CS4231 codec does mixer stuff)
#define GF1REV_MAXMAX		0x0f
#define GF1REV_ACEMIN		0x30	// Ace
#define GF1REV_ACEMAX		0x3f
#define GF1REV_VIPERMIN		0x50	// Viper
#define GF1REV_VIPERMAX		0x5f
#define GF1REV_2102MIN		0x60	// ???
#define GF1REV_2102MAX		0x6f



/***************************************** GF1 registers ******************************************/

// Global registers
#define GF1REG_DMA			0x41	// DRAM DMA control			8	rw
#define GF1REG_DMA_ADDR		0x42	// DMA start address		16	w
#define GF1REG_LOW			0x43	// DRAM I/O low				16	w
#define GF1REG_HIGH			0x44	// DRAM I/O high			8	w
#define GF1REG_TIMER		0x45	// Timer control			8	rw
#define GF1REG_COUNT1		0x46	// Timer 1 count			8	w
#define GF1REG_COUNT2		0x47	// Timer 2 count			8	w
#define GF1REG_SMP_FREQ		0x48	// Sampling frequency		8	w
#define GF1REG_SMP_CTRL		0x49	// Sampling control			8	rw
#define GF1REG_JOYSTICK		0x4b	// Joystick trim DAC		8	w
#define GF1REG_RESET		0x4c	// GUS reset register		8	rw


// Voice-specific registers (or with 0x80 to read)
//  Note: 0x0e and 0x0f are really not voice-specific
#define GF1REGV_VOICE_CTRL	0x00	// Voice control			8
#define GF1REGV_FREQ		0x01	// Frequency ctrl			16
#define GF1REGV_STARTH		0x02	// Start. addr. high		16
#define GF1REGV_STARTL		0x03	// Start. addr. low			16
#define GF1REGV_ENDH		0x04	// End addr. high			16
#define GF1REGV_ENDL		0x05	// End addr. low			16
#define GF1REGV_RAMP_RATE	0x06	// Vol. ramp rate			8
#define GF1REGV_RAMP_START	0x07	// Lower ramp volume		8
#define GF1REGV_RAMP_END	0x08	// Higher ramp volume		8
#define GF1REGV_VOLUME		0x09	// Current volume			16
#define GF1REGV_POSH		0x0a	// Current address high		16
#define GF1REGV_POSL		0x0b	// Current address low		16
#define GF1REGV_PANNING		0x0c	// Panning position			8
#define GF1REGV_RAMP_CTRL	0x0d	// Volume ramp control		8
#define GF1REGV_VOICES		0x0e	// Active voices			8
#define GF1REGV_VIRQ		0x0f	// Voice IRQ status			8


// DMA control register (0x41)
#define GF1DMA_ENABLE		0x01	// Enable DMA
#define GF1DMA_READ			0x02	// 1=read, 0=write
#define GF1DMA_DMA16		0x04	// DMA chan width (1 = 16bit, 0 = 8bit)
#define GF1DMA_RATE_MASK	0x18	// DMA xfer speed:
#define GF1DMA_R0			0x00	//   Fastest xfer (~650kHz)
#define GF1DMA_R1			0x08	//   Fastest / 2
#define GF1DMA_R2			0x10	//   Fastest / 4
#define GF1DMA_R3			0x18	//   Fastest / 8
#define GF1DMA_DEFAULT_RATE	GF1DMA_R1
#define GF1DMA_IRQ_ENABLE	0x20	// Generate IRQ at terminal count
#define GF1DMA_IRQ_PENDING	0x40	// Is IRQ pending?
#define GF1DMA_DATA16		0x40	// Write (data width)
#define GF1DMA_UNSIGNED		0x80	// Invert highest bit (7/15)


// Timer control register (0x45)
#define GF1TIMER_IRQ1		0x04	// Enable timer 1 IRQ
#define GF1TIMER_IRQ2		0x08	// Enable timer 2 IRQ


// Sampling control register (0x49)
#define GF1SMP_START		0x01	// Start sampling
#define GF1SMP_STEREO		0x02	// Channels (1 = stereo, 0 = mono)
#define GF1SMP_DMA16		0x04	// DMA width (1 = 16bit, 0 = 8bit)
#define GF1SMP_IRQ_ENABLE	0x20	// Generate IRQ at terminal count
#define GF1SMP_IRQ_PENDING	0x40	// Is IRQ pending?
#define GF1SMP_UNSIGNED		0x80	// Invert highest bit (7)


// GF1 reset register (0x4c)
#define GF1RES_RESET		0x01	// Master reset
#define GF1RES_DAC			0x02	// DAC enable
#define GF1RES_GF1			0x04	// GF1 master IRQ enable
#define GF1RES_ALL			0x07


// Voice control reg. (0x00/0x80)
#define GF1VC_STOPPED		0x01	// Is channel stopped? (read)
#define GF1VC_STOP			0x02	// Stop voice (write)
#define GF1VC_16BIT			0x04	// Data width
#define GF1VC_LOOPED		0x08	// Loop voice
#define GF1VC_BIDIR			0x10	// Loop is bidirectional
#define GF1VC_WAVE_IRQ		0x20	// Enable IRQ at endposition
#define GF1VC_BACKWARD		0x40	// Voice direction (1 = bwd, 0 = fwd)
#define GF1VC_IRQ_PENDING	0x80	// Voice is generating IRQ (read)


// Frequency control (0x01, 0x81) (unsigned, sign bit is in 0x00:bit6)
#define GF1FREQ_FRAC_BITS	10		// Number of fractional bits
#define GF1FREQ_FRAC_MASK	0x3fe	// Should be bits 1-9 (bit 0 is not used)


// Volume ramp rate (0x06/0x86)
#define GF1RR_INCREMENT_MASK 0x3f	// Volume increment
#define GF1RR_PERIOD_MASK	0xc0	// Volume change rate (each n-th frame)
#define GF1RR_PERIOD_1		0x00
#define GF1RR_PERIOD_8		0x40
#define GF1RR_PERIOD_16		0x80
#define GF1RR_PERIOD_64		0xc0


// Volume ramp control reg. (0x0d/0x8d)
#define GF1RC_STOPPED		0x01	// Is ramp stopped? (read)
#define GF1RC_STOP			0x02	// Stop volume ramping (write)
#define GF1RC_ROLLOVER		0x04	// Enable rollover
#define GF1RC_LOOPED		0x08	// Loop sample
#define GF1RC_BIDIR			0x10	// Loop is bidir
#define GF1RC_RAMP_IRQ		0x20	// Enable IRQ at rampend
#define GF1RC_DOWN			0x40	// Ramp direction (1=down,0 =up)
#define GF1RC_IRQ_PENDING	0x80	// Voice is generating ramp IRQ (read)


// IRQ status register (0x8f)
#define GF1VIRQ_VOICE_MASK		0x1f	// Channel number mask
#define GF1VIRQ_NOT_VOLUME		0x40	// 0 = Volume ramp IRQ occured
#define GF1VIRQ_NOT_WAVE		0x80	// 0 = Wave (rollover) IRQ occured
#define GF1VIRQ_NOT_PENDING		0xc0


#endif /* _GF1REGS_H_ */
