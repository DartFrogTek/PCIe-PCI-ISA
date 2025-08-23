/*
 * cswaves.cpp
 *
 * CS4231 compatible (GUS MAX & InterWave) Wave stream
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


#include "cswaves.h"


// Dma buffer length
#define BUFFER_LENGTH		16384


// Debugging info
#if GF1_DBG
#define DAILY_DEBUG(c,f) \
	{ \
	(c)->miniport->gf1_common->debug_to_registry (f); \
	GF1_DBGINT_ ((c)->miniport->gf1_common, (c)->capture ? L"CodecWaveInNotifications" : \
		L"CodecWaveOutNotifications", (c)->notifications); \
	}
#else /* GF1_DBG */
#define DAILY_DEBUG(c) {}
#endif /* GF1_DBG */


/* Create new wave stream object
 */
CCSWaveStream *create_codec_wave_stream
	(
    IN POOL_TYPE	pool_type,
	IN PUNKNOWN		outer_unknown
	)

	{
    PAGED_CODE();

    ASSERT (unknown);

	CCSWaveStream *p = new (pool_type) CCSWaveStream (outer_unknown);
	if (p)
		{
		p->AddRef();
		}

	return p;
	}



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg ("PAGE")


CCSWaveStream::CCSWaveStream
	(
	PUNKNOWN	unknown
	) : CUnknown (unknown)

	{
	PAGED_CODE();

	miniport = NULL;
	capture = FALSE;
	fmt_16bit = FALSE;
	fmt_stereo = FALSE;
	frequency = 0;
	notify_length = 0;
	dma_channel = NULL;
	service_group = NULL;
	state = KSSTATE_STOP;
	#if GF1_DBG
	notifications = 0;
	#endif /* GF1_DBG */
	}


#pragma code_seg ("PAGE")


CCSWaveStream::~CCSWaveStream()
	{
	PAGED_CODE();

	if (service_group)
		{
		service_group->Release();
		service_group = NULL;
		}

	if (dma_channel)
		{
		dma_channel->Release();
		dma_channel = NULL;
		if (miniport)
			{
			miniport->gf1_common->put_dma (capture ?
				GF1DMACHANNEL_CODEC_RECORD : GF1DMACHANNEL_CODEC_PLAYBACK);
			}
		}

	if (miniport)
		{
		if (capture)
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"CodecWaveInState", L"Destroyed");
			miniport->wavein_allocated = FALSE;
			}
		else
			{
			GF1_DBGSTR_ (miniport->gf1_common, L"CodecWaveOutState", L"Destroyed");
			miniport->waveout_allocated = FALSE;
			}
		miniport->Release();
		miniport = NULL;
		}
	}


#pragma code_seg ("PAGE")


/* Obtain an interface
 */
