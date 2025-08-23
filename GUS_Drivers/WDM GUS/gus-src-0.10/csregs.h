/*
 * csregs.h
 *
 * CS4231 registers (Max & PnP)
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


/* FIXME:
 CS4231 codec description incomplete
 */


#ifndef _CSREGS_H_
#define _CSREGS_H_



/*** Direct registers ***/

#define CR_RSELECT			0x1c	// CIDXR - Codec Index Address		8	rw	1,2,3
#define CR_DATA				0x1d	// CDATAP - Codec Indexed Data Port 8	rw	1,2,3
#define CR_STATUS			0x1e	// CSR1R - Codec Status Register 1	8	rw	1,2,3


// Codec Index Address (0x1c)
#define CRSEL_ADDR_MASK			0x1f	// Indirect Register Address Mask (low 5 bits in CR_RSELECT register)
								   		// The fourth bit is valid only for modes 2 and 3.
#define CRSEL_MODECHNG_ENABLE	0x40	// Mode Change Enable
#define CRSEL_INITIALIZATION	0x80	// Codec is initializing


// CR_STATUS - Codec Status Reg 1 (0x1e)
#define CSTAT_IRQ				0x01	// Global Interrupt Status
#define CSTAT_PLAYBUF_AVAIL		0x02	// Playback Channel Buffer Available
#define CSTAT_PLAY_LEFT_SAMPLE	0x04	// Playback Channel Left/Right Sample Indication
#define CSTAT_PLAY_UPPER_BYTE	0x08	// Playback Channel Upper/Lower Byte Indication
#define CSTAT_SAMPLE_ERROR		0x10	// Sample Error
#define CSTAT_RECBUF_AVAIL		0x20	// Record Channel Data Available
#define CSTAT_REC_LEFT_SAMPLE	0x40	// Record Channel Left/Right Sample Indication
#define CSTAT_REC_UPPER_BYTE	0x80	// Record Channel Upper/Lower Byte Indication



/*** Indirect registers ***/

#define CREG_ADC_LEFT			0x00	// Left input control register		8	rw	1,2,3
#define CREG_ADC_RIGHT			0x01	// Right input control register		8	rw	1,2,3
#define CREG_SYNTH_LEFT			0x02	// Left Aux #1 input control		8	rw	1,2,3
#define CREG_SYNTH_RIGHT		0x03	// Right Aux #1 input control		8	rw	1,2,3
#define CREG_CDIN_LEFT			0x04	// Left Aux #2 input control		8	rw	1,2,3
#define CREG_CDIN_RIGHT			0x05	// Right Aux #2 input control		8	rw	1,2,3
#define CREG_DAC_LEFT			0x06	// Left output control				8	rw	1,2,3
#define CREG_DAC_RIGHT			0x07	// Right output control				8	rw	1,2,3

#define CREG_PLAY_DATA_FORMAT	0x08	// CPDFI - Playback Data Format		8	r(w)1,2,3
#define CREG_CONFIG				0x09	// CFIG1I - Configuration Reg. 1	8	rw	1,2,3
#define CREG_EXT_CTRL			0x0a    // CEXTI - External Control         8   rw  1,2,3
#define CREG_STATUS2			0x0b	// CSR2I - StatusRregister 2		8	r	1,2,3
#define CREG_MODE_SELECT		0x0c	// CMODEI - ModeSelect, ID			8	rw  1,2,3
#define CREG_LOOPBACK			0x0d	// CLCI - Loopback Control			8   rw	1,2,3
#define CREG_PCOUNT_HI			0x0e	// CUPCTI - Upper Playback Count	8	rw	1,2,3 (loads)
#define CREG_PCOUNT_LO			0x0f	// CLPCTI - Lower Playback Count	8	rw	1,2,3

