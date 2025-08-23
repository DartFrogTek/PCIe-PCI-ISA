/*
 * gf1waver.cpp
 *
 * GF1 wave recording stream miniport
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

/*
 Note: This stream uses one GF1 voice for notifications. It has the same problems as
 its wavout counterpart, so see notes there.

 Note: Does not work at all on PnP. 8bit only. Probably could do impressive frequencies.
 */


#include "gf1waver.h"


// Voices
#define VOICE_NOTIFY		3		// IRQ generating voice (must be != waveout stuff)


// Dma buffer length
#define BUFFER_LENGTH		32768


// Debugging info
#if GF1_DBG
#define DAILY_DEBUG(c,f) \
	{ \
	(c)->miniport->gf1_common->debug_to_registry (f); \
	}
#else /* GF1_DBG */
#define DAILY_DEBUG(c) {}
#endif /* GF1_DBG */


#pragma code_seg ("PAGE")


/* Create new GF1 wave recording stream object
 */
CGF1WaveRecordStream *create_gf1_wave_record_stream
	(
    IN POOL_TYPE	pool_type,
	IN PUNKNOWN		outer_unknown
	)

	{
    PAGED_CODE();

    ASSERT (unknown);

	CGF1WaveRecordStream *r = new (pool_type) CGF1WaveRecordStream (outer_unknown);
	if (r)
		{
		r->AddRef();
		}

	return r;
	}



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg ("PAGE")


CGF1WaveRecordStream::CGF1WaveRecordStream
	(
	PUNKNOWN	unknown
	) : CUnknown (unknown)

	{
	PAGED_CODE();

	miniport = NULL;
	fmt_stereo = FALSE;
	dma_16bit = FALSE;
	frequency = 44100;

	dma_channel = NULL;

	notify_length = 0;
	notifications = 0;

	service_group = NULL;
	state = KSSTATE_STOP;

	#if GF1_DBG
	#endif /* GF1_DBG */
	}


#pragma code_seg ("PAGE")


CGF1WaveRecordStream::~CGF1WaveRecordStream()
	{
	PAGED_CODE();

	SetState (KSSTATE_STOP);	// Probably not required

	if (dma_channel)
		{
		dma_channel->Release();
		if (miniport)
			miniport->gf1_common->put_dma (GF1DMACHANNEL_RECORD);
		}

	if (service_group)
		{
		service_group->Release();
		service_group = NULL;
		}

	if (miniport)
		{
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveInState", L"Destroyed");
		miniport->wavein_allocated = FALSE;
		miniport->Release();
		miniport = NULL;
		}
	}


#pragma code_seg ("PAGE")


/* Obtain an interface
 */
STDMETHODIMP CGF1WaveRecordStream::NonDelegatingQueryInterface
	(
	REFIID		iface,
    PVOID *		object
	)

	{
	PAGED_CODE();

	ASSERT (object);

	if (IsEqualGUIDAligned (iface, IID_IUnknown))
		{
		*object = (PVOID) ((PUNKNOWN) ((PMINIPORTWAVECYCLICSTREAM) this));
		}
	else if (IsEqualGUIDAligned (iface, IID_IMiniportWaveCyclicStream))
		{
		*object = (PVOID) ((PMINIPORTWAVECYCLICSTREAM) this);
		}
	else
		{
		*object = NULL;
		}

	if (*object)
		{
		((PUNKNOWN) (*object))->AddRef();
		return STATUS_SUCCESS;
		}

	return STATUS_INVALID_PARAMETER;
	}



/******************************** IMiniportWaveCyclicStream stuff *********************************/

#pragma code_seg ("PAGE")


/* Set recording format
 */
