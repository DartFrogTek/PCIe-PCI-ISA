/*
 * voice.cpp
 *
 * Voice manipulating common for GF1 and InterWave.
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


//
// Converts pan (0-left, 127-middle, 255-right) to device specific value.
//

// Pan must be in 0-255 range.
#define GET_GF1_PAN(pan)		((BYTE)((pan)>>4))


// For InterWave (in enhanced mode) there are two separated stereo offsets.
// This macros calculate left and right offset values with constant total power for InterWave.
// FIXME: Tohle ted neni pravda! Mely by se pocotat nejaky dvojkovy lgaritmus.
//		  Predpociatat to do nejake tabulky?????
// FIXME: Staci 256 urovni panningu? Dle me urcite jo, navic plynuly prechod udela
//		  iw_voice_set_offsets_gradually().
// NOTE: Pokud bude tabulka, musi byt v nonpaged casti!
//
#define GET_IW_LEFT_PAN(pan)	((WORD)(((WORD)(255-(pan)))<<4))
#define GET_IW_RIGHT_PAN(pan)	((WORD)(((WORD)(pan))      <<4))



	/********************************** Voice manipulation ************************************/

#pragma code_seg()


struct SetVoicesContext
	{
	ULONG			old_voices;
	CGF1Common *	self;
	};


/* set_voices() synchronized stuff
 */
NTSTATUS CGF1Common::set_voices_synchronized
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	SetVoicesContext *	ctx;
	CGF1Common *   		self;
	ULONG				vmin;
	ULONG				v;

	ctx = (SetVoicesContext *) context;
	self = ctx->self;

	if (self->gf1_voices < ctx->old_voices)
		{
		vmin = self->gf1_voices;
		}
	else
		{
		vmin = ctx->old_voices;
		}

	// Stop all affected voices
	for (v = 0; v < ctx->old_voices; v++)
		{
		// FIXME: use volume ramps?
		self->dwrite (GF1R_VSELECT, (BYTE) v);
		self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
		GF1_SELFMOD (self)
			self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
			self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
		GF1_SELFMOD_END
		}

	// Change number of active voices
	if (self->board_revision >= GF1REVISION_PNP && self->iw_mode == IW_ENHANCED_MODE)
		self->gf1_mixing_frequency = 44100;
	else
		self->gf1_mixing_frequency = gf1_voices2frequency[self->gf1_voices-14];
	self->iwrite8 (GF1REGV_VOICES, (BYTE) ((self->gf1_voices - 1) | 0xc0));

	// Stop all affected voices
	for (v = 0; v < self->gf1_voices; v++)
		{
		self->dwrite (GF1R_VSELECT, (BYTE) v);
		self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
		GF1_SELFMOD (self)
			self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
			self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
		GF1_SELFMOD_END
		}

	// Update voice structures and notify owners
	for (v = 0; v < vmin; v++)
		{
		// These are still valid
		if (v >= ctx->old_voices)
			{
			// New voice
			self->voice[v].alloc = GF1VOICE_FREE;
			self->voice[v].callback = NULL;
			self->voice[v].context = NULL;
			}
		else if (self->voice[v].callback)
			{
			// Was valid - notify
			self->voice[v].callback (v, GF1VREASON_FREQUENCY_CHANGED, self->voice,
				self->voice[v].context);
			}
		}
	for (/*v = vmin*/; v < ctx->old_voices; v++)
		{
		// These are no longer valid
		self->voice[v].alloc = GF1VOICE_INVALID;
		if (self->voice[v].callback)
			{
			self->voice[v].callback (v, GF1VREASON_STOLEN, self->voice, self->voice[v].context);
			}
		self->voice[v].callback = NULL;
		self->voice[v].context = NULL;
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Change playback frequency of current voice
 */
void CGF1Common::set_voice_frequency
	(
	IN ULONG	freq
	)

	{
	iwrite16 (GF1REGV_FREQ, (WORD) ((((freq) << GF1FREQ_FRAC_BITS) /
		gf1_mixing_frequency + 1) & ~1));
	}


#pragma code_seg()


/* Change number of active voices
 */
void CGF1Common::set_voices
	(
	IN ULONG	voices
	)

	{
	KIRQL	old_irql;

	// Acquire voice lock
	KeAcquireSpinLock (&voice_lock, &old_irql);

	// Change number of active vocies
	if (voices < 14)
		voices = 14;
	else if (voices > 32)
		voices = 32;

	if (gf1_voices != voices)
		{
		// Apply changes
		SetVoicesContext ctx;

		ctx.old_voices = gf1_voices;
		ctx.self = this;
		gf1_voices = voices;
		interrupt_sync->CallSynchronizedRoutine (&set_voices_synchronized, &ctx);
		}

	// Release voice lock
	KeReleaseSpinLock (&voice_lock, old_irql);
	}


#pragma code_seg ()


/* Select voice.
 */
void CGF1Common::select_voice
	(
	IN BYTE voice
	)
	{
#if KEEP_AUTO_INCREMENT_FEATURE_ON_INTERWAVE
	 BYTE reg;
	 reg = dread (GF1R_VSELECT);
	 reg &= 0x1f;					// Clear voice select field.
	 reg |= (voice & 0x1f);
	 dwrite (GF1R_VSELECT, reg);
#else
	 dwrite (GF1R_VSELECT, voice & 0x1f);
#endif
	}


#pragma code_seg ()


/* Set voice panning
 */
void CGF1Common::voice_set_panning
	(
	IN BYTE	pan
	)
	{
	if (board_revision >= GF1REVISION_PNP && iw_mode == IW_ENHANCED_MODE)
		{
		// InterWave and its separated stereo offsets.
		// FIXME: Use iw_voice_set_offsets_immediately() ?
		iw_voice_set_offsets_gradually (GET_IW_LEFT_PAN (pan), GET_IW_RIGHT_PAN (pan));
		}
	else
		{
		// GF1 (or InterWave in GUS compatible mode) and its 0-15 possibly values.
		iwrite8 (GF1REGV_PANNING, GET_GF1_PAN (pan));
		}
	}


	/* Lock voice info
	 */
GF1Voice * CGF1Common::lock_voices
	(
	OUT PKIRQL	old_irql
	)

	{
	KeAcquireSpinLock (&voice_lock, old_irql);
	return voice;
	}


	/* Unlock voice info
	 */
void CGF1Common::unlock_voices
	(
	IN KIRQL	old_irql
	)

	{
	KeReleaseSpinLock (&voice_lock, old_irql);
	}
