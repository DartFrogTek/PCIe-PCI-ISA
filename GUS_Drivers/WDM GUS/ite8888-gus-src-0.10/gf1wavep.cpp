/*
 * gf1wavep.cpp
 *
 * GF1 wave playback stream miniport
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
 Note: This stream "emulates" cyclic dma in GUS DRAM. Writes to fake DMA buffer (CopyTo)
 lead to DMA transfers. Special channel is used to generate notification interrupts.

 Note: This code is does not support PnP in enhanced mode, but works in compatible.

 FIXME: I hate those special channels. But AdLib timers don't work well. Using rollover
 feature and generating interrupts by voice 0 is possible, but quite hard to do. There
 are big problems around sample loop (either we are risking that voice will pass beyond
 loop end, or notifications must be delayed a bit near loop end).
 I don't really understand why those notifications must be so precise.

 FIXME: Windows never uses 8bit or mono formats!

 FIXME: My GUS is probably too old and buggy, because it sometimes fires volume ramp IRQs
 (I never enabled them), it sometimes does not want to generate wavetable IRQs, etc.
 Or I'm completely stupid. Note that once the stream runs, everything is OK.
 Everything works very well on Interwave, but it's not needed there :-(

 FIXME: Quick starts/stops - do one notify and wait for copyto before starting voices?
 */


#include "gf1wavep.h"


// Min. number of bytes to copy from fake DMA buffer to DRAM
#define MINIMUM_TO_SEND		256


// Voice volume (FIXME?)
#define MONO_VOLUME			0xf800
#define STEREO_VOLUME		0xea00
#define ZERO_RAMP_VOLUME	0x08


// Voices
#define VOICE_LEFT			0		// Left or mono channel
#define VOICE_RIGHT			1		// Right channel
#define VOICE_NOTIFY		2		// IRQ generating voice


// Debugging info
#if GF1_DBG
#define DAILY_DEBUG(c,f) \
	{ \
	(c)->miniport->gf1_common->debug_to_registry (f); \
	GF1_DBGINT_ ((c)->miniport->gf1_common, L"GF1WaveOutCopyTotal", (c)->copyto_total); \
	GF1_DBGINT_ ((c)->miniport->gf1_common, L"GF1WaveOutCopyWeirdos", (c)->copyto_errors); \
	GF1_DBGINT_ ((c)->miniport->gf1_common, L"GF1WaveOutCopyTooHighLevel", (c)->copyto_hilevel); \
	GF1_DBGINT_ ((c)->miniport->gf1_common, L"GF1WaveOutBufferSize", (c)->buffer_cur_length); \
	GF1_DBGINT_ ((c)->miniport->gf1_common, L"GF1WaveOutWeirdIrqs", (c)->notify_ramp_irqs); \
	}
#else /* GF1_DBG */
#define DAILY_DEBUG(c) {}
#endif /* GF1_DBG */


#pragma code_seg ("PAGE")


/* Create new GF1 wave playback stream object
 */
CGF1WavePlaybackStream *create_gf1_wave_playback_stream
	(
    IN POOL_TYPE	pool_type,
	IN PUNKNOWN		outer_unknown
	)

	{
    PAGED_CODE();

    ASSERT (unknown);

	CGF1WavePlaybackStream *p = new (pool_type) CGF1WavePlaybackStream (outer_unknown);
	if (p)
		{
		p->AddRef();
		}

	return p;
	}



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg ("PAGE")


CGF1WavePlaybackStream::CGF1WavePlaybackStream
	(
	PUNKNOWN	unknown
	) : CUnknown (unknown)

	{
	PAGED_CODE();

	miniport = NULL;
	fmt_16bit = FALSE;
	fmt_stereo = FALSE;
	frequency = 44100;

	buffer = NULL;
	buffer_aligned = NULL;
	buffer_max_length = 0;
	buffer_cur_length = 0;

	transfer_offset = 0;
	copyto_offset = 0;

	transfers_pending = 0;
	//transfer_event

	notify_length = 0;
	notifications = 0;

	service_group = NULL;
	state = KSSTATE_STOP;

	#if GF1_DBG
	notify_ramp_irqs = 0;
	copyto_total = 0;
	copyto_errors = 0;
	copyto_hilevel = 0;
	#endif /* GF1_DBG */
	}


#pragma code_seg ("PAGE")


CGF1WavePlaybackStream::~CGF1WavePlaybackStream()
	{
	PAGED_CODE();

	if (buffer)
		{
		FreeBuffer();
		}

	if (service_group)
		{
		service_group->Release();
		service_group = NULL;
		}

	if (miniport)
		{
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Destroyed");
		miniport->gf1_common->put_dma (GF1DMACHANNEL_DRAM);
		miniport->waveout_allocated = FALSE;
		miniport->Release();
		miniport = NULL;
		}
	}


#pragma code_seg ("PAGE")


/* Obtain an interface
 */
