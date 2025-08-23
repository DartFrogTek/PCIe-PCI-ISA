/*
 * gf1cmn.cpp
 *
 * GF1 common class, other things
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


/* Slower write hack test
 ! I can't believe it, but this obviously causes hangups (in GF1 waveout).
 */
//#define		GF1_DELAY_IWRITE



	/************************************ Variable access *************************************/

#pragma code_seg()


/* Write to mixer
 */
NTSTATUS CGF1Common::write_mixer
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	CGF1Common *self = (CGF1Common *) context;
	self->dwrite (GF1R_MIX_CTRL, (BYTE) self->mixer_settings);
	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Get mixer settings
 */
BYTE CGF1Common::get_mixer
	(
	void
	)

	{
	PAGED_CODE();

	return (BYTE) (mixer_settings & (GF1MIX_NOLINEIN | GF1MIX_NOLINEOUT | GF1MIX_MICIN));
	}


#pragma code_seg()


/* Set mixer settings
 */
void CGF1Common::set_mixer
	(
	IN BYTE		mix
	)

	{
	// FIXME: changes to mixer_settings should be probably synchronized too
	mixer_settings &= ~(GF1MIX_NOLINEIN | GF1MIX_NOLINEOUT | GF1MIX_MICIN);
	mixer_settings |= mix & (GF1MIX_NOLINEIN | GF1MIX_NOLINEOUT | GF1MIX_MICIN);
	interrupt_sync->CallSynchronizedRoutine (&write_mixer, this);

	#if GF1_DBG
	if (KeGetCurrentIrql() == PASSIVE_LEVEL)
		{
		GF1_DBGINT (L"MixerSettings", mix);
		}
	#endif /* GF1_DBG */
	}


#pragma code_seg()


struct CodecRegContext
	{
	BYTE		reg;		// Register index
	BYTE		value;		// Value to read or write
	BYTE		mask;		// See write_codec_reg()
	CGF1Common	*self;
	};


/* Read indirect codec register
 */
NTSTATUS CGF1Common::read_codec_reg
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)
	{
	CodecRegContext *	ctx = (CodecRegContext *) context;
	ctx->value = ctx->self->ciread8 (ctx->reg);
	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Write indirect codec register
 */
NTSTATUS CGF1Common::write_codec_reg
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)
	{
	CodecRegContext *	ctx = (CodecRegContext *) context;

	ctx->self->ciwrite8 (ctx->reg, ((ctx->self->ciread8 (ctx->reg)) & ctx->mask) | ctx->value);

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Get indirect codec register
 */
BYTE CGF1Common::get_codec_reg
	(
	IN BYTE		reg
	)
	{
	CodecRegContext		ctx;
	ctx.reg = reg;
	ctx.self = this;
	interrupt_sync->CallSynchronizedRoutine (&read_codec_reg, &ctx);
	return ctx.value;
	}


#pragma code_seg()


/* Set indirect codec register
 */
void CGF1Common::set_codec_reg
	(
	IN BYTE		reg,
	IN BYTE		mask,
	IN BYTE		value
	)
	{
	CodecRegContext		ctx;
	ctx.reg = reg;
	ctx.mask = mask;
	ctx.value = value;
	ctx.self = this;
	interrupt_sync->CallSynchronizedRoutine (&write_codec_reg, &ctx);
	}


#pragma code_seg()


/* Get board revision
 */
GF1Revision CGF1Common::get_revision
	(
	void
	)

	{
	return board_revision;
	}


#pragma code_seg()


/* Get mixing frequency
 */
ULONG CGF1Common::get_frequency
	(
	void
	)

	{
	return gf1_mixing_frequency;
	}


#pragma code_seg()


/* Get DMA resource list
 */
PRESOURCELIST CGF1Common::get_dma_resources
	(
	void
	)

	{
	return dma_resources;
	}



	/************************************ Register stuff **************************************/


#pragma code_seg()


/* Write to direct register / port
 */
void CGF1Common::dwrite
	(
	IN DWORD	reg,
	IN BYTE		val
	)

	{
	ASSERT_VALID_DREG (reg);

	WRITE_PORT_UCHAR (GET_DREG_PORT (reg), val);
	}


#pragma code_seg()


/* Read direct register / port
 */
BYTE CGF1Common::dread
	(
	IN DWORD	reg
	)

	{
	ASSERT_VALID_DREG (reg);

	return READ_PORT_UCHAR (GET_DREG_PORT (reg));
	}


#pragma code_seg()


/* Write to indirect register (8bit)
 */
void CGF1Common::iwrite8
	(
	IN BYTE		reg,
	IN BYTE		val
	)

	{
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), reg);
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8), val);
	#ifdef GF1_DELAY_IWRITE
	READ_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8));
	READ_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8));
	READ_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8));
	READ_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8));
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8), val);
	#endif /* GF1_DELAY_IWRITE */
	}


