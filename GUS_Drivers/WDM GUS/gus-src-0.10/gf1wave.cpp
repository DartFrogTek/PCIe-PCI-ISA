/*
 * gf1wave.h
 *
 * GF1 Wave miniport
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


#include "gf1wave.h"
#include "gf1wavep.h"
#include "gf1waver.h"



/************************************** Filter description ****************************************/

	/************************************** Data ranges ***************************************/

static KSDATARANGE_AUDIO data_range_out[32-14+1];

static KSDATARANGE_AUDIO data_range_in[GF1_RECFREQ_COUNT];

static KSDATARANGE data_range_bridge[] =
	{
		{
		sizeof (KSDATARANGE_AUDIO),
		0,
		0,
		0,
		STATICGUIDOF (KSDATAFORMAT_TYPE_AUDIO),
		STATICGUIDOF (KSDATAFORMAT_SUBTYPE_ANALOG),
		STATICGUIDOF (KSDATAFORMAT_SPECIFIER_NONE)
		}
	};


/* FIXME: mono channels don't have such frequency restrictions, but I don't really care.
 DatarangeIntersection could have serious problems if another range was used (there is
 no MinimumChannels and it could select mono instead of different frequency). I'm not
 going to write my own DatarangeIntersection. And who wants frequencies < 20kHz? And
 WDM audio automatically converts sample rates from user-specified to hw-supported.
 */
static PKSDATARANGE data_range_out_ptr[32-14+1];

static PKSDATARANGE data_range_in_ptr[GF1_RECFREQ_COUNT];

static PKSDATARANGE data_range_bridge_ptr[] =
	{
	&data_range_bridge[0]
	};



	/***************************************** Pins *******************************************/

static PCPIN_DESCRIPTOR pin[] =
	{
		// GF1WPIN_WAVEOUT (FIXME: support multiple instances?)
		{
		1, 1, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_out_ptr),
			data_range_out_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_SINK,
			&KSCATEGORY_AUDIO,
			NULL,
			0
			}
		},

		// GF1WPIN_WAVEOUT_BRIDGE
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_OUT,
			KSPIN_COMMUNICATION_NONE,
			&KSCATEGORY_AUDIO,
			NULL,
			0
			}
		},

		// GF1WPIN_WAVEIN
		{
		1, 1, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_in_ptr),
			data_range_in_ptr,
			KSPIN_DATAFLOW_OUT,
			KSPIN_COMMUNICATION_SINK,
			&PINNAME_CAPTURE,
			&KSAUDFNAME_RECORDING_CONTROL,
			0
			}
		},

		// GF1WPIN_WAVEIN_BRIDGE
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSCATEGORY_AUDIO,
			NULL,
			0
			}
		}
	};



	/***************************************** Nodes ******************************************/

// FIXME: add volume control to waveout?
enum GF1WaveNode
	{
	GF1WNODE_DAC = 0,
	GF1WNODE_ADC
	};

static PCNODE_DESCRIPTOR node[] =
	{
		// GF1WNODE_DAC
		{
		0,
		NULL,
		&KSNODETYPE_DAC,
		NULL
		},

		// GF1WNODE_ADC
		{
		0,
		NULL,
		&KSNODETYPE_ADC,
		NULL
		}
	};


	/************************************** Connections ***************************************/

static PCCONNECTION_DESCRIPTOR connection[] =
	{
	// WaveOut
	{	PCFILTER_NODE,	GF1WPIN_WAVEOUT,			GF1WNODE_DAC,	1						},
	{	GF1WNODE_DAC,	0,							PCFILTER_NODE,	GF1WPIN_WAVEOUT_BRIDGE	},

	// WaveIn

	{	PCFILTER_NODE,	GF1WPIN_WAVEIN_BRIDGE,		GF1WNODE_ADC,	1						},
	{	GF1WNODE_ADC,	0,							PCFILTER_NODE,	GF1WPIN_WAVEIN			}
	};


	/*********************************** Filter descriptor ***********************************/

static PCFILTER_DESCRIPTOR gf1_descriptor =
	{
	0,
	NULL,

	// Pins
	sizeof (PCPIN_DESCRIPTOR),
	SIZEOF_ARRAY (pin),
	pin,

	// Nodes
	sizeof (PCNODE_DESCRIPTOR),
	SIZEOF_ARRAY (node),
	node,

	// Connections
	SIZEOF_ARRAY (connection),
	connection,

	0,
	NULL
	};



/********************************************* Code ***********************************************/

#pragma code_seg("PAGE")


/* Create new GF1 wave object
 */