STDMETHODIMP CGF1WavePlaybackStream::NonDelegatingQueryInterface
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
	else if (IsEqualGUIDAligned (iface, IID_IDmaChannel))
		{
		*object = (PVOID) ((PDMACHANNEL) this);
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



/*************************************** IDmaChannel stuff ****************************************/

#pragma code_seg ("PAGE")


/* Allocate fake DMA cyclic buffer
 */
NTSTATUS CGF1WavePlaybackStream::AllocateBuffer
	(
	IN ULONG				buffer_size,
	IN PPHYSICAL_ADDRESS	constraint		OPTIONAL
	)

	{
	PAGED_CODE();

	ULONG align;

	ASSERT (buffer_size <= GF1MEM_RESERVED_DMA - 4);
	ASSERT (!buffer);

	// Get dma alignment
	align = miniport->gf1_common->ddma_get_alignment();

	// Allocate buffer
	buffer_size &= ~GF1MEM_ALIGNMENT;
	buffer_size &= ~align;
	buffer = ExAllocatePool (NonPagedPool, buffer_size + align);
	if (!buffer)
		{
		return STATUS_INSUFFICIENT_RESOURCES;
		}
	buffer_aligned = (PBYTE) (((ULONG_PTR) ((PBYTE) buffer + align)) & ~align);

	buffer_max_length = buffer_size;
	buffer_cur_length = buffer_size;
	transfer_offset = 0;
	copyto_offset = 0;

	return STATUS_SUCCESS;
	}


#pragma code_seg ("PAGE")


/* Free fake DMA cyclic buffer
 */
void CGF1WavePlaybackStream::FreeBuffer
	(
	void
	)

	{
	PAGED_CODE();

	while (transfers_pending > 0)
		{
		// Wait for tranfer event
		LARGE_INTEGER	timeout;
		NTSTATUS		status;

		timeout.QuadPart = -10000000;	// 1 sec
		status = KeWaitForSingleObject (&transfer_event, Executive, KernelMode, FALSE, &timeout);
		if (status == STATUS_TIMEOUT)
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutWarning", L"Timeout in FreeBuffer");
			GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutTransfersPending", transfers_pending);
			return;
			}

		/* Note:
		 Sync event, so it should be non-signaled now.
		 No locks are required in this case.
		 */
		}

	ASSERT (buffer);
	ExFreePool (buffer);
	buffer = NULL;
	buffer_aligned = NULL;
	buffer_max_length = 0;
	buffer_cur_length = 0;
	}


#pragma code_seg()


/* Not used
 */
ULONG CGF1WavePlaybackStream::TransferCount
	(
	void
	)

	{
	return buffer_cur_length;
	}


#pragma code_seg()


/* Not used
 */
ULONG CGF1WavePlaybackStream::MaximumBufferSize
	(
	void
	)

	{
	return buffer_max_length;
	}


#pragma code_seg()


/* Get size of allocated buffer
 */
ULONG CGF1WavePlaybackStream::AllocatedBufferSize
	(
	void
	)

	{
	return buffer_max_length;
	}


#pragma code_seg()


/* Get current size of buffer
 */
ULONG CGF1WavePlaybackStream::BufferSize
	(
	void
	)

	{
	return buffer_cur_length;
	}


#pragma code_seg()


/* Set current size of buffer
 */
void CGF1WavePlaybackStream::SetBufferSize
	(
	IN ULONG	size
	)

	{
	// Let's hope stream is not playing right now
	ASSERT (size <= buffer_max_length);

	size &= ~GF1MEM_ALIGNMENT;
	size &= ~miniport->gf1_common->ddma_get_alignment();
	buffer_cur_length = size;
	}


#pragma code_seg()


/* Get virtual system address of allocated buffer
 */
PVOID CGF1WavePlaybackStream::SystemAddress
	(
	void
	)

	{
	return buffer_aligned;
	}


#pragma code_seg()


/* Not used
 */
PHYSICAL_ADDRESS CGF1WavePlaybackStream::PhysicalAddress
	(
	void
	)

	{
	PHYSICAL_ADDRESS r;
	r.QuadPart = 0;
	return r;
	}


#pragma code_seg()


/* Not used
 */
PADAPTER_OBJECT CGF1WavePlaybackStream::GetAdapterObject
	(
	void
	)

	{
	return NULL;
	}


#pragma code_seg()


/* Copy data from system memory to DMA buffer
 */