#pragma code_seg()


/* Write to indirect register (16bit)
 */
void CGF1Common::iwrite16
	(
	IN BYTE		reg,
	IN WORD		val
	)

	{
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), reg);
	WRITE_PORT_USHORT ((PUSHORT) (GET_DREG_PORT (GF1R_DATA16)), val);
	#ifdef GF1_DELAY_IWRITE
	READ_PORT_USHORT ((PUSHORT) GET_DREG_PORT (GF1R_DATA16));
	READ_PORT_USHORT ((PUSHORT) GET_DREG_PORT (GF1R_DATA16));
	READ_PORT_USHORT ((PUSHORT) GET_DREG_PORT (GF1R_DATA16));
	READ_PORT_USHORT ((PUSHORT) GET_DREG_PORT (GF1R_DATA16));
	WRITE_PORT_USHORT ((PUSHORT) (GET_DREG_PORT (GF1R_DATA16)), val);
	#endif /* GF1_DELAY_IWRITE */
	}


#pragma code_seg()


/* Read indirect register (8bit)
 */
BYTE CGF1Common::iread8
	(
	IN BYTE		reg
	)

	{
	if (!(reg & 0x40))
		reg |= 0x80;

	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), reg);
	return READ_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8));
	}


#pragma code_seg()


/* Read indirect register (16bit)
 */
WORD CGF1Common::iread16
	(
	IN BYTE		reg
	)

	{
	if (!(reg & 0x40))
		reg |= 0x80;
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), reg);
	return READ_PORT_USHORT ((PUSHORT) (GET_DREG_PORT (GF1R_DATA16)));
	}


#pragma code_seg()


/* Write byte to DRAM
 */
void CGF1Common::poke
	(
	IN DWORD	addr,
	IN BYTE		val
	)

	{
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), GF1REG_LOW);
	WRITE_PORT_USHORT ((PUSHORT) (GET_DREG_PORT (GF1R_DATA16)), (WORD) (addr & 0xffff));
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), GF1REG_HIGH);
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8), (BYTE) (addr >> 16));
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_DRAM), val);
	}


#pragma code_seg()


/* Read byte from DRAM
 */
BYTE CGF1Common::peek
	(
	IN DWORD	addr
	)

	{
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), GF1REG_LOW);
	WRITE_PORT_USHORT ((PUSHORT) (GET_DREG_PORT (GF1R_DATA16)), (WORD) (addr & 0xffff));
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_RSELECT), GF1REG_HIGH);
	WRITE_PORT_UCHAR (GET_DREG_PORT (GF1R_DATA8), (BYTE) (addr >> 16));
	return READ_PORT_UCHAR (GET_DREG_PORT (GF1R_DRAM));
	}


#pragma code_seg()


/* Write to indirect codec register (8bit)
 */
void CGF1Common::ciwrite8
	(
	IN BYTE		reg,
	IN BYTE		val
	)
	{
	BYTE r;

	ASSERT (!(reg & (~CRSEL_ADDR_MASK)));	// Address is 5 bits long.

	r = dread (CR_RSELECT);		// Read index register.
	r &= (~CRSEL_ADDR_MASK);	// Clear indirect register address.
	r |= reg;					// Set indirect register address.
	dwrite (CR_RSELECT, r);		// Write it to index register.
	dwrite (CR_DATA, val);		// Write the value.
	}


