/*
 * codec.cpp
 *
 * Codec common stuff (initialization) - CS4231 compatible (GUS MAX & InterWave)
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


/* Close codes
 */
void CGF1Common::codec_close
	(
	void
	)

	{
	// Every codec-accesing miniport should be closed now

	if (board_revision == GF1REVISION_MAX)
		{
		// Recompute the value we write to Max control reg.
		BYTE max_ctrl;
		max_ctrl = ((BYTE) dreg_to_port_table[GF1R_MAX_CTRL] & 0xf0) >> 4;
		max_ctrl |= GF1MAX_ENABLE;
		if (dram_dma >= 4)	// FIXME: replace this >= 4 and < 4 by ... & CM_..._DMA_8
			max_ctrl |= GF1MAX_REC16;
		if (rec_dma >= 4)
			max_ctrl |= GF1MAX_PLAY16;

		// And do some strange actions, taken from GUS MAX SDK.
		dwrite (GF1R_MAX_CTRL, max_ctrl & ~0x10);
		dwrite (GF1R_MAX_CTRL, max_ctrl);
		if (dram_dma < 4)
			dwrite (GF1R_MAX_CTRL, max_ctrl & ~0x10);

		dwrite (GF1R_MAX_CTRL, max_ctrl & ~0x20);
		dwrite (GF1R_MAX_CTRL, max_ctrl);
		if (rec_dma < 4)
			dwrite (GF1R_MAX_CTRL, max_ctrl & ~0x20);
		}

	// Clear any pending irqs.
	ciwrite8 (CREG_IRQ_STATUS, 0);
	dread (CR_STATUS);
	dwrite (CR_STATUS, 0);

	// Disable codec irqs.
	ciwrite8 (CREG_EXT_CTRL, ciread8 (CREG_EXT_CTRL) & ~CEXTCTRL_IRQ_ENABLE);

	// Clear any pending irqs.
	ciwrite8 (CREG_IRQ_STATUS, 0);
	dread (CR_STATUS);
	dwrite (CR_STATUS, 0);
	}


#pragma code_seg ("PAGE")


/* Initialize Codec
 N: Board revision must be valid.
 */
void CGF1Common::codec_init
	(
	void
	)

	{
	BYTE reg;

	// Setup Max codec
	if (board_revision == GF1REVISION_MAX)
		{
		BYTE max_ctrl;

		// Configure Codec port to 3xc
		max_ctrl = ((BYTE) dreg_to_port_table[GF1R_MAX_CTRL] & 0xf0) >> 4;
		max_ctrl |= GF1MAX_ENABLE;
		if (dram_dma >= 4)
			max_ctrl |= GF1MAX_REC16;
		if (rec_dma >= 4)
			max_ctrl |= GF1MAX_PLAY16;
		dwrite (GF1R_MAX_CTRL, max_ctrl);
		GF1_DELAY (1);	// Probably not needed.
		}

	if (board_revision == GF1REVISION_PNP)
		{
		// Interwave - use mode 3
		ciwrite8 (CREG_MODE_SELECT, CODEC_MODE3);
		iw_reset_codec();
		codec_mode = CODEC_MODE3;
		}
	else
		{
		// Max supports mode 2 only
		ciwrite8 (CREG_MODE_SELECT, CODEC_MODE2);
		codec_mode = CODEC_MODE2;
		}

	/* FIXME:
	 Set single DMA flag? It's probably useless if we don't try to start both
	 playback and recording if DMAs are shared.
	 */

	// Reset mixer to default state. (FIXME: should be done in topology)
	ciwrite8 (CREG_ADC_LEFT, CADC_SRC_MIXER);
	ciwrite8 (CREG_ADC_RIGHT, CADC_SRC_MIXER);
	ciwrite8 (CREG_SYNTH_LEFT, 0x08);
	ciwrite8 (CREG_SYNTH_RIGHT, 0x08);
	ciwrite8 (CREG_CDIN_LEFT, 0x08);
	ciwrite8 (CREG_CDIN_RIGHT, 0x08);
	ciwrite8 (CREG_DAC_LEFT, 0x00);
	ciwrite8 (CREG_DAC_RIGHT, 0x00);
	ciwrite8 (CREG_LINEIN_LEFT, 0x08);
	ciwrite8 (CREG_LINEIN_RIGHT, 0x08);
	ciwrite8 (CREG_CONFIG2, CCFG2_CENTER_DAC);

	// Clear any pending irqs.
	ciwrite8 (CREG_IRQ_STATUS, 0);
	dwrite (CR_STATUS, 0);

	// Enable codec irqs.
    ciwrite8 (CREG_EXT_CTRL, ciread8 (CREG_EXT_CTRL) | CEXTCTRL_IRQ_ENABLE);

	GF1_DBGINT (L"CodecInitialized", 1);
	}


#pragma code_seg()


	/* Get codec mode
	 */
ULONG CGF1Common::get_codec_mode
	(
	void
	)

	{
	return codec_mode;
	}