void CGF1WavePlaybackStream::CopyTo
	(
	IN PVOID	destination,
	IN PVOID	source,
	IN ULONG	length
	)

	{
	if (!buffer_aligned || destination < buffer_aligned ||
		(PBYTE) destination + length > buffer_aligned + buffer_cur_length)
		{
		// !Buffer does not exist
		#if GF1_DBG
		copyto_errors++;
		#endif /* GF1_DBG */
		return;
		}

	// Copy it
	if (source)
		RtlCopyMemory (destination, source, length);
	else
		RtlFillMemory (destination, length, fmt_16bit ? 0 : 0x80);

	#if GF1_DBG

	copyto_total += length;
	if (destination != buffer_aligned + copyto_offset)
		{
		// Unexpected copyto location
		copyto_errors++;
		if (KeGetCurrentIrql() == PASSIVE_LEVEL)
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutWarning", L"Unexpected CopyTo destination");
			DAILY_DEBUG (this, L"CGF1WavePlaybackStream::CopyTo");
			}
		}

	#endif /* GF1_DBG */

	// Write samples from loop start to loop end
	if (destination == buffer_aligned)
		{
		// Note: buffer must be shorter than reserved length (at least by 4 bytes)
		BYTE xor = fmt_16bit ? 0x00 : 0x80;

		for (ULONG i = 0; i < 4; i++)
			{
			miniport->gf1_common->poke (GF1MEM_RESERVED_SILENCE + buffer_cur_length + i,
				buffer_aligned[i] ^ xor);
			}
		}

	// Update pointers
	copyto_offset = ((PBYTE) destination) + length - buffer_aligned;
	if (copyto_offset >= buffer_cur_length)
		copyto_offset -= buffer_cur_length;

	// This can run at any IRQL, dma transfers require <= DISPATCH_LEVEL
	if (KeGetCurrentIrql() > DISPATCH_LEVEL)
		{
		#if GF1_DBG
		copyto_hilevel++;
		#endif /* GF1_DBG */
		return;
		}

	// Send it
	if (copyto_offset <= transfer_offset)
		{
		// Transfer till end of buffer
		/* Note:
		 ('copyto_offset' == 'transfer_offset') condition could mean that whole buffer
		 has been overwritten with something.
		 */
		if (transfer_offset < buffer_cur_length)
			{
			if (miniport->gf1_common->ddma_download_nonpaged (GF1MEM_RESERVED_SILENCE + transfer_offset,
				buffer_aligned + transfer_offset, buffer_cur_length - transfer_offset,
				fmt_16bit ? GF1DMA_DATA16 : GF1DMA_UNSIGNED, &transfers_pending, &transfer_event))
				{
				InterlockedIncrement (&transfers_pending);
				}
			}
		transfer_offset = 0;
		}

	{
	// Regular send
	ULONG length;
	ULONG align;

	// Align 'copyto_offset' (end of buffer to send)
	align = miniport->gf1_common->ddma_get_alignment();
	length = copyto_offset;
	length &= ~GF1MEM_ALIGNMENT;
	length &= ~align;

	// Compute maximum number of bytes we can transfer (should be >= 0)
	if (length > transfer_offset)
		length -= transfer_offset;
	else
		length = 0;

	// Is it worth sending?
	if (length > GF1MEM_ALIGNMENT && length > align && length >= MINIMUM_TO_SEND)
		{
		if (miniport->gf1_common->ddma_download_nonpaged (GF1MEM_RESERVED_SILENCE + transfer_offset,
			buffer_aligned + transfer_offset, length, fmt_16bit ? GF1DMA_DATA16 : GF1DMA_UNSIGNED,
			&transfers_pending, &transfer_event))
			{
			InterlockedIncrement (&transfers_pending);
			}
		transfer_offset += length;
		/* FIXME:
		 Should not be required, because 'copyto_offset' < 'buffer_cur_length' or
		 'copyto_offset' == 'transfer_offset' == 0.
		 */
		if (transfer_offset >= buffer_cur_length)
			transfer_offset -= buffer_cur_length;
		}
	}//eob

	}


#pragma code_seg()


/* Copy data from DMA buffer to system memory
 */
void CGF1WavePlaybackStream::CopyFrom
	(
	IN PVOID	destination,
	IN PVOID	source,
	IN ULONG	length
	)

	{
	// Copy memory, but I don't really know if port _reads_ playback DMA buffer
	RtlCopyMemory (destination, source, length);
	}



/******************************** IMiniportWaveCyclicStream stuff *********************************/


#pragma code_seg ("PAGE")


/* Set playback format
 FIXME: Windows obviously always sets this to 16bit stereo
 */
NTSTATUS CGF1WavePlaybackStream::SetFormat
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
		(wf->wBitsPerSample != 8 && wf->wBitsPerSample != 16) ||
		(wf->nChannels != 1 && wf->nChannels != 2))
		{
		return STATUS_INVALID_PARAMETER;
		}

	// Check frequency
	ULONG voices;

	for (voices = 14; voices < 33; voices++)
		{
		if (wf->nSamplesPerSec == gf1_voices2frequency[voices-14])
			break;
		}

	if (voices > 32)
		return STATUS_INVALID_PARAMETER;

	// OK - change format
	fmt_16bit = (wf->wBitsPerSample == 16 ? TRUE : FALSE);
	fmt_stereo = (wf->nChannels == 2 ? TRUE : FALSE);
	frequency = wf->nSamplesPerSec;

	// Change number of active voices
	miniport->gf1_common->set_voices (voices);

	/* FIXME:
	 I hope 'SetNotificationFreq()' will be always called after 'SetFormat()'.
	 */

	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutChannels", fmt_stereo ? 2 : 1);
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutBitsPerSample", fmt_16bit ? 16 : 1);
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutFrequency", frequency);
	DAILY_DEBUG (this, L"CGF1WavePlaybackStream::SetFormat");

	return STATUS_SUCCESS;
	}


#pragma code_seg ("PAGE")


/* Set notification frequency
 */