NTSTATUS CGF1WaveRecordStream::SetFormat
	(
	IN PKSDATAFORMAT	format
	)

	{
	PAGED_CODE();

	ASSERT (format);

	// Is format type valid?
	if (format->FormatSize < sizeof (KSDATAFORMAT_WAVEFORMATEX) ||
		!IsEqualGUIDAligned (format->MajorFormat, KSDATAFORMAT_TYPE_AUDIO) ||
		!IsEqualGUIDAligned (format->SubFormat, KSDATAFORMAT_SUBTYPE_PCM) ||
		!IsEqualGUIDAligned (format->Specifier, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX))
		{
		return STATUS_INVALID_PARAMETER;
		}

	// Is format valid?
	PWAVEFORMATEX	wf = (PWAVEFORMATEX) (format + 1);

	if (wf->wFormatTag != WAVE_FORMAT_PCM ||
		wf->wBitsPerSample != 8 ||
		(wf->nChannels != 1 && wf->nChannels != 2))
		{
		return STATUS_INVALID_PARAMETER;
		}

	// Check frequency
	ULONG fid;

	for (fid = 0; fid < GF1_RECFREQ_COUNT; fid++)
		{
		if (wf->nSamplesPerSec == gf1_recfreq[fid])
			break;
		}

	if (fid >= GF1_RECFREQ_COUNT)
		return STATUS_INVALID_PARAMETER;

	// OK - change format
	fmt_stereo = (wf->nChannels == 2 ? TRUE : FALSE);
	frequency = wf->nSamplesPerSec;

	/*
	 I hope 'SetNotificationFreq()' will be always called after 'SetFormat()'.
	 */

	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveInChannels", fmt_stereo ? 2 : 1);
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveInFrequency", frequency);
	DAILY_DEBUG (this, L"CGF1WaveRecordStream::SetFormat");

	return STATUS_SUCCESS;
	}


#pragma code_seg ("PAGE")


/* Set notification frequency
 */
ULONG CGF1WaveRecordStream::SetNotificationFreq
	(
	IN ULONG	interval,
	OUT PULONG	framing_size
	)

	{
	PAGED_CODE();

	notify_length = interval;	// Note: precision depends on number of active voices
	*framing_size = (frequency * interval / 1000) << (fmt_stereo ? 1 : 0);
	DAILY_DEBUG (this, L"CGF1WaveRecordStream::SetNotificationFreq");

	return interval;
	}


#pragma code_seg()


/* Get current playback position
 */