STDMETHODIMP CCSWaveStream::NonDelegatingQueryInterface
	(
	IN REFIID		iface,
    OUT PVOID *		object
	)

	{
	PAGED_CODE();

	ASSERT (object);

	if (IsEqualGUIDAligned (iface, IID_IUnknown))
		{
        *object = (PVOID) ((PUNKNOWN) this);
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


/* Set playback format
 */
NTSTATUS CCSWaveStream::SetFormat
	(
	IN PKSDATAFORMAT	format
	)

	{
	PAGED_CODE();

	NTSTATUS	status = STATUS_SUCCESS;

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
	{
	ULONG fid;

	for (fid = 0; fid < CODEC_FREQ_COUNT; fid++)
		{
		if (wf->nSamplesPerSec == codec_freq[fid])
			break;
		}

	if (fid >= CODEC_FREQ_COUNT)
		return STATUS_INVALID_PARAMETER;
	}//eob (frequency)

	// Check duplex stuff
	KeWaitForSingleObject (&miniport->mutex, Executive, KernelMode, FALSE, NULL);

	if (capture)
		{
		miniport->wavein_allocated = TRUE;
		if (miniport->gf1_common->get_codec_mode() == CODEC_MODE3)
			{
			frequency = wf->nSamplesPerSec;
			}
		else
			{
			if (miniport->waveout_allocated && miniport->frequency != wf->nSamplesPerSec)
				{
				status = STATUS_INVALID_PARAMETER;
				GF1_DBGINT_ (miniport->gf1_common, L"CodecWaveInInvalidFrequency",
					wf->nSamplesPerSec);
				}
			else
				{
				miniport->frequency = wf->nSamplesPerSec;
				frequency = wf->nSamplesPerSec;
				}
			}
		}
	else
		{
		miniport->waveout_allocated = TRUE;
		if (miniport->gf1_common->get_codec_mode() == CODEC_MODE3)
			{
			frequency = wf->nSamplesPerSec;
			}
		else
			{
			if (miniport->wavein_allocated && miniport->frequency != wf->nSamplesPerSec)
				{
				status = STATUS_INVALID_PARAMETER;
				GF1_DBGINT_ (miniport->gf1_common, L"CodecWaveOutInvalidFrequency",
					wf->nSamplesPerSec);
				}
			else
				{
				miniport->frequency = wf->nSamplesPerSec;
				frequency = wf->nSamplesPerSec;
				}
			}
		}

	KeReleaseMutex (&miniport->mutex, FALSE);

	if (NT_SUCCESS (status))
		{
		// OK? - change format
		fmt_16bit = (wf->wBitsPerSample == 16 ? TRUE : FALSE);
		fmt_stereo = (wf->nChannels == 2 ? TRUE : FALSE);

		/*
		 I hope 'SetNotificationFreq()' will be always called after 'SetFormat()'.
		 */

		GF1_DBGINT_ (miniport->gf1_common, capture ? L"CodecWaveInChannels" :
			L"CodecWaveOutChannels", fmt_stereo ? 2 : 1);
		GF1_DBGINT_ (miniport->gf1_common, capture ? L"CodecWaveInBitsPerSample" :
			L"CodecWaveOutBitsPerSample", fmt_16bit ? 16 : 8);
		GF1_DBGINT_ (miniport->gf1_common, capture ? L"CodecWaveInFrequency" :
			L"CodecWaveOutFrequency", frequency);
		}
	DAILY_DEBUG (this, L"CCSWaveStream::SetFormat");

	return status;
	}


#pragma code_seg ("PAGE")


/* Set notification frequency
 */
ULONG CCSWaveStream::SetNotificationFreq
	(
    IN      ULONG   interval,
    OUT     PULONG  framing_size
	)

	{
    PAGED_CODE();

	notify_length = frequency * interval / 1000;
	if (notify_length > 0xffff)
		notify_length = 0xffff;
	else if (notify_length < 0x10)
		notify_length = 0x10;
	*framing_size = notify_length << (fmt_stereo ? fmt_16bit ? 2 : 1 : fmt_16bit ? 1 : 0);

	GF1_DBGINT_ (miniport->gf1_common, capture ? L"CodecWaveInNotifyLength" :
		L"CodecWaveOutNotifyLength", notify_length);
	DAILY_DEBUG (this, L"CCSWaveStream::SetNotificationFreq");

	return notify_length * 1000 / frequency;
	}


#pragma code_seg()


/* Gets the current position.
 */
NTSTATUS CCSWaveStream::GetPosition
	(
    OUT PULONG		position
	)

	{
    ASSERT (position);

	if (!dma_channel || state != KSSTATE_RUN)
		{
		*position = 0;
		}
	else
		{
		ULONG transfer_count = dma_channel->TransferCount();

		if (transfer_count)
			{
			*position = dma_channel->ReadCounter();

			if (*position < transfer_count)
				{
	            *position = transfer_count - *position;
				}
			else
				{
				*position = 0;
				}
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
NTSTATUS CCSWaveStream::NormalizePhysicalPosition
	(
	IN OUT PLONGLONG	position
	)

	{
	*position = (_100NS_UNITS_PER_SECOND *
		(*position >> (fmt_stereo ? fmt_16bit ? 2 : 1 : fmt_16bit ? 1 : 0))) / frequency;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Stop stream
 */
NTSTATUS CCSWaveStream::ss_stop
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	CCSWaveStream *		self;
	IGF1Common *		common;

	self = (CCSWaveStream *) context;
	common = self->miniport->gf1_common;

	// Stop playback / recording
	common->ciwrite8 (CREG_CONFIG, common->ciread8 (CREG_CONFIG) &
		~(self->capture ? CCFG_RECORD : CCFG_PLAYBACK));

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Resume playback / recording
 */
NTSTATUS CCSWaveStream::ss_run
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	CCSWaveStream *		self;
	IGF1Common *		common;

	self = (CCSWaveStream *) context;
	common = self->miniport->gf1_common;

	if (self->capture)
		{
		// Program counter
		common->ciwrite8 (CREG_RCOUNT_LO, (BYTE) (self->notify_length & 0xff));
		common->ciwrite8 (CREG_RCOUNT_HI, (BYTE) (self->notify_length >> 8));

		// Enable recording
		common->ciwrite8 (CREG_CONFIG, common->ciread8 (CREG_CONFIG) | CCFG_RECORD);
		}
	else
		{
		// Program counter
		common->ciwrite8 (CREG_PCOUNT_LO, (BYTE) (self->notify_length & 0xff));
		common->ciwrite8 (CREG_PCOUNT_HI, (BYTE) (self->notify_length >> 8));

		// Enable playback
		common->ciwrite8 (CREG_CONFIG, common->ciread8 (CREG_CONFIG) | CCFG_PLAYBACK);
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg ("PAGE")


/* Change channel state
 */
NTSTATUS CCSWaveStream::SetState
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
		// Stop playback or record
		miniport->gf1_common->call_synchronized (&ss_stop, this);
		dma_channel->Stop();
		if (capture)
			miniport->gf1_common->set_record_handler (NULL, NULL);
		else
			miniport->gf1_common->set_playback_handler (NULL, NULL);
		GF1_DBGSTR_ (miniport->gf1_common, capture ? L"CodecWaveInState" :
			L"CodecWaveOutState", L"Stopped");
		}
	else if (new_state == KSSTATE_RUN)
		{
		// Start playback or record
		if (capture)
			{
			miniport->codec_set_record_frequency (frequency);
			miniport->codec_set_record_data_format (fmt_stereo, fmt_16bit, CODEC_COMPRESS_NONE);
			miniport->gf1_common->set_record_handler (&codec_irq_callback, this);
			}
		else
			{
			miniport->codec_set_playback_frequency (frequency);
			miniport->codec_set_playback_data_format (fmt_stereo, fmt_16bit, CODEC_COMPRESS_NONE);
			miniport->gf1_common->set_playback_handler (&codec_irq_callback, this);
			}
		dma_channel->Start (dma_channel->BufferSize(), capture ? FALSE : TRUE);
		miniport->gf1_common->call_synchronized (&ss_run, this);
		GF1_DBGSTR_ (miniport->gf1_common, capture ? L"CodecWaveInState" :
			L"CodecWaveOutState", L"Playing");
		}

	state = new_state;
	DAILY_DEBUG (this,
		state == KSSTATE_STOP ?
			capture ?
				L"CCSWaveStream::SetState(stop_recording)" :
				L"CCSWaveStream::SetState(stop_playback)" :
			capture ?
				L"CCSWaveStream::SetState(run_recording)" :
				L"CCSWaveStream::SetState(run_playback)");

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Clear buffer
 */
void CCSWaveStream::Silence
	(
	IN PVOID	buffer,
	IN ULONG	length
	)

	{
	RtlFillMemory (buffer, length, fmt_16bit ? 0 : 0x80);
	}


#pragma code_seg()


/* Codec IRQ callback
 */
void CCSWaveStream::codec_irq_callback
	(
	IN PVOID	context
	)

	{
	CCSWaveStream *		self = (CCSWaveStream *) context;

	#if GF1_DBG
	self->notifications++;
	#endif /* GF1_DBG */

	// IRQ Already acknowledged, so just notify
	self->miniport->port->Notify (self->service_group);
	}


#pragma code_seg ("PAGE")


/* Initialize stream
 */
NTSTATUS CCSWaveStream::init
	(
	IN CCSWave *			_miniport,
	IN BOOLEAN				_capture,
	IN PRESOURCELIST		resources,
	OUT PSERVICEGROUP *		out_service_group,
	OUT PDMACHANNEL *		out_dma_channel
	)

	{
	PAGED_CODE();

	NTSTATUS	status;

	ASSERT (_miniport);
	ASSERT (out_service_group);
	ASSERT (out_dma_channel);

	// Store miniport
	miniport = _miniport;
	miniport->AddRef();

	capture = _capture;

	// Get DMA channel
	if (!miniport->gf1_common->get_dma (capture ? GF1DMACHANNEL_CODEC_RECORD :
		GF1DMACHANNEL_CODEC_PLAYBACK))
		{
		status = STATUS_INSUFFICIENT_RESOURCES;
		}
	else
		{
		PCM_PARTIAL_RESOURCE_DESCRIPTOR		rd;

		// Allocate DMA adapter
		rd = resources->FindTranslatedDma (capture ? 0 : 1);
		dma_channel = NULL;
		status = miniport->port->NewSlaveDmaChannel (&dma_channel, NULL,
			resources, capture ? 0 : 1, BUFFER_LENGTH, TRUE,
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
			GF1_DBGINT_ (miniport->gf1_common, capture ? L"CodecWaveInBufferSize" :
				L"CodecWaveOutBufferSize", dma_length);
			}

		if (!NT_SUCCESS (status))
			{
			// DMA channel allocation failed
			if (dma_channel)
				dma_channel->Release();
			dma_channel = NULL;
			miniport->gf1_common->put_dma (capture ? GF1DMACHANNEL_CODEC_RECORD :
				GF1DMACHANNEL_CODEC_PLAYBACK);
			}
		}

	// Create service group
	if (NT_SUCCESS (status))
		status = PcNewServiceGroup (&service_group, NULL);

	if (NT_SUCCESS (status))
		{
		// Return non-addrefed pointers
		*out_service_group = service_group;
		*out_dma_channel = dma_channel;
		GF1_DBGSTR_ (miniport->gf1_common, capture ? L"CodecWaveInState" :
			L"CodecWaveOutState", L"Initialized");
		}

	return status;

	// SetFormat() should be called ASAP
	}