ULONG CGF1WavePlaybackStream::SetNotificationFreq
	(
	IN ULONG	interval,
	OUT PULONG	framing_size
	)

	{
	PAGED_CODE();

	// Try faster timer
	/*ticks = GF1TIMER1_FREQ * interval / 1000;
	if (ticks > 0x100)
		{
		// Requested interval is too low - try slower timer
		ticks = GF1TIMER2_FREQ * interval / 1000;
		if (ticks > 0x100)
			{
			// Still not enough - our notifications will be faster than requested
			ticks = 0x100;
			}
		notify_timer = 2;
		}
	else
		{
		notify_timer = 1;
		}
	if (ticks == 0)
		ticks = 1;	// Should not happen unless interval is 0
	timer_count = 0x100 - ticks;

	// Compute number of bytes per interval
	*framing_size = frequency * ticks / (notify_timer == 1 ? GF1TIMER1_FREQ : GF1TIMER2_FREQ) <<
		(fmt_stereo ? fmt_16bit ? 2 : 1 : fmt_16bit ? 1 : 0);

	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutNotifyFraming", *framing_size);//
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutNotifyIntervalIn", interval);//
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutNotifyTimer", notify_timer);//
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutNotifyTimerCount", timer_count);//
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutNotifyInterval",
		1000 * ticks / (notify_timer == 1 ? GF1TIMER1_FREQ : GF1TIMER2_FREQ));
	DAILY_DEBUG (this);

	return 1000 * ticks / (notify_timer == 1 ? GF1TIMER1_FREQ : GF1TIMER2_FREQ);*/

	notify_length = frequency * interval / 1000;
	*framing_size = notify_length << (fmt_stereo ? fmt_16bit ? 2 : 1 : fmt_16bit ? 1 : 0);
	GF1_DBGINT_ (miniport->gf1_common, L"GF1WaveOutNotifyLength", notify_length);
	DAILY_DEBUG (this, L"CGF1WavePlaybackStream::SetNotificationFreq");

	return interval;
	}


#pragma code_seg()


struct GPContext
	{
	IGF1Common *	common;
	ULONG			position;
	};