NTSTATUS create_gf1_wave
	(
    OUT PUNKNOWN *	unknown,
    IN POOL_TYPE	pool_type
	)

	{
    PAGED_CODE();

    ASSERT (unknown);

    STD_CREATE_BODY (CGF1Wave, unknown, NULL, pool_type);
	}



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg ("PAGE")


CGF1Wave::CGF1Wave
	(
	PUNKNOWN	unknown
	) : CUnknown (unknown)

	{
	PAGED_CODE();

	gf1_common = NULL;
	port = NULL;
	waveout_allocated = FALSE;
	wavein_allocated = FALSE;
	}


#pragma code_seg ("PAGE")


CGF1Wave::~CGF1Wave()
	{
	// Note: Because streams have references to CGF1Wave, they can't be running right now
	#if GF1_DBG
	if (gf1_common)
		{
		GF1_DBGINT_ (gf1_common, L"DestroyingCGF1Wave", 1);
		}
	#endif /* GF1_DBG */

	if (port)
		{
		//port->Release();
		port = NULL;
		}

	if (gf1_common)
		{
		gf1_common->Release();
		gf1_common = NULL;
		}
	}


#pragma code_seg ("PAGE")


/* Obtain an interface
 */
STDMETHODIMP CGF1Wave::NonDelegatingQueryInterface
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
	else if (IsEqualGUIDAligned (iface, IID_IMiniport))
		{
		*object = (PVOID) ((PMINIPORT) this);
		}
	else if (IsEqualGUIDAligned (iface, IID_IMiniportWaveCyclic))
		{
		*object = (PVOID) ((PMINIPORTWAVECYCLIC) this);
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



/**************************************** IMiniport stuff *****************************************/

#pragma code_seg ("PAGE")


/* Get topology description
 */
NTSTATUS CGF1Wave::GetDescription
	(
	OUT PPCFILTER_DESCRIPTOR *		descriptor
	)

	{
	PAGED_CODE();

	ASSERT (descriptor);

	*descriptor = &gf1_descriptor;

	return STATUS_SUCCESS;
	}



/*********************************** IMiniportWaveCyclic stuff ************************************/

#pragma code_seg ("PAGE")


/* Initialize miniport
 */
NTSTATUS CGF1Wave::Init
	(
	IN PUNKNOWN			common_unknown,
	IN PRESOURCELIST,
	IN PPORTWAVECYCLIC	_port
	)

	{
	PAGED_CODE();

	NTSTATUS status;

	ASSERT (common_unknown);
	ASSERT (_port);

	// Keep port
	port = _port;
	/* Note:
	 Do not addref it, because ports addrefs us - and we don't want cycles. Port is
	 responsible for destroying miniport, so this pointer should be always valid.
	 */
	//port->AddRef();

	// Keep IGF1Common
	status = common_unknown->QueryInterface (IID_IGF1Common, (PVOID *) &gf1_common);

	if (NT_SUCCESS (status))
		{
		ULONG i;

		// Prepare waveout data range descriptors
		for (i = 14; i <= 32; i++)
			{
			data_range_out[i-14].DataRange.FormatSize = sizeof (KSDATARANGE_AUDIO);
			data_range_out[i-14].DataRange.Flags = 0;
			data_range_out[i-14].DataRange.SampleSize = 0;
			data_range_out[i-14].DataRange.Reserved = 0;
			data_range_out[i-14].DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
			data_range_out[i-14].DataRange.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			data_range_out[i-14].DataRange.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
			data_range_out[i-14].MaximumChannels = 2;
			data_range_out[i-14].MinimumBitsPerSample = 8;
			data_range_out[i-14].MaximumBitsPerSample = 16;
			data_range_out[i-14].MinimumSampleFrequency = gf1_voices2frequency[i-14];
			data_range_out[i-14].MaximumSampleFrequency = gf1_voices2frequency[i-14];
			data_range_out_ptr[i-14] = (PKSDATARANGE) &data_range_out[i-14];
			}

		// Prepare wavein data range descriptors
		for (i = 0; i < GF1_RECFREQ_COUNT; i++)
			{
			data_range_in[i].DataRange.FormatSize = sizeof (KSDATARANGE_AUDIO);
			data_range_in[i].DataRange.Flags = 0;
			data_range_in[i].DataRange.SampleSize = 0;
			data_range_in[i].DataRange.Reserved = 0;
			data_range_in[i].DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
			data_range_in[i].DataRange.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			data_range_in[i].DataRange.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
			data_range_in[i].MaximumChannels = 2;
			data_range_in[i].MinimumBitsPerSample = 8;
			data_range_in[i].MaximumBitsPerSample = 8;
			data_range_in[i].MinimumSampleFrequency = gf1_recfreq[i];
			data_range_in[i].MaximumSampleFrequency = gf1_recfreq[i];
			data_range_in_ptr[i] = (PKSDATARANGE) &data_range_in[i];
			}
		}

	return status;
	}


#pragma code_seg ("PAGE")


/* Get data range intersection
 */
NTSTATUS CGF1Wave::DataRangeIntersection
	(
	IN ULONG,
	IN PKSDATARANGE,
	IN PKSDATARANGE,
	IN ULONG,
	OUT PVOID,
	OUT PULONG
	)

	{
	// Let portcls do the work
	return STATUS_NOT_IMPLEMENTED;
	}


#pragma code_seg ("PAGE")


/* Create new wave stream
 */
NTSTATUS CGF1Wave::NewStream
	(
	OUT PMINIPORTWAVECYCLICSTREAM *		stream,
	IN PUNKNOWN							outer_unknown,
	IN POOL_TYPE						pool_type,
	IN ULONG							pin,
	IN BOOLEAN							capture,
	IN PKSDATAFORMAT					data_format,
	OUT PDMACHANNEL *					dma_channel,
	OUT PSERVICEGROUP *					service_group
	)

	{
	PAGED_CODE();

	NTSTATUS	status = STATUS_SUCCESS;

	ASSERT (stream);
	ASSERT (data_format);
	ASSERT (dma_channel);
	ASSERT (service_group);

	if (capture)
		{
		if (wavein_allocated)
			{
			status = STATUS_INVALID_DEVICE_REQUEST;
			}
		else
			{
			CGF1WaveRecordStream *		str		= NULL;
			PSERVICEGROUP 				sg		= NULL;
			PDMACHANNEL					dc		= NULL;

			// Create new stream
			str = create_gf1_wave_record_stream (pool_type, outer_unknown);
			if (!str)
				{
				status = STATUS_INSUFFICIENT_RESOURCES;
				}

			if (NT_SUCCESS (status))
				{
				// Initialize stream
				status = str->init (this, gf1_common->get_dma_resources(), &sg, &dc);
				}

			if (NT_SUCCESS (status))
				{
				// Set recording format
				status = str->SetFormat (data_format);
				}

			if (NT_SUCCESS (status))
				{
				// OK - get outgoing pointers
				*service_group = sg;
				sg->AddRef();

				*dma_channel = dc;
				dc->AddRef();

				str->QueryInterface (IID_IMiniportWaveCyclicStream, (PVOID *) stream);

				wavein_allocated = TRUE;
				}

			// Release local references
			//sg not AddRefed
			//dc not AddRefed
			if (str)
				str->Release();
			}
		}
	else
		{
		if (waveout_allocated)
			{
			/*
			 Only one waveout voice supported, because it takes up to three GF1 voices
			 and pretty big part of DRAM DMA bandwidth.
			 */
			status = STATUS_INVALID_DEVICE_REQUEST;
			GF1_DBGSTR_ (gf1_common, L"GF1WaveOutState", L"Tried to reinitialize");
			}
		else if (!gf1_common->get_dma (GF1DMACHANNEL_DRAM))
			{
			// Another DMA channel is conflicting with DRAM DMA
			status = STATUS_INSUFFICIENT_RESOURCES;
			GF1_DBGSTR_ (gf1_common, L"GF1WaveOutState", L"Can't allocate DMA");
			}
		else
			{
			// DRAM DMA is referenced by this stream and will be put back in its destructor
			CGF1WavePlaybackStream *	str		= NULL;
			PSERVICEGROUP 				sg		= NULL;

			// Create new stream
			str = create_gf1_wave_playback_stream (pool_type, outer_unknown);
			if (!str)
				{
				status = STATUS_INSUFFICIENT_RESOURCES;
				}

			if (NT_SUCCESS (status))
				{
				// Initialize stream
				status = str->init (this, &sg);
				if (!NT_SUCCESS (status))
					{
					/* Free DRAM DMA
				 	Note: If 'init' succeeds, stream is responsible for destroying DMA.
				 	*/
					gf1_common->put_dma (GF1DMACHANNEL_DRAM);
					}
				}

			if (NT_SUCCESS (status))
				{
				// Set playback format
				status = str->SetFormat (data_format);
				}

			if (NT_SUCCESS (status))
				{
				// OK - get outgoing pointers
				*service_group = sg;
				sg->AddRef();

				str->QueryInterface (IID_IDmaChannel, (PVOID *) dma_channel);

				str->QueryInterface (IID_IMiniportWaveCyclicStream, (PVOID *) stream);

				waveout_allocated = TRUE;
				}

			// Release local references
			//sg not AddRefed - stream will destroy it
			if (str)
				str->Release();
			}
		}

	return status;
	}