#define CREG_CONFIG2			0x10	// CGIF2I - Configuration Reg. 2	8	rw	2,3
#define CREG_CONFIG3			0x11	// CFIG3I - Configuration Reg. 3	8	rw	3
#define CREG_LINEIN_LEFT		0x12	// CLLICI - Left LineIn Control		8	rw	2,3
#define CREG_LINEIN_RIGHT		0x13	// CRLICI - Right LineIn Control	8	rw	2,3
#define CREG_TCOUNT_HI			0x14	// CUTIMI - Upper Timer Count		8	rw	2,3
#define CREG_TCOUNT_LO			0x15	// CLTIMI - Lower Timer Count		8	rw	2,3 (loads)
#define CREG_MICIN_LEFT			0x16	// CLMICI - Left MicIn Control		8	rw	3
#define CREG_MICIN_RIGHT		0x17	// CRMICI - Right MicIn Control		8	rw	3
#define CREG_IRQ_STATUS			0x18    // CSR3I - Codec Status Register 3  8   rw	2,3
#define CREG_LINEOUT_LEFT		0x19	// CLOAI - LineOut Lt Attenuation	8	rw	3
#define CREG_MONO_IO_CTRL		0x1a	// CMONOI - Mono I/O Control		8   rw	2,3
#define CREG_LINEOUT_RIGHT		0x1b	// CROAI - LineOut Rt Attenuation	8	rw	3
#define CREG_REC_DATA_FORMAT	0x1c	// CRDFI - Record Data Format		8	r(w) 2,3
#define CREG_VARIABLE_FREQ		0x1d	// CPVFI - Playback Var. Freq.		8	rw	3
#define CREG_RCOUNT_HI			0x1e	// CURCTI - Upper Record Count		8	rw	2,3 (loads)
#define CREG_RCOUNT_LO			0x1f	// CLRCTI - Lower Record Count		8	rw	2,3


// ADC input control (0x00 / 0x01)
#define CADC_GAIN_MASK		0x0f	// 0 to +22.5 dB
#define CADC_SRC_MASK		0xc0	// ADC source
#define CADC_SRC_LINE		0x00	//  Line-in
#define CADC_SRC_SYNTH		0x40	//  Aux1 (synth)
#define CADC_SRC_MIC		0x80	//  Stereo mic in
#define CADC_SRC_MIXER		0xc0	//  Mixer output ("what u hear")


// Gain control (0x02/0x03, 0x04/0x05, 0x12/0x13, 0x16/0x17)
#define CGAIN_GAIN_MASK		0x1f	// +12 to -34.5 dB
#define CGAIN_MUTE			0x80


// DAC attenuation control (0x06 / 0x07)
#define CDAC_ATTN_MASK		0x3f	// 0 to -94.5 dB
#define CDAC_MUTE			0x80


// Playback / record Data Format (0x08 / 0x1c)
#define CDF_FREQ_MASK		0x0f	// Frequency field mask (shared in modes 1 and 2)
#define CDF_DATAF_MASK		0xf0	// Data format field mask (shared in mode 1)
#define CDF_XTAL1			0x00	// Crystal select - XTAL2 or XTAL1
#define CDF_XTAL2			0x01
#define CDF_STEREO			0x10	// Playback Stereo/Mono Select
#define CDF_8				0x00	// 8bit unsigned
#define CDF_U_LAW			0x20	// U-law
#define CDF_16LE			0x40	// 16 bit signed, little endian
#define CDF_A_LAW			0x60	// A-law
#define CDF_ADPCM			0xa0	// IMA-compliant ADPCM (invalid in mode 1)
#define CDF_16BE			0xc0	// 16 bit signed, big endian (invalid in mode 1)


// Configuration register 1 (0x09)
#define CCFG_PLAYBACK		0x01	// Enable playback
#define CCFG_RECORD			0x02	// Enable recording
//Other bits are write-protected by default, default settings seem ok to me
#define CCFG_SINGLE_DMA		0x04	// 1 or 2 Channel DMA Operation Select (?should be set?)


// CREG_EXT_CTRL - External Control (0x0a)
#define CEXTCTRL_IRQ_ENABLE	0x02    // Enable codec interrupts