static NTSTATUS gp_get_position
	(
	PINTERRUPTSYNC,
	PVOID				context
	)

	{
	GPContext *		ctx;
	WORD			lo1, hi1;
	WORD			lo2, hi2;

	ctx = (GPContext *) context;
	ctx->common->dwrite (GF1R_VSELECT, VOICE_LEFT);
	lo1 = ctx->common->iread16 (GF1REGV_POSL);
	hi1 = ctx->common->iread16 (GF1REGV_POSH);
	lo2 = ctx->common->iread16 (GF1REGV_POSL);
	hi2 = ctx->common->iread16 (GF1REGV_POSH);

	// FIXME: this still is not 100% because these regs are self-modifying
	if (lo2 < lo1)
		{
		// Overflowed - we don't know if 'hi1' is the old one or new one
		ctx->position = ((lo2 >> 9) & 0x7f) | ((hi2 & 0x1fff) << 7);
		}
	else
		{
		ctx->position = ((lo1 >> 9) & 0x7f) | ((hi1 & 0x1fff) << 7);
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Get current playback position
 */
NTSTATUS CGF1WavePlaybackStream::GetPosition
	(
	OUT PULONG		position
	)

	{
	ASSERT (position);

	if (!buffer || state != KSSTATE_RUN)
		{
		*position = 0;
		}
	else
		{
		GPContext ctx;

		ctx.common = miniport->gf1_common;
		miniport->gf1_common->call_synchronized (&gp_get_position, &ctx);
		if (fmt_16bit)
			ctx.position = (ctx.position & 0xc0000) | ((ctx.position & 0x1ffff) << 1);
		if (ctx.position >= GF1MEM_RESERVED_SILENCE)
			{
			ctx.position -= GF1MEM_RESERVED_SILENCE;
			if (ctx.position >= buffer_cur_length)
				*position = buffer_cur_length - 4;
			else
				*position = ctx.position;
			}
		else
			{
			*position = 0;
			}
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Convert position in bytes to time in 100ns units
 */
NTSTATUS CGF1WavePlaybackStream::NormalizePhysicalPosition
	(
	IN OUT PLONGLONG	position
	)

	{
	*position = (_100NS_UNITS_PER_SECOND *
		(*position >> (fmt_stereo ? fmt_16bit ? 2 : 1 : fmt_16bit ? 1 : 0))) / frequency;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct SSStopContext
	{
	CGF1WavePlaybackStream *	self;
	GF1Voice *					voice;
	};


/* Stop playback
 */
NTSTATUS CGF1WavePlaybackStream::ss_stop
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	SSStopContext *				ctx;
	CGF1WavePlaybackStream *	self;
	IGF1Common *				common;

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

	common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
	GF1VOICE_START (common, 0);
	GF1VOICE_END (common, 0);
	GF1VOICE_POS (common, 0);

	self->transfer_offset = 0;
	self->copyto_offset = 0;

	// Start ramp down
	common->dwrite (GF1R_VSELECT, VOICE_LEFT);
	/*if (self->fmt_stereo)
		{
		common->iwrite16 (GF1REGV_VOLUME, STEREO_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_START, ZERO_RAMP_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_END, (BYTE) (STEREO_VOLUME >> 8));
		common->iwrite8 (GF1REGV_RAMP_RATE, GF1RR_INCREMENT_MASK | GF1RR_PERIOD_1);

		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_DOWN);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_DOWN);
		GF1_SELFMOD_END

		common->dwrite (GF1R_VSELECT, 1);
		common->iwrite16 (GF1REGV_VOLUME, STEREO_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_START, ZERO_RAMP_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_END, (BYTE) (STEREO_VOLUME >> 8));
		common->iwrite8 (GF1REGV_RAMP_RATE, GF1RR_INCREMENT_MASK | GF1RR_PERIOD_1);

		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_DOWN);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_DOWN);
		GF1_SELFMOD_END

		common->dwrite (GF1R_VSELECT, 0);
		}
	else
		{
		common->iwrite16 (GF1REGV_VOLUME, MONO_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_START, ZERO_RAMP_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_END, (BYTE) (MONO_VOLUME >> 8));
		common->iwrite8 (GF1REGV_RAMP_RATE, GF1RR_INCREMENT_MASK | GF1RR_PERIOD_1);

		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_DOWN);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_DOWN);
		GF1_SELFMOD_END
		}*/

	// Stop voice 0
	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
	common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD_END

	common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
	GF1VOICE_START (common, 0);
	GF1VOICE_END (common, 0);
	GF1VOICE_POS (common, 0);

	// Stop voice 1 if stereo
	if (self->fmt_stereo)
		{
		common->dwrite (GF1R_VSELECT, VOICE_RIGHT);

		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
			common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
		GF1_SELFMOD_END

		common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
		GF1VOICE_START (common, 0);
		GF1VOICE_END (common, 0);
		GF1VOICE_POS (common, 0);
		}

	// Deallocate voices
	if (ctx->voice[VOICE_LEFT].alloc == GF1VOICE_WAVEOUT)
		ctx->voice[VOICE_LEFT].alloc = GF1VOICE_FREE;
	if (ctx->voice[VOICE_RIGHT].alloc == GF1VOICE_WAVEOUT)
		ctx->voice[VOICE_RIGHT].alloc = GF1VOICE_FREE;
	if (ctx->voice[VOICE_NOTIFY].alloc == GF1VOICE_WAVEOUT)
		ctx->voice[VOICE_NOTIFY].alloc = GF1VOICE_FREE;

	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct SSRunContext
	{
	CGF1WavePlaybackStream *	self;
	GF1Voice *					voice;
	BOOLEAN						result;
	};


/* Resume playback
 */
NTSTATUS CGF1WavePlaybackStream::ss_run
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	SSRunContext *				ctx;
	CGF1WavePlaybackStream *	self;
	IGF1Common *				common;

	ctx = (SSRunContext *) context;
	self = ctx->self;
	common = self->miniport->gf1_common;

	self->transfer_offset = 0;
	self->copyto_offset = 0;
	self->notifications = 0;

	/* Allocate voices
	 Note: we don't install notification handlers, because
	 - no-one else calls 'set_voices()'
	 - these voices can't become invalid
	 FIXME: If there are another notification reasons, those should be probably handled
	 */
	if ((ctx->voice[VOICE_LEFT].alloc != GF1VOICE_FREE &&
		 ctx->voice[VOICE_LEFT].alloc != GF1VOICE_SYNTH) ||
		(ctx->voice[VOICE_RIGHT].alloc != GF1VOICE_FREE &&
		 ctx->voice[VOICE_RIGHT].alloc != GF1VOICE_SYNTH) ||
		(ctx->voice[VOICE_NOTIFY].alloc != GF1VOICE_FREE &&
		 ctx->voice[VOICE_NOTIFY].alloc != GF1VOICE_SYNTH))
		{
		// Can't steal
		ctx->result = FALSE;
		return STATUS_SUCCESS; // no-one cares about results
		}

	// Steal left/mono
	ctx->voice[VOICE_LEFT].alloc = GF1VOICE_WAVEOUT;
	if (ctx->voice[VOICE_LEFT].callback)
		{
		ctx->voice[VOICE_LEFT].callback (VOICE_LEFT, GF1VREASON_STOLEN,
			ctx->voice, ctx->voice[VOICE_LEFT].context);
		}
	ctx->voice[VOICE_LEFT].callback = NULL;
	ctx->voice[VOICE_LEFT].context = NULL;

	// Steal right
	if (self->fmt_stereo)
		{
		ctx->voice[VOICE_RIGHT].alloc = GF1VOICE_WAVEOUT;
		if (ctx->voice[VOICE_RIGHT].callback)
			{
			ctx->voice[VOICE_RIGHT].callback (VOICE_RIGHT, GF1VREASON_STOLEN,
				ctx->voice, ctx->voice[VOICE_RIGHT].context);
			}
		ctx->voice[VOICE_RIGHT].callback = NULL;
		ctx->voice[VOICE_RIGHT].context = NULL;
		}

	// Steal notification
	ctx->voice[VOICE_NOTIFY].alloc = GF1VOICE_WAVEOUT;
	if (ctx->voice[VOICE_NOTIFY].callback)
		{
		ctx->voice[VOICE_NOTIFY].callback (VOICE_NOTIFY, GF1VREASON_STOLEN,
			ctx->voice, ctx->voice[VOICE_NOTIFY].context);
		}
	ctx->voice[VOICE_NOTIFY].callback = NULL;
	ctx->voice[VOICE_NOTIFY].context = NULL;

	// Stop voice 0
	common->dwrite (GF1R_VSELECT, VOICE_LEFT);

	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
	common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD_END

	// Set volume, frequency and panning (FIXME: configurable volume?)
	if (self->fmt_stereo)
		{
		common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
		//common->iwrite16 (GF1REGV_VOLUME, STEREO_VOLUME);
		common->iwrite8 (GF1REGV_PANNING, 0);
		common->iwrite16 (GF1REGV_FREQ, 2 << GF1FREQ_FRAC_BITS);
		}
	else
		{
		common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
		//common->iwrite16 (GF1REGV_VOLUME, MONO_VOLUME);
		common->iwrite8 (GF1REGV_PANNING, 8);
		common->iwrite16 (GF1REGV_FREQ, 1 << GF1FREQ_FRAC_BITS);
		}

	// Set loop start, loop end and position
	if (self->fmt_16bit)
		{
		GF1VOICE_START (common, GF1_16BIT_ADDR (GF1MEM_RESERVED_SILENCE));
		GF1VOICE_END (common, GF1_16BIT_ADDR (GF1MEM_RESERVED_SILENCE + self->buffer_cur_length));
		GF1VOICE_POS (common, GF1_16BIT_ADDR (GF1MEM_RESERVED_SILENCE));
		}
	else
		{
		GF1VOICE_START (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));
		GF1VOICE_END (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE + self->buffer_cur_length));
		GF1VOICE_POS (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));
		}

	// Voice 1 if stereo
	if (self->fmt_stereo)
		{
		common->dwrite (GF1R_VSELECT, VOICE_RIGHT);

		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
			common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
		GF1_SELFMOD_END

		common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
		//common->iwrite16 (GF1REGV_VOLUME, STEREO_VOLUME);
		common->iwrite8 (GF1REGV_PANNING, 15);
		common->iwrite16 (GF1REGV_FREQ, 2 << GF1FREQ_FRAC_BITS);

		// Set loop start, end and position
		if (self->fmt_16bit)
			{
			GF1VOICE_START (common, GF1_16BIT_ADDR (GF1MEM_RESERVED_SILENCE + 2));
			GF1VOICE_END (common, GF1_16BIT_ADDR (GF1MEM_RESERVED_SILENCE + 2 +
				self->buffer_cur_length));
			GF1VOICE_POS (common, GF1_16BIT_ADDR (GF1MEM_RESERVED_SILENCE + 2));
			}
		else
			{
			GF1VOICE_START (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE + 1));
			GF1VOICE_END (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE + 1 +
				self->buffer_cur_length));
			GF1VOICE_POS (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE + 1));
			}

		common->dwrite (GF1R_VSELECT, VOICE_LEFT);
		}

	// Start voices
	common->iwrite8 (GF1REGV_VOICE_CTRL, (self->fmt_16bit ? GF1VC_16BIT : 0) | GF1VC_LOOPED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, (self->fmt_16bit ? GF1VC_16BIT : 0) | GF1VC_LOOPED);
	GF1_SELFMOD_END
	if (self->fmt_stereo)
		{
		common->dwrite (GF1R_VSELECT, VOICE_RIGHT);
		common->iwrite8 (GF1REGV_VOICE_CTRL, (self->fmt_16bit ? GF1VC_16BIT : 0) | GF1VC_LOOPED);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_VOICE_CTRL, (self->fmt_16bit ? GF1VC_16BIT : 0) | GF1VC_LOOPED);
		GF1_SELFMOD_END
		common->dwrite (GF1R_VSELECT, VOICE_LEFT);
		}

	// Ramp volumes up
	if (self->fmt_stereo)
		{
		/*common->iwrite8 (GF1REGV_RAMP_START, ZERO_RAMP_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_END, (BYTE) (STEREO_VOLUME >> 8));
		common->iwrite8 (GF1REGV_RAMP_RATE, GF1RR_INCREMENT_MASK | GF1RR_PERIOD_1);

		common->iwrite8 (GF1REGV_RAMP_CTRL, 0);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_RAMP_CTRL, 0);
		GF1_SELFMOD_END

		common->dwrite (GF1R_VSELECT, 1);
		common->iwrite8 (GF1REGV_RAMP_START, ZERO_RAMP_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_END, (BYTE) (STEREO_VOLUME >> 8));
		common->iwrite8 (GF1REGV_RAMP_RATE, GF1RR_INCREMENT_MASK | GF1RR_PERIOD_1);

		common->iwrite8 (GF1REGV_RAMP_CTRL, 0);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_RAMP_CTRL, 0);
		GF1_SELFMOD_END*/
		common->iwrite16 (GF1REGV_VOLUME, STEREO_VOLUME);
		common->dwrite (GF1R_VSELECT, VOICE_RIGHT);
		common->iwrite16 (GF1REGV_VOLUME, STEREO_VOLUME);
		}
	else
		{
		/*common->iwrite8 (GF1REGV_RAMP_START, ZERO_RAMP_VOLUME);
		common->iwrite8 (GF1REGV_RAMP_END, (BYTE) (MONO_VOLUME >> 8));
		common->iwrite8 (GF1REGV_RAMP_RATE, GF1RR_INCREMENT_MASK | GF1RR_PERIOD_1);

		common->iwrite8 (GF1REGV_RAMP_CTRL, 0);
		GF1_SELFMOD(common)
			common->iwrite8 (GF1REGV_RAMP_CTRL, 0);
		GF1_SELFMOD_END*/
		common->iwrite16 (GF1REGV_VOLUME, MONO_VOLUME);
		}

	// Start timer voice (FIXME: this obviously sometimes fails; or is GUS full of bugs?)
	common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
	common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD_END

	common->iwrite16 (GF1REGV_VOLUME, ZERO_RAMP_VOLUME << 8);
	common->iwrite8 (GF1REGV_PANNING, 7);
	common->iwrite16 (GF1REGV_FREQ, 1 << GF1FREQ_FRAC_BITS);
	GF1VOICE_START (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));
	GF1VOICE_END (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE + self->notify_length));
	GF1VOICE_POS (common, GF1_8BIT_ADDR (GF1MEM_RESERVED_SILENCE));

	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED);
	GF1_DELAY (1);
	common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD(common)
		common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD_END

	/*common->iwrite8 (GF1REG_TIMER, 0);
	common->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
	common->dwrite (GF1R_TIMER_DATA, GF1TDATA_CLEAR_IRQ);
	common->dwrite (GF1R_TIMER_DATA, GF1TDATA_T1MASK | GF1TDATA_T2MASK);
	if (self->notify_timer == 1)
		{
		common->iwrite8 (GF1REG_COUNT1, (BYTE) self->timer_count);
		common->iwrite8 (GF1REG_TIMER, GF1TIMER_IRQ1);
		common->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
		common->dwrite (GF1R_TIMER_DATA, GF1TDATA_T1START | GF1TDATA_T2MASK);
		}
	else
		{
		common->iwrite8 (GF1REG_COUNT2, (BYTE) self->timer_count);
		common->iwrite8 (GF1REG_TIMER, GF1TIMER_IRQ2);
		common->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
		common->dwrite (GF1R_TIMER_DATA, GF1TDATA_T2START | GF1TDATA_T1MASK);
		}*/

	ctx->result = TRUE;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Change channel state
 */