NTSTATUS CGF1WaveRecordStream::GetPosition
	(
	OUT PULONG		position
	)

	{
	ASSERT (position);

	*position = 0;
	if (state == KSSTATE_RUN && dma_channel)
		{
		ULONG tc;

		tc = dma_channel->TransferCount();
		if (tc)
			{
			*position = dma_channel->ReadCounter();
			if (tc > *position)
				*position = tc - *position;
			else
				*position = 0;
			}
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Convert position in bytes to time in 100ns units
 */
NTSTATUS CGF1WaveRecordStream::NormalizePhysicalPosition
	(
	IN OUT PLONGLONG	position
	)

	{
	*position = (_100NS_UNITS_PER_SECOND * (*position >> (fmt_stereo ? 1 : 0))) / frequency;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct SSStopContext
	{
	CGF1WaveRecordStream *	self;
	GF1Voice *				voice;
	};


/* Stop recording
 */
NTSTATUS CGF1WaveRecordStream::ss_stop
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	SSStopContext *			ctx;
	CGF1WaveRecordStream *	self;
	IGF1Common *			common;

	ctx = (SSStopContext *) context;
	self = ctx->self;
	common = self->miniport->gf1_common;

	// Stop notification voice
	common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	for (ULONG t = 0; t < 4; t++)
		{
		// Clear interrupts
		if (common->iread8 (GF1REGV_VOICE_CTRL) & GF1VC_IRQ_PENDING)
			{
			common->dread (GF1R_IRQ_STATUS);
			common->iread8 (GF1REGV_VIRQ);
			}
		}

	// Stop it
	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
	common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
	GF1_SELFMOD_END
	common->iwrite16 (GF1REGV_VOLUME, 0);
	GF1VOICE_START (common, 0);
	GF1VOICE_END (common, 0);
	GF1VOICE_POS (common, 0);

	// Stop recording
	common->iwrite8 (GF1REG_SMP_CTRL, 0);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REG_SMP_CTRL, 0);
	GF1_SELFMOD_END

	// Deallocate notification voice
	if (ctx->voice[VOICE_NOTIFY].alloc == GF1VOICE_WAVEIN)
		{
		ctx->voice[VOICE_NOTIFY].alloc = GF1VOICE_FREE;
		ctx->voice[VOICE_NOTIFY].callback = NULL;
		ctx->voice[VOICE_NOTIFY].context = NULL;
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct SSRunContext
	{
	CGF1WaveRecordStream *	self;
	GF1Voice *				voice;
	BOOLEAN					result;
	};


/* Resume recording
 */
NTSTATUS CGF1WaveRecordStream::ss_run
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	SSRunContext *			ctx;
	CGF1WaveRecordStream *	self;
	IGF1Common *			common;

	ctx = (SSRunContext *) context;
	self = ctx->self;
	common = self->miniport->gf1_common;

	self->notifications = 0;

	// Allocate notification voice
	if (ctx->voice[VOICE_NOTIFY].alloc != GF1VOICE_FREE &&
		ctx->voice[VOICE_NOTIFY].alloc != GF1VOICE_SYNTH)
		{
		// Can't steal
		ctx->result = FALSE;
		return STATUS_SUCCESS; // no-one cares about results
		}

	// Steal notification voice
	ctx->voice[VOICE_NOTIFY].alloc = GF1VOICE_WAVEIN;
	if (ctx->voice[VOICE_NOTIFY].callback)
		{
		ctx->voice[VOICE_NOTIFY].callback (VOICE_NOTIFY, GF1VREASON_STOLEN,
			ctx->voice, ctx->voice[VOICE_NOTIFY].context);
		}
	ctx->voice[VOICE_NOTIFY].callback = notify_callback;
	ctx->voice[VOICE_NOTIFY].context = self;

	// Start notification voice
	common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
	common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD_END

	common->iwrite16 (GF1REGV_VOLUME, 0);
	common->iwrite8 (GF1REGV_PANNING, 7);
	common->iwrite16 (GF1REGV_FREQ, 1 << GF1FREQ_FRAC_BITS);
	GF1VOICE_START (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));
	GF1VOICE_END (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE +
		(self->notify_length * common->get_frequency() / 1000 << (self->fmt_stereo ? 1 : 0))));
	GF1VOICE_POS (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));

	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED);
	GF1_DELAY (1);
	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD_END

	// Set rate
	common->iwrite8 (GF1REG_SMP_FREQ, (BYTE) GF1SMP_RATE (self->frequency));

	// Start sampling
	common->iwrite8 (GF1REG_SMP_CTRL, GF1SMP_START | (self->fmt_stereo ? GF1SMP_STEREO : 0) |
		(self->dma_16bit ? GF1SMP_DMA16 : 0) | GF1SMP_UNSIGNED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REG_SMP_CTRL, GF1SMP_START | (self->fmt_stereo ? GF1SMP_STEREO : 0) |
			(dma_16bit ? GF1SMP_DMA16 : 0) | GF1SMP_UNSIGNED);
	GF1_SELFMOD_END

	ctx->result = TRUE;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Change channel state
 */
