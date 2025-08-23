/*
 * cswave.cpp
 *
 * CS4231 compatible (GUS MAX & InterWave codec) Wave miniport
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


#include "cswave.h"
#include "cswaves.h"


/************************************** Filter description ****************************************/

	/************************************** Data ranges ***************************************/

static KSDATARANGE_AUDIO data_range[CODEC_FREQ_COUNT];

static PKSDATARANGE data_range_ptr[CODEC_FREQ_COUNT];

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

static PKSDATARANGE data_range_bridge_ptr[] =
	{
	&data_range_bridge[0]
	};



	/***************************************** Pins *******************************************/

static PCPIN_DESCRIPTOR pin[] =
	{
		// CODECWPIN_WAVEOUT
		{
		1, 1, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_ptr),
			data_range_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_SINK,
			&KSCATEGORY_AUDIO,
			NULL,
			0
			}
		},

		// CODECWPIN_WAVEOUT_BRIDGE
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

		// CODECWWPIN_WAVEIN
		{
		1, 1, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_ptr),
			data_range_ptr,
			KSPIN_DATAFLOW_OUT,
			KSPIN_COMMUNICATION_SINK,
			&PINNAME_CAPTURE,
			&KSAUDFNAME_RECORDING_CONTROL,
			0
			}
		},

		// CODECWPIN_WAVEIN_BRIDGE
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

enum CodecWaveNode
	{
	CODECWNODE_DAC = 0,
	CODECWNODE_ADC
	};

static PCNODE_DESCRIPTOR node[] =
	{
		// CODECWNODE_DAC
		{
		0,
		NULL,
		&KSNODETYPE_DAC,
		NULL
		},

		// CODECWNODE_ADC
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
	{	PCFILTER_NODE,	CODECWPIN_WAVEOUT,			CODECWNODE_DAC,	1						},
	{	CODECWNODE_DAC,	0,							PCFILTER_NODE,	CODECWPIN_WAVEOUT_BRIDGE},

	// WaveIn
	{	PCFILTER_NODE,	CODECWPIN_WAVEIN_BRIDGE,	CODECWNODE_ADC,	1						},
	{	CODECWNODE_ADC,	0,							PCFILTER_NODE,	CODECWPIN_WAVEIN		}
	};



	/*********************************** Filter descriptor ***********************************/

static PCFILTER_DESCRIPTOR codec_descriptor =
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


/* Create new CS4231 compatible wave object
 */
NTSTATUS create_codec_wave
	(
	OUT PUNKNOWN *	unknown,
	IN POOL_TYPE	pool_type
	)
    {
    PAGED_CODE();

    ASSERT (unknown);

    STD_CREATE_BODY (CCSWave, unknown, NULL, pool_type);
    }



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg ("PAGE")


CCSWave::CCSWave
    (
    PUNKNOWN unknown
    ) : CUnknown (unknown)
    {
    PAGED_CODE();

    gf1_common = NULL;
    port = NULL;
    waveout_allocated = FALSE;
    wavein_allocated = FALSE;
	frequency = 0;
	//mutex
    }


#pragma code_seg ("PAGE")