NTSTATUS CGF1WavePlaybackStream::SetState
	(
	IN KSSTATE		new_state
	)

	{
	PAGED_CODE();

	if (new_state != KSSTATE_RUN)
		new_state = KSSTATE_STOP;

	if (state == new_state)
		return STATUS_SUCCESS;

	if (new_state == KSSTATE_STOP)
		{
		// Stop playback
		SSStopContext	ctx;
		KIRQL 			old_irql;

		ctx.self = this;
		ctx.voice = miniport->gf1_common->lock_voices (&old_irql);
		miniport->gf1_common->call_synchronized (&ss_stop, &ctx);
		miniport->gf1_common->unlock_voices (old_irql);

		miniport->gf1_common->set_ramp_handler (VOICE_NOTIFY, NULL, NULL, NULL);
		miniport->gf1_common->set_wavetable_handler (VOICE_NOTIFY, NULL, NULL, NULL);
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Stopped");
		}
	else if (new_state == KSSTATE_RUN)
		{
		// Resume playback
		ULONG			i;
		SSRunContext	rctx;
		SSStopContext	sctx;

		rctx.self = this;
		sctx.self = this;

		/* Well, again, my GUS classic is lazy wavetable IRQ generator
		 Note: Because this does not help at all, do not even try
		 */
		//for (i = 0; i < 4; i++)
			{
			//ULONGLONG		t;
			//LARGE_INTEGER	d;
			KIRQL			old_irql;

			// Start stream
			miniport->gf1_common->set_wavetable_handler (VOICE_NOTIFY,
				&wave_ack_handler, &wave_action_handler, this);
			miniport->gf1_common->set_ramp_handler (VOICE_NOTIFY,
				&ramp_ack_handler, &ramp_action_handler, this);

			rctx.voice = miniport->gf1_common->lock_voices (&old_irql);
			miniport->gf1_common->call_synchronized (&ss_run, &rctx);
			miniport->gf1_common->unlock_voices (old_irql);

			if (!rctx.result)
				{
				GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Can't allocate voices");
				return STATUS_INSUFFICIENT_RESOURCES;
				}

			// Wait a bit
			/*t = PcGetTimeInterval (0);
			d.QuadPart = -5 * 10000;	// 5ms intervals
			while (PcGetTimeInterval (t) < GTI_MILLISECONDS (2000 * notify_length / frequency))
				{
				if (notifications)
					break;
				KeDelayExecutionThread (KernelMode, FALSE, &d);
				}

			// Check if IRQ has been generated
			if (notifications)
				break;

			// Well, we failed, so solve it somehow (reset something?) and try again
			sctx.voice = miniport->gf1_common->lock_voices (&old_irql);
			miniport->gf1_common->call_synchronized (&ss_stop, &sctx);
			miniport->gf1_common->unlock_voices (old_irql);

			miniport->gf1_common->set_ramp_handler (VOICE_NOTIFY, NULL, NULL, NULL);
			miniport->gf1_common->set_wavetable_handler (VOICE_NOTIFY, NULL, NULL, NULL);
			t = PcGetTimeInterval (0);
			d.QuadPart = -50 * 10000;	// 50ms intervals
			while (PcGetTimeInterval (t) < GTI_MILLISECONDS (250))
				KeDelayExecutionThread (KernelMode, FALSE, &d);

			sctx.voice = miniport->gf1_common->lock_voices (&old_irql);
			miniport->gf1_common->call_synchronized (&ss_stop, &sctx);
			miniport->gf1_common->unlock_voices (old_irql);*/
			}

		/*if (i == 4)
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Playback jammed");
			}
		else*/
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Playing");
			}
		}

	state = new_state;

	DAILY_DEBUG (this, state == KSSTATE_STOP ? L"CGF1WavePlaybackStream::SetState(stop)" :
		L"CGF1WavePlaybackStream::SetState(run)");

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Clear buffer
 */