NTSTATUS CGF1WaveRecordStream::SetState
	(
	IN KSSTATE		new_state
	)

	{
	if (new_state != KSSTATE_RUN)
		new_state = KSSTATE_STOP;

	if (state == new_state)
		return STATUS_SUCCESS;

	if (new_state == KSSTATE_STOP)
		{
		// Stop recording
		SSStopContext	ctx;
		KIRQL			old_irql;

		dma_channel->Stop();

		ctx.self = this;
		ctx.voice = miniport->gf1_common->lock_voices (&old_irql);
		miniport->gf1_common->call_synchronized (&ss_stop, &ctx);
		miniport->gf1_common->unlock_voices (old_irql);

		miniport->gf1_common->set_wavetable_handler (VOICE_NOTIFY, NULL, NULL, NULL);
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveInState", L"Stopped");
		}
	else if (new_state == KSSTATE_RUN)
		{
		// Resume recording
		ULONG			i;
		SSRunContext	rctx;
		SSStopContext	sctx;

		rctx.self = this;
		sctx.self = this;

		// Well, again, my GUS classic is lazy wavetable IRQ generator
		for (i = 0; i < 4; i++)
			{
			ULONGLONG		t;
			LARGE_INTEGER	d;
			KIRQL			old_irql;

			// Start stream
			miniport->gf1_common->set_wavetable_handler (VOICE_NOTIFY,
				&wave_ack_handler, &wave_action_handler, this);

			rctx.voice = miniport->gf1_common->lock_voices (&old_irql);
			miniport->gf1_common->call_synchronized (&ss_run, &rctx);
			miniport->gf1_common->unlock_voices (old_irql);

			dma_channel->Start (dma_channel->BufferSize(), FALSE);

			if (!rctx.result)
				{
				GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveInState", L"Can't allocate voices");
				return STATUS_INSUFFICIENT_RESOURCES;
				}

			// Wait a bit
			t = PcGetTimeInterval (0);
			d.QuadPart = -5 * 10000;	// 5ms intervals
			while (PcGetTimeInterval (t) < GTI_MILLISECONDS (2 * notify_length))
				{
				if (notifications)
					break;
				KeDelayExecutionThread (KernelMode, FALSE, &d);
				}

			// Check if IRQ has been generated
			if (notifications)
				break;

			// Well, we failed, so solve it somehow (reset something?) and try again
			dma_channel->Stop();

			sctx.voice = miniport->gf1_common->lock_voices (&old_irql);
			miniport->gf1_common->call_synchronized (&ss_stop, &sctx);
			miniport->gf1_common->unlock_voices (old_irql);

			miniport->gf1_common->set_wavetable_handler (VOICE_NOTIFY, NULL, NULL, NULL);
			t = PcGetTimeInterval (0);
			d.QuadPart = -50 * 10000;	// 50ms intervals
			while (PcGetTimeInterval (t) < GTI_MILLISECONDS (250))
				KeDelayExecutionThread (KernelMode, FALSE, &d);

			sctx.voice = miniport->gf1_common->lock_voices (&old_irql);
			miniport->gf1_common->call_synchronized (&ss_stop, &sctx);
			miniport->gf1_common->unlock_voices (old_irql);
			}

		if (i == 4)
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveInState", L"Notification jammed");
			}
		else
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveInState", L"Recording");
			}
		}

	state = new_state;

	DAILY_DEBUG (this, state == KSSTATE_STOP ? L"CGF1WaveRecordStream::SetState(stop)" :
		L"CGF1WaveRecordStream::SetState(run)");

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Clear buffer
 */
void CGF1WaveRecordStream::Silence
	(
	IN PVOID	dest,
	IN ULONG	length
	)

	{
	RtlFillMemory (dest, length, 0x80);
	}



/***************************************** Our own stuff ******************************************/

#pragma code_seg()


/* Wavetable acknowledge callback
 */
void CGF1WaveRecordStream::wave_ack_handler
	(
	IN ULONG,
	IN PVOID	context
	)

	{
	CGF1WaveRecordStream *	self;

	self = (CGF1WaveRecordStream *) context;

	// Disable interrupts for a while
	self->miniport->gf1_common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	self->miniport->gf1_common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED);
	GF1_SELFMOD(self->miniport->gf1_common)
		self->miniport->gf1_common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED);
	GF1_SELFMOD_END
	}


#pragma code_seg()


/* Wavetable action callback
 */
void CGF1WaveRecordStream::wave_action_handler
	(
	IN ULONG,
	IN PVOID	context
	)

	{
	CGF1WaveRecordStream *	self;

	self = (CGF1WaveRecordStream *) context;

	// Notify
	self->miniport->port->Notify (self->service_group);
	self->notifications++;

	// Enable interrupts
	self->miniport->gf1_common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	self->miniport->gf1_common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD(self->miniport->gf1_common)
		self->miniport->gf1_common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD_END
	}



