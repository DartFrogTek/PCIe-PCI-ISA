/*
 * iwvoice.cpp
 *
 * InterWave part of CGF1Common, voice stuff
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


#include "gf1cmnc.h"


#pragma code_seg ("PAGE")

/* Switch voice to pan mode.
 N: Voice must be selected.  InterWave doesn't need to be in enhanced mode.
 */
void CGF1Common::iw_voice_set_pan_mode
	(
	void
	)
	{
	BYTE	reg;

	// Set to pan mode.
	reg = iread8 (IWREGV_SYNTH_MODE);
	reg &= (~IWVSM_OFFSET_ENABLE);
	iwrite8 (IWREGV_SYNTH_MODE, reg);
	}

#pragma code_seg ("PAGE")

/* Switch voice to offset mode.
 N: Voice must be selected and InterWave must be in enhanced mode.
 */
void CGF1Common::iw_voice_set_offset_mode
	(
	void
	)
	{
	BYTE	reg;

	// Set to offset mode.
	reg = iread8 (IWREGV_SYNTH_MODE);
	reg |= IWVSM_OFFSET_ENABLE;
	iwrite8 (IWREGV_SYNTH_MODE, reg);
	}

#pragma code_seg ()

/* Set voice stereo offsets gradually.
 The synthesizer increments or decrements the current values in voice's left and right
 stereo offsets (SROI and SLOI) by 1 each sample until until they reach the values
 contained in 'left' and 'right' arguments (SROFI) and (SLOFI).
 N: Voice must be selected, InterWave must be in enhanced mode and voice
	must be in offset mode.
	Offsets must be from 0-4095 (12 bits) range.
 */

void CGF1Common::iw_voice_set_offsets_gradually
	(
	IN WORD	left,
	IN WORD	right
	)
	{
	iwrite16 (IWREGV_RIGHT_OFFS, right << 4);
	iwrite16 (IWREGV_LEFT_OFFS, left << 4);
	}

#pragma code_seg ()

/* Set voice stereo offsets immediately.
 N: Voice must be selected, InterWave must be in enhanced mode and voice
	must be in offset mode.
	Offsets must be from 0-4095 (12 bits) range.
 */
void CGF1Common::iw_voice_set_offsets_immediately
	(
	IN WORD	left,
	IN WORD	right
	)
	{
	right = right << 4;
	left = left << 4;
	iwrite16 (IWREGV_RIGHT_OFFS_FINAL, right);
	iwrite16 (IWREGV_LEFT_OFFS_FINAL, left);
	iwrite16 (IWREGV_RIGHT_OFFS, right);
	iwrite16 (IWREGV_LEFT_OFFS, left);
	}

/****************************************** IRQ and DMA stuff *************************************/

/* DUMMY code:

//
// IRQ Channel 1 : UICI[2:0]
// IRQ Channel 2 : UICI[5:3]
//
// DMA Channel 1 : UDCI[2:0]
// DMA Channel 2 : UDCI[5:3]
//
// UICI[6]==1 && UDCI[7]==0		-> All through IRQ Channel 1.
// UICI[6]==1 && UDCI[7]==1		-> All through IRQ Channel 2.
//
// UDCI[6]==1 -> All throug DMA Channel 1.
//
//
	{
	BYTE	reg;
	BYTE	uici;
	BYTE	udci;

	//
	// Read UICI a UDCI registers.
	//

	// Enable access & read to hidden regs.
	reg = iread8 (IWREG_VERSION);
	reg |= IWVER_HIDDEN_REG_UNLOCK | IWVER_REG_READ_MODE;	// Ability to read from GF1R_REG_CTRL.
	iwrite8 (IWREG_VERSION, reg);

	// Set UMCR[6]=1 (select UICI).
	reg = dread (GF1R_MIX_CTRL);
	reg |= GF1MIX_CONTROL;			// Select UICI (Interrupt Control register).
	dwrite (GF1R_MIX_CTRL, reg);

	// Select UICI register selector.
	reg = dread (GF1R_REG_CTRL);	// We can read because IWREG_VERSION[4]==1.
	reg &= (~IWREGCTL_REG_SELECT);	// Clear Register Selector == Select UICI.
	dwrite (GF1R_REG_CTRL, reg);

	// Read UICI.
	uici = dread (GF1R_CONTROL);

	// SetUMCR[6]=0 (select UDCI).
	reg = dread (GF1_MIX_CTRL);
	reg &= (~GF1MIX_CONTROL);		// Select UDCI (DMA Control register).
	dwrite (GF1R_MIX_CTRL, reg);

	// Read UDCI.
	udci = dread (GF1R_CONTROL);

	// Disable access & reads.
	reg = iread8 (IWREG_VERSION);
	reg &= (~IWVER_HIDDEN_REG_UNLOCK) & (~IWVER_REG_READ_MODE);
	iwrite8 (IWREG_VERSION, reg);

	}
*/