CCSWave::~CCSWave()
    {
    PAGED_CODE();

	// Note: Because streams have references to CCSWave, they can't be running right now
    if (port)
        {
        //port->Release(); we don't reference it to prevent cycles
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
STDMETHODIMP CCSWave::NonDelegatingQueryInterface
	(
	IN REFIID		iface,
    OUT PVOID *		object
	)
    {
	PAGED_CODE();

	ASSERT (object);

	if (IsEqualGUIDAligned (iface, IID_IUnknown))
		{
		*object = PVOID(PUNKNOWN(this));
		}
	else if (IsEqualGUIDAligned (iface, IID_IMiniport))
		{
		*object = PVOID(PMINIPORT(this));
		}
	else if (IsEqualGUIDAligned (iface, IID_IMiniportWaveCyclic))
		{
		*object = PVOID(PMINIPORTWAVECYCLIC(this));
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
NTSTATUS CCSWave::GetDescription
	(
	OUT PPCFILTER_DESCRIPTOR *		descriptor
	)

	{
	PAGED_CODE();

	ASSERT (descriptor);

	*descriptor = &codec_descriptor;

	return STATUS_SUCCESS;
	}



/*********************************** IMiniportWaveCyclic stuff ************************************/

#pragma code_seg ("PAGE")


/* Initialize miniport
 */
NTSTATUS CCSWave::Init
	(
	IN PUNKNOWN			common_unknown,
	IN PRESOURCELIST,
	IN PPORTWAVECYCLIC	_port
	)

	{
	PAGED_CODE();

    NTSTATUS    status;

	ASSERT (common_unknown);
	ASSERT (_port);

	// Keep port. Do not addref it.
	port = _port;
	//port->AddRef();

	// Keep IGF1Common
	// Returns AddRefed pointer.
	status = common_unknown->QueryInterface (IID_IGF1Common, (PVOID *) &gf1_common);

	if (NT_SUCCESS (status))
		{
		// Prepare data range descriptors
		ULONG	i;

		for (i = 0; i < CODEC_FREQ_COUNT; i++)
			{
			data_range[i].DataRange.FormatSize = sizeof (KSDATARANGE_AUDIO);
			data_range[i].DataRange.Flags = 0;
			data_range[i].DataRange.SampleSize = 0;
			data_range[i].DataRange.Reserved = 0;
			data_range[i].DataRange.MajorFormat = KSDATAFORMAT_TYPE_AUDIO;
			data_range[i].DataRange.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			data_range[i].DataRange.Specifier = KSDATAFORMAT_SPECIFIER_WAVEFORMATEX;
			data_range[i].MaximumChannels = 2;
			data_range[i].MinimumBitsPerSample = 8;
			data_range[i].MaximumBitsPerSample = 16;
			data_range[i].MinimumSampleFrequency = codec_freq[i];
			data_range[i].MaximumSampleFrequency = codec_freq[i];
			data_range_ptr[i] = (PKSDATARANGE) &data_range[i];
			}
		}

	// Initialize mutex
	KeInitializeMutex (&mutex, 1);

	// Finish codec reset
	codec_set_playback_frequency (44100);
	codec_set_playback_data_format (TRUE, TRUE, CODEC_COMPRESS_NONE);
	codec_set_record_frequency (44100);
	codec_set_record_data_format (TRUE, TRUE, CODEC_COMPRESS_NONE);

    return status;
    }


#pragma code_seg ("PAGE")


/* Get data range intersection
 */
NTSTATUS CCSWave::DataRangeIntersection
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
NTSTATUS CCSWave::NewStream
	(
	OUT PMINIPORTWAVECYCLICSTREAM *		out_stream,
	IN PUNKNOWN							outer_unknown,
	IN POOL_TYPE						pool_type,
	IN ULONG,
	IN BOOLEAN							capture,
	IN PKSDATAFORMAT					data_format,
	OUT PDMACHANNEL *					out_dma_channel,
	OUT PSERVICEGROUP *					out_service_group
	)
    {
    PAGED_CODE();

	ASSERT (out_stream);
	ASSERT (data_format);
	ASSERT (out_dma_channel);
	ASSERT (out_service_group);

    NTSTATUS    	status	= STATUS_SUCCESS;
	PWAVEFORMATEX	wf		= (PWAVEFORMATEX) (data_format + 1);
	KIRQL			old_irql;

	KeWaitForSingleObject (&mutex, Executive, KernelMode, FALSE, NULL);

	// Check format and stuff
	if (capture)
		{
		if (wavein_allocated)
			status = STATUS_INVALID_DEVICE_REQUEST;
		}
	else
		{
		if (waveout_allocated)
			status = STATUS_INVALID_DEVICE_REQUEST;
		}

	KeReleaseMutex (&mutex, NULL);

	if (NT_SUCCESS (status))
		{
		CCSWaveStream *		str		= NULL;
		PSERVICEGROUP		sg		= NULL;
		PDMACHANNEL			dc		= NULL;

		// Create new stream
		str = create_codec_wave_stream (pool_type, outer_unknown);
		if (!str)
			{
			status = STATUS_INSUFFICIENT_RESOURCES;
			}

		if (NT_SUCCESS (status))
			{
			// Initialize stream
			status = str->init (this, capture, gf1_common->get_dma_resources(), &sg, &dc);
			}

		if (NT_SUCCESS (status))
			{
			// Set recording format - this allocates wavein / waveout
			status = str->SetFormat (data_format);
			}

		if (NT_SUCCESS (status))
			{
			// OK - get outgoing pointers
			*out_service_group = sg;
			sg->AddRef();

			*out_dma_channel = dc;
			dc->AddRef();

			str->QueryInterface (IID_IMiniportWaveCyclicStream, (PVOID *) out_stream);
			}

		// Release local references
		//sg not AddRefed
		//dc not AddRefed
		if (str)
			str->Release();
		}

	return status;
	}



/***************************************** Our own stuff ******************************************/

#pragma code_seg()


struct DFContext
	{
	// This
	CCSWave *		self;

	// Format register (CREG_PLAY_DATA_FORMAT or CREG_REC_DATA_FORMAT)
	BYTE			reg;

	// Which bits to change
	BYTE			mask;

	// New value
	BYTE			value;
	};


/* Write value to playback / record data format register.
 */
NTSTATUS CCSWave::codec_write_data_format
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	DFContext *		ctx = (DFContext *) context;
	IGF1Common *	common = ctx->self->gf1_common;
	BYTE			n;

	// Enable write access to protected registers
	n = common->dread (CR_RSELECT);
	n |= CRSEL_MODECHNG_ENABLE;
	common->dwrite (CR_RSELECT, n);

	// Write value
	n = common->ciread8 (ctx->reg);
	n = (n & ~ctx->mask) | (ctx->value & ctx->mask);
	common->ciwrite8 (ctx->reg, n);

	if (common->get_revision() == GF1REVISION_MAX)
		{
		ULONGLONG t;

		// Original CS4231 needs some care
		common->dread (CR_DATA);	// ERRATA SHEETS ...
		common->dread (CR_DATA);	// ERRATA SHEETS ...

		// Wait till sync is done ...
		t = PcGetTimeInterval (0);
		while (PcGetTimeInterval (t) < GTI_MILLISECONDS (100) &&
			   (common->dread (CR_RSELECT) & 0x80));
			{
			GF1_DELAY (1);
			}

		// Need this. outp doesn't always take ... .
		while (common->dread (CR_RSELECT) != ctx->reg)
			common->dwrite (CR_RSELECT, ctx->reg);

		// Autocalibration - wait till codec is done (no need to preserve old bits)
		t = PcGetTimeInterval (0);
		do	{
			common->dwrite (CR_RSELECT, CREG_STATUS2);
			} while (common->dread (CR_RSELECT) != CREG_STATUS2 &&
				     PcGetTimeInterval (t) < GTI_MILLISECONDS (100));
		t = PcGetTimeInterval (0);
		while ((common->dread (CR_DATA) & CSTAT2_IN_CALIB) &&
		       PcGetTimeInterval (t) < GTI_MILLISECONDS (100))
			{
			common->dwrite (CR_RSELECT, CREG_STATUS2);
			}
		}
	else
		{
		// Disable write access.
		n = common->dread (CR_RSELECT);
		n &= ~CRSEL_MODECHNG_ENABLE;
		common->dwrite (CR_RSELECT, n);
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg ("PAGE")


/* Set codec playback frequency
 */
void CCSWave::codec_set_playback_frequency
	(
	ULONG	freq
	)

	{
	ULONG		i;
	DFContext	ctx;

	// Select "closest" frequency (round up).
	for (i = 0; i < CODEC_FREQ_COUNT - 1; i++)
		{
		if (freq <= codec_freq[i])
			break;
		}

	ctx.self = this;
	ctx.reg = CREG_PLAY_DATA_FORMAT;
	ctx.mask = CDF_FREQ_MASK;
	ctx.value = codec_freq_format[i];
	gf1_common->call_synchronized (&codec_write_data_format, &ctx);
	}


#pragma code_seg ("PAGE")


/* Set codec recording frequency
 */
void CCSWave::codec_set_record_frequency
	(
	ULONG	freq
	)

	{
	ULONG		i;
	DFContext	ctx;

	// Select "closest" frequency (round up).
	for (i = 0; i < CODEC_FREQ_COUNT - 1; i++)
		{
		if (freq <= codec_freq[i])
			break;
		}

	ctx.self = this;
	ctx.reg = (gf1_common->get_codec_mode() == CODEC_MODE3 ?
		CREG_REC_DATA_FORMAT : CREG_PLAY_DATA_FORMAT);
	ctx.mask = CDF_FREQ_MASK;
	ctx.value = codec_freq_format[i];
	gf1_common->call_synchronized (&codec_write_data_format, &ctx);
	}


#pragma code_seg ("PAGE")


/* Set codec playback data format
 */
void CCSWave::codec_set_playback_data_format
	(
	BOOLEAN				stereo,
	BOOLEAN				bit16,
	CODECCompressType	compress
	)

	{
	DFContext	ctx;

	ctx.self = this;
	ctx.reg = CREG_PLAY_DATA_FORMAT;
	ctx.mask = CDF_DATAF_MASK;
	if (bit16)
		{
		if (compress == CODEC_COMPRESS_ADPCM)
			ctx.value = CDF_ADPCM;	// 16-bit ADPCM
		else
			ctx.value = CDF_16LE;	// 16-bit little endian
		}
	else
		{
		if (compress == CODEC_COMPRESS_ALAW)
			ctx.value = CDF_A_LAW;	// 8-bit, companded Alaw
		else
			if (compress == CODEC_COMPRESS_ULAW)
				ctx.value = CDF_U_LAW;
			else
				ctx.value = CDF_8;
		}
	if (stereo)
		ctx.value |= CDF_STEREO;
	gf1_common->call_synchronized (&codec_write_data_format, &ctx);
	}


#pragma code_seg ("PAGE")


/* Set codec record data format
 */
void CCSWave::codec_set_record_data_format
	(
	BOOLEAN				stereo,
	BOOLEAN				bit16,
	CODECCompressType	compress
	)

	{
	DFContext	ctx;

	ctx.self = this;
	if (gf1_common->get_codec_mode() == CODEC_MODE1) // FIXME: or single DMA (?)
		ctx.reg = CREG_PLAY_DATA_FORMAT;
	else
		ctx.reg = CREG_REC_DATA_FORMAT;
	ctx.mask = CDF_DATAF_MASK;
	if (bit16)
		{
		if (compress == CODEC_COMPRESS_ADPCM)
			ctx.value = CDF_ADPCM;	// 16-bit ADPCM
		else
			ctx.value = CDF_16LE;	// 16-bit little endian
		}
	else
		{
		if (compress == CODEC_COMPRESS_ALAW)
			ctx.value = CDF_A_LAW;	// 8-bit, companded Alaw
		else
			if (compress == CODEC_COMPRESS_ULAW)
				ctx.value = CDF_U_LAW;
			else
				ctx.value = CDF_8;
		}
	if (stereo)
		ctx.value |= CDF_STEREO;
	gf1_common->call_synchronized (&codec_write_data_format, &ctx);
	}