void CGF1WavePlaybackStream::Silence
	(
	IN PVOID	dest,
	IN ULONG	length
	)

	{
	RtlFillMemory (dest, length, fmt_16bit ? 0 : 0x80);
	}



/***************************************** Our own stuff ******************************************/

#pragma code_seg()


/* Timer IRQ callback
 */
void CGF1WavePlaybackStream::timer_irq_handler
	(
	IN PVOID	context
	)

	{
	CGF1WavePlaybackStream *	self;
	IGF1Common *				common;

	self = (CGF1WavePlaybackStream *) context;
	common = self->miniport->gf1_common;

	// Restart timer
	/*if (self->notify_timer == 1)
		{
		common->iwrite8 (GF1REG_COUNT1, (BYTE) self->timer_count);
		common->iwrite8 (GF1REG_TIMER, GF1TIMER_IRQ1);
		common->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
		common->dwrite (GF1R_TIMER_DATA, GF1TDATA_T1START | GF1TDATA_T2MASK);
		}
	else
		{
		common->iwrite8 (GF1REG_COUNT2, (BYTE) self->timer_count);
		common->iwrite8 (GF1REG_TIMER, GF1TIMER_IRQ2);
		common->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
		common->dwrite (GF1R_TIMER_DATA, GF1TDATA_T2START | GF1TDATA_T1MASK);
		}*/

	// Notify
	self->miniport->port->Notify (self->service_group);
	}