#pragma code_seg()


/* Notification voice callback
 */
void CGF1WaveRecordStream::notify_callback
	(
	IN ULONG,
	IN GF1VoiceReason	reason,
	IN GF1Voice *,
	IN PVOID			context
	)

	{
	CGF1WaveRecordStream *	self;
	IGF1Common *			common;

	self = (CGF1WaveRecordStream *) context;
	common = self->miniport->gf1_common;

	switch (reason)
		{
	case GF1VREASON_FREQUENCY_CHANGED:
		// Restart notification voice
		common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
			common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
		GF1_SELFMOD_END

		common->iwrite16 (GF1REGV_VOLUME, 0);
		common->iwrite8 (GF1REGV_PANNING, 7);
		common->iwrite16 (GF1REGV_FREQ, 1 << GF1FREQ_FRAC_BITS);
		GF1VOICE_START (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));
		GF1VOICE_END (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE +
			(self->notify_length * common->get_frequency() / 1000 << (self->fmt_stereo ? 1 : 0))));
		GF1VOICE_POS (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));

		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED);
		GF1_DELAY (1);
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
		GF1_SELFMOD_END

		// Notify
		self->miniport->port->Notify (self->service_group);
		break;

	default:
		// Can't be stolen
		break;
		}
	}


#pragma code_seg ("PAGE")


/* Initialize stream
 */
NTSTATUS CGF1WaveRecordStream::init
	(
	IN CGF1Wave *	 		_miniport,
	IN PRESOURCELIST		dma_resources,
	OUT PSERVICEGROUP *		_service_group,
	OUT PDMACHANNEL *		_dma_channel
	)

	{
	PAGED_CODE();

	NTSTATUS	status;

	ASSERT (_miniport);
	ASSERT (dma_resources);
	ASSERT (_service_group);
	ASSERT (_dma_channel);

	// Store miniport
	miniport = _miniport;
	miniport->AddRef();

	// Get DMA channel
	if (!miniport->gf1_common->get_dma (GF1DMACHANNEL_RECORD))
		{
		status = STATUS_INSUFFICIENT_RESOURCES;
		}
	else
		{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR		rd;

		// Allocate DMA adapter
		rd = dma_resources->FindTranslatedDma (1);
		dma_channel = NULL;
		status = miniport->port->NewSlaveDmaChannel (&dma_channel, NULL,
			dma_resources, 1, BUFFER_LENGTH, TRUE,
			(rd->Flags & CM_RESOURCE_DMA_TYPE_A) ? TypeA :
			(rd->Flags & CM_RESOURCE_DMA_TYPE_B) ? TypeB : Compatible);

		if (NT_SUCCESS (status))
			{
			// Allocate DMA buffer
			ULONG	dma_length	= 2 * BUFFER_LENGTH;

			do	{
				dma_length >>= 1;
				status = dma_channel->AllocateBuffer (dma_length, NULL);
				} while (!NT_SUCCESS (status) && dma_length > PAGE_SIZE / 2);
			GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveInBufferSize", dma_length);
			}

		dma_16bit = (rd->Flags & CM_RESOURCE_DMA_8) ? FALSE : TRUE;

		if (!NT_SUCCESS (status))
			{
			// DMA channel allocation failed
			if (dma_channel)
				dma_channel->Release();
			dma_channel = NULL;
			miniport->gf1_common->put_dma (GF1DMACHANNEL_RECORD);
			}
		}

	// Create service group
	if (NT_SUCCESS (status))
		status = PcNewServiceGroup (&service_group, NULL);

	if (NT_SUCCESS (status))
		{
		// Return non-addrefed pointers
		*_service_group = service_group;
		*_dma_channel = dma_channel;
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveInState", L"Initialized");
		}

	return status;

	// SetFormat() should be called ASAP
	}
