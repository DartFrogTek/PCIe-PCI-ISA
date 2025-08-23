/*
 * iwregs.h
 *
 * Interwave-specific registers
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


#ifndef _IWREGS_H_
#define _IWREGS_H_


// Lengths of I/O port spaces
#define IW_P2XR_IO_SPACE_LENGTH		0x10	// Compatibility Base Port I/O space length
#define IW_P3XR_IO_SPACE_LENGTH		0x8		// MIDI and Synth Base Port I/O space length
#define IW_PCODAR_IO_SPACE_LENGTH	0x4		// Base Port for Codec I/O space length

// Base Port address masks
#define IW_P2XR_MASK		(~0xf)
#define IW_P3XR_MASK		(~0x7)
#define IW_PCODAR_MASK		(~0x3)

// InterWave internal modes.
#define IW_GUS_COMPATIBLE_MODE		(0)
#define IW_ENHANCED_MODE			(1)

#define IW_MAX_DRAM_SUPPORTED		(16*1024*1024)	// 16MB
#define IW_MAX_DRAM_IN_BANK			(4*1024*1024)	// 4MB
#define IW_MEMORY_STEP				(65536)



/*** Interwave indirect registres. ***/

// Global regs
#define IWREG_SYNTH_GLOB_MODE	0x19
#define IWREG_MEM_CONFIG		0x52	// LMCFI - LMC Configuration		16	rw
#define IWREG_MEM_CTRL			0x53	// LMCI - LMC Control				8	rw
#define IWREG_VERSION			0x5b


// Voice regs
#define IWREGV_RIGHT_OFFS		0x0c	// SROI — Synthesizer Right Offset		16	rw
#define IWREGV_LEFT_OFFS		0x13	// SLOI — Synthesizer Left Offset		16	rw
#define IWREGV_RIGHT_OFFS_FINAL	0x1b	// SROFI—Synthesizer Right Offset Final	16	rw
#define IWREGV_LEFT_OFFS_FINAL	0x1c	// SLOFI—Synthesizer Left Offset Final	16	rw
#define IWREGV_SYNTH_MODE		0x15	// SMSI - Synthesizer Mode Select.		8	rw


// Synthesizer Global Mode Reg (0x19/0x99)
#define IWSM_ENH_MODE		0x01	// InterWave Ehnanced mode
#define IWSM_LFO			0x02	// Global LFO Enable (enables operation of all LFOs)


// LMC Configuration (0x52)
#define IWMEMCFG_DRAMCFG	0x0f		// DRAM Configuration (low 4bits)
										// Bank3 Bank2 Bank1 Bank0
#define IWMEMCFG_DRAMCFG_0002	0x00	// 0,	 0,	   0,    256k
#define IWMEMCFG_DRAMCFG_0022	0x01	// 0,	 0,	   256k, 256k
#define IWMEMCFG_DRAMCFG_2222	0x02	// 256k, 256k, 256k, 256k
#define IWMEMCFG_DRAMCFG_0012	0x03	// 0,	 0,	   1M,   256k
#define IWMEMCFG_DRAMCFG_1112	0x04	// 1M,	 1M,   1M,   256k
#define IWMEMCFG_DRAMCFG_0122	0x05	// 0,	 1M,   256k, 256k
#define IWMEMCFG_DRAMCFG_1122	0x06	// 1M,	 1M,   256k, 256k
#define IWMEMCFG_DRAMCFG_0001	0x07	// 0,	 0,    0,    1M
#define IWMEMCFG_DRAMCFG_0011	0x08	// 0,	 0,    1M,   1M
#define IWMEMCFG_DRAMCFG_1111	0x09	// 1M,	 1M,   1M,   1M
#define IWMEMCFG_DRAMCFG_0004	0x0a	// 0,	 0,    0,    4M
#define IWMEMCFG_DRAMCFG_0044	0x0b	// 0,	 0,    4M,   4M
#define IWMEMCFG_DRAMCFG_4444	0x0c	// 4M,	 4M,   4M,   4M


// LMC Control (0x53)
#define IWMEM_AUTO_INC		0x01	// Auto-increment
#define IWMEM_ACCESS_ROM	0x02	// IO cycles access ROM memory


// IWREG_VERSION (0x5b)
#define IWVER_HIDDEN_REG_UNLOCK		0x01	// Unconditionally enables accesses to hidden registers
											// to UHRDP (Gus Hidden Register Data Port.
#define IWVER_MPU_EMUL				0x02	// MPU-401 Emulation Mode.
#define IWVER_PULL_UP_POWER			0x04	// Pull-Up Power.
#define IWVER_REG_READ_MODE			0x08	// Register Read Mode.
#define IWVER_VERSION_NUM			0xf0	// IC die Version Number.


// InterWave can read from GF1R_REG_CTRL (if IVERI[4]==1).
#define IWREGCTL_REG_SELECT			0x07	// Register Selector.
#define IWREGCTL_GPI1				0x08	// General Purpose 1 Interrupt.
#define IWREGCTL_GPI2				0x10	// General Purpose 2 Interrupt.
#define IWREGCTL_TOGGLE_2XC			0x20	// Toggle UI2XCR[7].
#define IWREGCTL_ENABLE_GPR_ACCESS	0x40	// Enable General Purpose Register Access.
#define IWREGCTL_ENABLE_2XE			0x80	// Enable U2XER Read Interrupts.


// UDMI - DMA Channel Control.
#define IWDMACTL_CHANNEL1			0x07	// DMA Select Channel 1.
#define IWDMACTL_CHANNEL2			0x38	// DMA Select Channel 2.
#define IWDMACTL_COMBINE			0x40	// Combine DMA Channels.
#define IWDMACTL_EXTRA_INT			0x80	// Extra Interrupt.


// UICI - Interrupt Control.
#define IWIRQCTL_CHANNEL1			0x07	// Channel 1 IRQ Selection
#define IWIRQCTL_CHANNEL2			0x38	// Channel 2 IRQ Selection
#define IWIRQCTL_COMBINE			0x40	// Combine Interrupt Channels.
#define IWIRQCTL_ADLIB_SB_TO_NMI	0x80	// AdLib–Sound Blaster to NMI.


// SMSI - Synthesizer Mode Select (0x15/0x95)
#define IWVSM_FX_ENABLE		0x01	// Effects Processor Enable.
#define IWVSM_DEACTIVATE	0x02	// Deactivate Voice.
#define IWVSM_ALT_FX_PATH	0x10	// Alternate Effects Path.
#define IWVSM_OFFSET_ENABLE	0x20	// Offset Enable.
#define IWVSM_U_LAW			0x40
#define IWVSM_ROM			0x80


#endif /* _IWREGS_H_*/