// Status 2 (0x0b)
#define CSTAT2_IN_CALIB     0x20	// Calibrating (only emulated on Interwave)


// Codec Mode Select (0x0c)
/* Note:
 InterWave Codec operation modes:
 The InterWave codec is fully register-compatible with the CS4231 (modes 1 and 2) and the
 AD1848 devices. The InterWave IC uses an indirect addressing mechanism for accessing
 most of the codec registers. In mode 1, there are 16 indirect registers; in mode 2, there are
 28 indirect registers; and in mode 3, there are 32 indirect registers.

 Gravis UltraSound MAX should contain original CS4231 codec - modes 1 and 2 only.

 Why should be mode 3 supported:
 - independent playback and recording frequencies
 - better mixer
 ? anything else
 */
#define CODEC_MODE1			0x00
#define CODEC_MODE2			0x40	// This is our mode
#define CODEC_MODE3			0x6c	// Should be supported in future


// Loopback control (0x0d) - attenuation between input-to-adc and output-from-dac
#define CLOOP_ENABLE		0x01	// Unmute
#define CLOOP_ATTN_MASK		0xfc	// 0 to -94.5 dB


// Configuration register 2 (0x10)
#define CCFG2_CENTER_DAC	0x01	// DAC out. force enable (reset to center if FIFO underruns)
#define CCFG2_PCOUNT_OFF	0x10	// Disable playback counter (mode 3 only)
#define CCFG2_RCOUNT_OFF	0x20	// Disable record counter (mode 3 only)
#define CCFG2_TIMER			0x40	// Enable timer
#define CCFG2_FULL_VOLTAGE	0x80	// Output voltage (default is 0)


// Configuration register 3 (0x11)
#define CCFG3_JUST_A_BIT			0x01	// Nonsense
#define CCFG3_SYNTH_TO_AUX1			0x02	// Source of Aux1 is Synth output if high
#define CCFG3_VARIABLE_PLAYBACK		0x04	// Enable variable frequency playback
#define CCFG3_ADPCM_REC_SUSPEND		0x08	// Pause ADPCM recording
#define CCFG3_FIFO_THRESHOLD_MASK	0x30
#define CCFG3_PLAYBACK_IRQ			0x40	// Enable playback IRQs
#define CCFG3_RECORD_IRQ			0x80	// Enable recording IRQs


// CREG_IRQ_STATUS - Irq status register (0x18)
#define CIRQSTAT_PLAY_UNDERRUN	0x01    // Playback FIFO underrun
#define CIRQSTAT_PLAY_OVERRUN	0x02    // Playback FIFO overrun
#define CIRQSTAT_REC_OVERRUN	0x04    // Record FIFO overrun
#define CIRQSTAT_REC_UNDERRUN	0x08    // Record FIFO underrun
#define CIRQSTAT_PLAYBACK_IRQ	0x10    // Playback interrupt
#define CIRQSTAT_RECORD_IRQ		0x20    // Record interrupt
#define CIRQSTAT_TIMER_IRQ		0x40    // Timer interrupt


// Left/right output attenuation (0x19 / 0x1b)
#define COUT_ATTN_MASK		0x1f	// 0 to -46.5 dB
#define COUT_MUTE			0x80


// Mono input/output control (0x1a)
#define CMONO_ATTN_IN_MASK	0x1f	// 0 to -45 dB
#define CMONO_MUTE_OUT		0x40
#define CMONO_MUTE_IN		0x80


// Destination for mixer settings
//#define CHANNEL_LEFT		0x00
//#define CHANNEL_RIGHT		0x01
//#define LEFT_CHANNEL		CHANNEL_LEFT
//#define RIGHT_CHANNEL		CHANNEL_RIGHT



/*** Codec flags ********/

//#define CODECF_SINGLE_DMA	0x01	// See codec.cpp for details.
									// NOTE: It doesn't mean that dram_dma == rec_dma!



#endif /* _CSREGS_H_ */