#pragma code_seg()


/* Read indirect codec register (8bit)
 */
BYTE CGF1Common::ciread8
	(
	IN BYTE		reg
	)
	{
	BYTE r;

	ASSERT (!(reg & (~CRSEL_ADDR_MASK)));	// Address is 5 bits long.

	r = dread (CR_RSELECT);		// Read index register.
	r &= (~CRSEL_ADDR_MASK);	// Clear indirect register address.
	r |= reg;					// Set indirect register address.
	dwrite (CR_RSELECT, r);		// Write it to index register.
	return dread (CR_DATA);		// Read the value.
	}



	/****************************************** DMA *******************************************/

#pragma code_seg()


/* Grab DMA channel
 */
BOOLEAN CGF1Common::get_dma
	(
	IN GF1DmaChannel	channel
	)

	{
	KIRQL		old_irql;
	BOOLEAN		result;

	// We have to use this "any ddma stuff" lock because we access 'ddma_inits'
	KeAcquireSpinLock (&ddma_lock, &old_irql);

	switch (channel)
		{
	case GF1DMACHANNEL_DRAM:
		// DRAM DMA
		if (dma_crec_allocated ||
			(dram_dma == rec_dma && (dma_rec_allocated || dma_cplay_allocated)))
			{
			result = FALSE;
			}
		else
			{
			ddma_inits++;
			if (ddma_inits == 1)
				{
				ddma_enabled = TRUE;

				if (!ddma_busy && ddma_first >= 0)
					{
					// FIXME: Required but possibly dangerous
					KeReleaseSpinLock (&ddma_lock, old_irql);
					ddma_send();
					KeAcquireSpinLock (&ddma_lock, &old_irql);
					}
				}
			result = TRUE;
			}
		break;

	case GF1DMACHANNEL_RECORD:
		// GF1 recording
		if (dma_rec_allocated || dma_cplay_allocated ||
			(dram_dma == rec_dma && (ddma_inits || dma_crec_allocated)))
			{
			result = FALSE;
			}
		else
			{
			dma_rec_allocated = TRUE;
			result = TRUE;
			}
		break;

	case GF1DMACHANNEL_CODEC_PLAYBACK:
		// CS4231 playback
		if (dma_cplay_allocated || dma_rec_allocated ||
			(dram_dma == rec_dma && (ddma_inits || dma_crec_allocated)))
			{
			result = FALSE;
			}
		else
			{
			dma_cplay_allocated = TRUE;
			result = TRUE;
			}
		break;

	case GF1DMACHANNEL_CODEC_RECORD:
		// CS4231 recording
		if (dma_crec_allocated || ddma_inits ||
			(dram_dma == rec_dma && (dma_rec_allocated || dma_cplay_allocated)))
			{
			result = FALSE;
			}
		else
			{
			dma_crec_allocated = TRUE;
			result = TRUE;
			}
		break;
		}

	KeReleaseSpinLock (&ddma_lock, old_irql);

	return result;
	}


/* Free DMA channel
 */
void CGF1Common::put_dma
	(
	IN GF1DmaChannel	channel
	)

	{
	KIRQL		old_irql;

	// We have to use this "any ddma stuff" lock because we access 'ddma_inits'
	KeAcquireSpinLock (&ddma_lock, &old_irql);

	switch (channel)
		{
	case GF1DMACHANNEL_DRAM:
		// DRAM DMA
		if (ddma_inits)
			{
			ddma_inits--;
			if (!ddma_inits)
				ddma_enabled = FALSE;
			}
		break;

	case GF1DMACHANNEL_RECORD:
		// GF1 recording
		dma_rec_allocated = FALSE;
		break;

	case GF1DMACHANNEL_CODEC_PLAYBACK:
		// CS4231 playback
		dma_cplay_allocated = FALSE;
		break;

	case GF1DMACHANNEL_CODEC_RECORD:
		dma_crec_allocated = FALSE;
		break;
		}

	KeReleaseSpinLock (&ddma_lock, old_irql);
	}



	/***************************************** Hacks ******************************************/

// This whole driver should probably be placed here