#pragma code_seg()


/* Wavetable acknowledge callback
 */
void CGF1WavePlaybackStream::wave_ack_handler
	(
	IN ULONG,
	IN PVOID	context
	)

	{
	CGF1WavePlaybackStream *	self;

	self = (CGF1WavePlaybackStream *) context;

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
void CGF1WavePlaybackStream::wave_action_handler
	(
	IN ULONG,
	IN PVOID	context
	)

	{
	CGF1WavePlaybackStream *	self;

	self = (CGF1WavePlaybackStream *) context;

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


/* Volume ramp acknowledge callback
 */
void CGF1WavePlaybackStream::ramp_ack_handler
	(
	IN ULONG,
	IN PVOID	context
	)

	{
	CGF1WavePlaybackStream *	self;

	self = (CGF1WavePlaybackStream *) context;

	// Disable ramps
	self->miniport->gf1_common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	self->miniport->gf1_common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD(self->miniport->gf1_common)
		self->miniport->gf1_common->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED);
	GF1_SELFMOD_END
	}


#pragma code_seg()


/* Volume ramp action callback
 */
void CGF1WavePlaybackStream::ramp_action_handler
	(
	IN ULONG,
	IN PVOID	context
	)

	{
	CGF1WavePlaybackStream *	self;

	self = (CGF1WavePlaybackStream *) context;

	#if GF1_DBG
	self->notify_ramp_irqs++;
	#endif /* GF1_DBG */

	// Enable wavetable (why? why not)
	self->miniport->gf1_common->dwrite (GF1R_VSELECT, VOICE_NOTIFY);

	self->miniport->gf1_common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD(self->miniport->gf1_common)
		self->miniport->gf1_common->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_LOOPED | GF1VC_WAVE_IRQ);
	GF1_SELFMOD_END
	}


#pragma code_seg ("PAGE")


/* Initialize stream
 */
NTSTATUS CGF1WavePlaybackStream::init
	(
	IN CGF1Wave *			_miniport,
	OUT PSERVICEGROUP *		_service_group
	)

	{
	PAGED_CODE();

	NTSTATUS	status;

	ASSERT (_miniport);
	ASSERT (_service_group);

	// Store miniport
	miniport = _miniport;
	miniport->AddRef();

	// Create service group
	status = PcNewServiceGroup (&service_group, NULL);

	// Allocate DMA buffer
	if (NT_SUCCESS (status))
		{
		status = AllocateBuffer (GF1MEM_RESERVED_DMA - 4, NULL);
		}

	if (NT_SUCCESS (status))
		{
		// Return non-addrefed pointer
		*_service_group = service_group;
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Initialized");
		}
	else
		{
		GF1_DBGSTR_ (miniport->gf1_common, L"GF1WaveOutState", L"Can't initialize");
		}

	// Initialize FreeBuffer event
	KeInitializeEvent (&transfer_event, SynchronizationEvent, TRUE);

	return status;

	// SetFormat() should be called ASAP
	}
