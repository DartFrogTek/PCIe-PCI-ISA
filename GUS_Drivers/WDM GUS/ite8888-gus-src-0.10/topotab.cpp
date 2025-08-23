/*
 * topotab.cpp
 *
 * Topology tables
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


#include "topoguid.h"


/* GF1 topology (no CS4231):

 WaveOut >-----+S+
               +U+-----+---------+S+
   Synth >-----+M+     |  +------+U+-M-> LineOut
                       |  |  +---+M+
                       |  |  |
                       +--|--|---+S+
  LineIn >----------M-----+--|---+U+---> WaveIn
                             | +-+M+
                             | |
   MicIn >----------M--------+-+

 Notes:
 HW does not support LineIn/MicIn volume.
 Actually, WaveOut = Synth if CS4231 is not present.
 WaveOut/Synth is always mixed to WaveIn.
 Because there are not controls between GF1 SUM and other sums, WaveOut and Synth
 are directly connected to these.

 FIXME: Fake WaveOut/Synth volumes or should these be implemented in Wave/DMusic miniports?
 FIXME: CdIn == LineIn?
 */


/* ICS topology (FIXME)
 Pretty stupid mixer, just adds volume levels all inputs. Not supported - nobody has the HW.
 */


/* CS4231 - Max / InterWave topology:

A attenuation
G gain/attenuation (gain only after wavein mux)
M mute
(.) property not supported
X connection not supported

  Synth/AUX1 >--------------------+--+
                                  |  |
                                  |  +--M/G------+++
     WaveOut >--------------------|-----M/A------+S+
                         +--------|-----M/G------+U+--+-----M/A-------> LineOut
                         |  +-----|-----M/G------+M+  |
                         |  |  +--|-----M/G------+++  |
                         |  |  |  |                   |
                         |  |  |  |                   +---+++
                         |  |  |  |                       +M+
                         |  |  |  +-----------------------+U+----G----> WaveIn
                         |  |  |     +--------------------+X+
    CD(AUX2) >-----------+  |  |     |  +----------(G)----+++
                            |  |     |  |
      LineIn >--------------+--------+  |
                               |        |
       MicIn >----M------------+--------+


 Notes:

 Not all stuff supported here:
 - MicIn-to-mux 20 dB gain on/off - orig. CS4231 (Max) only
 - Old-style LineIn mute - sndvol32 would be probably just confused by it
   (note: old-style MicIn mute is supported just because new mic stuff is not)
 - Digital loopback + attenuation (should be in wave miniport)
 - SynthIn is not used (useless until DirectMusic miniport works)

 Interwave-only:
 - MicIn-to-sum gain (-34.5 to 12 dB)
 - LineOut attenuation / stereo mute

 Max-only:
 - Old-style MicIn master mute

 This is how supported subset of the mixer looks, but exported topology must be a bit
 different, so volume controls and stuff like that work:
 - Sum-to-Mux line is exported as WhatYouHear-to-Mux line (WUH being virtual audio source)
 - Instead of single after-Mux gain, every Mux input has its own fake gain.

 To reduce code/data size, Max and Interwave topologies share node tables:
 - CSNODE_MICIN_MUTE is "master" mic mute on Max and mic-to-sum mute on PnP.
 - IWNODE_* are not visible on Max.
*/



/****************************************** Data ranges *******************************************/

// FIXME: should I do something like #pragma data_seg ("PAGE") or what?

/* Bridge pin data range
 */
static KSDATARANGE data_range_bridge[] =
	{
		{
		sizeof (KSDATARANGE),
		0,
		0,
		0,
		STATICGUIDOF (KSDATAFORMAT_TYPE_AUDIO),
		STATICGUIDOF (KSDATAFORMAT_SUBTYPE_ANALOG),
		STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
		}
	};

static PKSDATARANGE data_range_bridge_ptr[] =
	{
	&data_range_bridge[0]
	};



/********************************************* Pins ***********************************************/

static PCPIN_DESCRIPTOR pin[] =
	{
		// GF1TPIN_WAVEOUT
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_LEGACY_AUDIO_CONNECTOR,
			NULL,
			0
			}
		},

		// GF1TPIN_SYNTH
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_SYNTHESIZER,
			&KSAUDFNAME_MIDI,
			0
			}
		},

		// GF1TPIN_LINEIN
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_LINE_CONNECTOR,
			&KSAUDFNAME_LINE_IN,
			0
			}
		},

		// GF1TPIN_MICIN
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_MICROPHONE,
			NULL,
			0
			}
		},

		// GF1TPIN_LINEOUT
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_OUT,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_SPEAKER,
			&KSAUDFNAME_VOLUME_CONTROL,
			0
			}
		},

		// GF1TPIN_WAVEIN
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

		// CSTPIN_CDIN (Max+ only)
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_CD_PLAYER,
			&KSAUDFNAME_CD_AUDIO,
			0
			}
		},

		// CSTPIN_MIXERIN (Max+ only)
		{
		0, 0, 0,
		NULL,
			{
			0, NULL, 0, NULL,
			SIZEOF_ARRAY (data_range_bridge_ptr),
			data_range_bridge_ptr,
			KSPIN_DATAFLOW_IN,
			KSPIN_COMMUNICATION_NONE,
			&KSNODETYPE_ANALOG_CONNECTOR,
			&GUSAUDFNAME_MIXER_IN,
			0
			}
		}
	};



/****************************************** Properties ********************************************/

// Mute
static PCPROPERTY_ITEM prop_mute[] =
	{
		{
		&KSPROPSETID_Audio,
		KSPROPERTY_AUDIO_MUTE,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
		&CGF1Topology::prophandler_onoff
		},
		{
		&KSPROPSETID_Audio,
		KSPROPERTY_AUDIO_CPU_RESOURCES,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
		&CGF1Topology::prophandler_cpu
		}
	};

DEFINE_PCAUTOMATION_TABLE_PROP (automation_mute, prop_mute);


// Volume
static PCPROPERTY_ITEM prop_volume[] =
	{
		{
		&KSPROPSETID_Audio,
		KSPROPERTY_AUDIO_VOLUMELEVEL,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
		&CGF1Topology::prophandler_volume
		},
		{
		&KSPROPSETID_Audio,
		KSPROPERTY_AUDIO_CPU_RESOURCES,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
		&CGF1Topology::prophandler_cpu
		}
	};

DEFINE_PCAUTOMATION_TABLE_PROP (automation_volume, prop_volume);


// CS4231 record multiplexor
static PCPROPERTY_ITEM prop_mux[] =
	{
		{
		&KSPROPSETID_Audio,
		KSPROPERTY_AUDIO_MUX_SOURCE,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET,// | KSPROPERTY_TYPE_BASICSUPPORT,
		&CGF1Topology::prophandler_mux_source
		},
		{
		&KSPROPSETID_Audio,
		KSPROPERTY_AUDIO_CPU_RESOURCES,
		KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
		&CGF1Topology::prophandler_cpu
		}
	};

DEFINE_PCAUTOMATION_TABLE_PROP (automation_mux, prop_mux);



/********************************************* Nodes **********************************************/

static GF1NodeName gf1_node_name[GF1NODE_MAX] =
	{
	// GF1NODE_LINEIN_MUTE
	{ L"LineInMute", NULL },

	// GF1NODE_MICIN_MUTE
	{ L"MicInMute", NULL },

	// GF1NODE_LINEOUT_MUTE
	{ L"LineOutMute", NULL },

	// GF1NODE_LINEOUT_SUM
	{ NULL, NULL },

	// GF1NODE_WAVEIN_SUM
	{ NULL, NULL }
	};

static PCNODE_DESCRIPTOR gf1_node[GF1NODE_MAX] =
	{
		// GF1NODE_LINEIN_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_LINE_MUTE
		},

		// GF1NODE_MICIN_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_MIC_MUTE
		},

		// GF1NODE_LINEOUT_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_MASTER_MUTE
		},

		// GF1NODE_LINEOUT_SUM
		{
		0,
		NULL,
		&KSNODETYPE_SUM,
		&GUSAUDFNAME_MASTER_MIX
		},

		// GF1NODE_WAVEIN_SUM
		{
		0,
		NULL,
		&KSNODETYPE_SUM,
		&KSAUDFNAME_RECORDING_SOURCE
		},
	};


static GF1NodeName cs_node_name[IWNODE_MAX] =
	{
	// CSNODE_WAVEOUT_VOLUME
	{ L"WaveOutVolL", L"WaveOutVolR" },

	// CSNODE_WAVEOUT_MUTE
	{ L"WaveOutMuteL", L"WaveOutMuteR" },

	// CSNODE_SYNTH_VOLUME
	{ L"SynthVolL", L"SynthVolR" },

	// CSNODE_SYNTH_MUTE
	{ L"SynthMuteL", L"SynthMuteR" },

	// CSNODE_CDIN_VOLUME
	{ L"CDInVolL", L"CDInVolR" },

	// CSNODE_CDIN_MUTE
	{ L"CDInMuteL", L"CDInMuteR" },

	// CSNODE_LINEIN_VOLUME
	{ L"LineInVolL", L"LineInVolR" },

	// CSNODE_LINEIN_MUTE
	{ L"LineInMuteL", L"LineInMuteR" },

	// CSNODE_MICIN_GAIN

	// CSNODE_MICIN_MUTE (handled in a specific way - mono / stereo depending on codec mode)
	{ L"MicInMuteL", L"MicInMuteR" },

	// CSNODE_MASTER_SUM
	{ NULL, NULL },

	// CSNODE_LINEOUT_MUTE (handled in a specific way - mono / stereo depending on codec mode)
	{ L"MasterMuteL", L"MasterMuteR" },

	// CSNODE_SYNTH_MUX_VOLUME
	{ L"SynthMuxVolL", L"SynthMuxVolR" },

	// CSNODE_LINEIN_MUX_VOLUME
	{ L"LineInMuxVolL", L"LineInMuxVolR" },

	// CSNODE_MICIN_MUX_VOLUME
	{ L"MicInMuxVolL", L"MicInMuxVolR" },

	// CSNODE_MIXER_MUX_VOLUME
	{ L"MixerMuxVolL", L"MixerMuxVolR" },

	// CSNODE_WAVEIN_MUX
	{ L"WaveInMux", NULL },

	// IWNODE_MICIN_VOLUME
	{ L"MicInVolL", L"MicInVolR" },

	// IWNODE_LINEOUT_VOLUME
	{ L"MasterVolL", L"MasterVolR" }
	};

static PCNODE_DESCRIPTOR cs_node[IWNODE_MAX] =
	{
		// CSNODE_WAVEOUT_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_WAVE_VOLUME
		},

		// CSNODE_WAVEOUT_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_WAVE_MUTE
		},

		// CSNODE_SYNTH_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_MIDI_VOLUME
		},

		// CSNODE_SYNTH_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_MIDI_MUTE
		},

		// CSNODE_CDIN_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_CD_VOLUME
		},

		// CSNODE_CDIN_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_CD_MUTE
		},

		// CSNODE_LINEIN_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_LINE_VOLUME
		},

		// CSNODE_LINEIN_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_LINE_MUTE
		},

		// CSNODE_MICIN_GAIN

		// CSNODE_MICIN_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_MIC_MUTE
		},

		// CSNODE_MASTER_SUM
		{
		0,
		NULL,
		&KSNODETYPE_SUM,
		&GUSAUDFNAME_MASTER_MIX
		},

		// CSNODE_LINEOUT_MUTE
		{
		0,
		&automation_mute,
		&KSNODETYPE_MUTE,
		&KSAUDFNAME_MASTER_MUTE
		},

		// CSNODE_SYNTH_MUX_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_MIDI_IN_VOLUME
		},

		// CSNODE_LINEIN_MUX_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_LINE_IN_VOLUME
		},

		// CSNODE_MICIN_MUX_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_MIC_IN_VOLUME
		},

		// CSNODE_MIXER_MUX_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&GUSAUDFNAME_MIXER_IN
		},

		// CSNODE_WAVEIN_MUX
		{
		0,
		&automation_mux,
		&KSNODETYPE_MUX,
		&KSAUDFNAME_RECORDING_SOURCE
		},

		// Interwave-only nodes follow

		// IWNODE_MICIN_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_MIC_VOLUME
		},

		// IWNODE_LINEOUT_VOLUME
		{
		0,
		&automation_volume,
		&KSNODETYPE_VOLUME,
		&KSAUDFNAME_MASTER_VOLUME
		}
	};

#if (IWNODE_MAX - CSNODE_MAX) != 2
#error "topo.h <-> topotab.cpp node definitions do not match"
#endif


/****************************************** Connections *******************************************/

static PCCONNECTION_DESCRIPTOR gf1_connection[] =
	{
	// WaveOut
	{	PCFILTER_NODE,			GF1TPIN_WAVEOUT,		GF1NODE_LINEOUT_SUM,	1				},
	{	PCFILTER_NODE,			GF1TPIN_WAVEOUT,		GF1NODE_WAVEIN_SUM,		1				},

	// Synth
	{	PCFILTER_NODE,			GF1TPIN_SYNTH,			GF1NODE_LINEOUT_SUM,	2  				},
	{	PCFILTER_NODE,			GF1TPIN_SYNTH,			GF1NODE_WAVEIN_SUM,		2  				},

	// LineIn
	{	PCFILTER_NODE,			GF1TPIN_LINEIN,			GF1NODE_LINEIN_MUTE,	1  				},
	{	GF1NODE_LINEIN_MUTE,	0,						GF1NODE_LINEOUT_SUM,	3				},
	{	GF1NODE_LINEIN_MUTE,	0,						GF1NODE_WAVEIN_SUM,		3				},

	// MicIn
	{	PCFILTER_NODE,			GF1TPIN_MICIN,			GF1NODE_MICIN_MUTE,		1  				},
	{	GF1NODE_MICIN_MUTE,		0,						GF1NODE_LINEOUT_SUM,	4  				},
	{	GF1NODE_MICIN_MUTE,		0,						GF1NODE_WAVEIN_SUM,		4  				},

	// Master output
	{	GF1NODE_LINEOUT_SUM,	0,						GF1NODE_LINEOUT_MUTE,	1				},
	{	GF1NODE_LINEOUT_MUTE,	0,						PCFILTER_NODE,			GF1TPIN_LINEOUT	},

	// WaveIn
	{	GF1NODE_WAVEIN_SUM,		0,						PCFILTER_NODE,			GF1TPIN_WAVEIN	}

	};


static PCCONNECTION_DESCRIPTOR cs_connection[] =
	{
	// Synth
	{	PCFILTER_NODE,			GF1TPIN_SYNTH,			CSNODE_SYNTH_VOLUME,	1				},
	{	CSNODE_SYNTH_VOLUME,	0,						CSNODE_SYNTH_MUTE,		1				},
	{	CSNODE_SYNTH_MUTE,		0,						CSNODE_MASTER_SUM,		1				},
	{	PCFILTER_NODE,			GF1TPIN_SYNTH,			CSNODE_SYNTH_MUX_VOLUME, 1				},
	{	CSNODE_SYNTH_MUX_VOLUME, 0,						CSNODE_WAVEIN_MUX,		CSMUXIN_SYNTH	},

	// WaveOut
	{	PCFILTER_NODE,			GF1TPIN_WAVEOUT,		CSNODE_WAVEOUT_VOLUME,	1				},
	{	CSNODE_WAVEOUT_VOLUME,	0,						CSNODE_WAVEOUT_MUTE,	1				},
	{	CSNODE_WAVEOUT_MUTE,	0,						CSNODE_MASTER_SUM,		2				},

	// CDIn
	{	PCFILTER_NODE,			CSTPIN_CDIN,			CSNODE_CDIN_VOLUME,		1				},
	{	CSNODE_CDIN_VOLUME,		0,						CSNODE_CDIN_MUTE,		1				},
	{	CSNODE_CDIN_MUTE,		0,						CSNODE_MASTER_SUM,		3				},

	// LineIn
	{	PCFILTER_NODE,			GF1TPIN_LINEIN,			CSNODE_LINEIN_VOLUME,	1  				},
	{	CSNODE_LINEIN_VOLUME,	0,						CSNODE_LINEIN_MUTE,		1				},
	{	CSNODE_LINEIN_MUTE,		0,						CSNODE_MASTER_SUM,		4				},
	{	PCFILTER_NODE,			GF1TPIN_LINEIN,			CSNODE_LINEIN_MUX_VOLUME, 1				},
	{	CSNODE_LINEIN_MUX_VOLUME, 0,					CSNODE_WAVEIN_MUX,		CSMUXIN_LINE	},

	// MicIn
	{	PCFILTER_NODE,			GF1TPIN_MICIN,			CSNODE_MICIN_MUTE,		1				},
	{	CSNODE_MICIN_MUTE,		0,						CSNODE_MASTER_SUM,		5				},
	{	CSNODE_MICIN_MUTE,		0,						CSNODE_MICIN_MUX_VOLUME, 1				},
	{	CSNODE_MICIN_MUX_VOLUME, 0,						CSNODE_WAVEIN_MUX,		CSMUXIN_MIC		},

	// Master output
	{	CSNODE_MASTER_SUM,		0,						CSNODE_LINEOUT_MUTE,	1				},
	{	CSNODE_LINEOUT_MUTE,	0,						PCFILTER_NODE,			GF1TPIN_LINEOUT	},

	// Fake mixer in (what-u-hear)
	{	PCFILTER_NODE,			CSTPIN_MIXERIN,			CSNODE_MIXER_MUX_VOLUME, 1				},
	{	CSNODE_MIXER_MUX_VOLUME, 0,						CSNODE_WAVEIN_MUX,		CSMUXIN_MIXER	},

	// WaveIn
	{	CSNODE_WAVEIN_MUX,		0,						PCFILTER_NODE,			GF1TPIN_WAVEIN	}

	};


static PCCONNECTION_DESCRIPTOR iw_connection[] =
	{
	// Synth
	{	PCFILTER_NODE,			GF1TPIN_SYNTH,			CSNODE_SYNTH_VOLUME,	1				},
	{	CSNODE_SYNTH_VOLUME,	0,						CSNODE_SYNTH_MUTE,		1				},
	{	CSNODE_SYNTH_MUTE,		0,						CSNODE_MASTER_SUM,		1				},
	{	PCFILTER_NODE,			GF1TPIN_SYNTH,			CSNODE_SYNTH_MUX_VOLUME, 1				},
	{	CSNODE_SYNTH_MUX_VOLUME, 0,						CSNODE_WAVEIN_MUX,		CSMUXIN_SYNTH	},

	// WaveOut
	{	PCFILTER_NODE,			GF1TPIN_WAVEOUT,		CSNODE_WAVEOUT_VOLUME,	1				},
	{	CSNODE_WAVEOUT_VOLUME,	0,						CSNODE_WAVEOUT_MUTE,	1				},
	{	CSNODE_WAVEOUT_MUTE,	0,						CSNODE_MASTER_SUM,		2				},

	// CDIn
	{	PCFILTER_NODE,			CSTPIN_CDIN,			CSNODE_CDIN_VOLUME,		1				},
	{	CSNODE_CDIN_VOLUME,		0,						CSNODE_CDIN_MUTE,		1				},
	{	CSNODE_CDIN_MUTE,		0,						CSNODE_MASTER_SUM,		3				},

	// LineIn
	{	PCFILTER_NODE,			GF1TPIN_LINEIN,			CSNODE_LINEIN_VOLUME,	1  				},
	{	CSNODE_LINEIN_VOLUME,	0,						CSNODE_LINEIN_MUTE,		1				},
	{	CSNODE_LINEIN_MUTE,		0,						CSNODE_MASTER_SUM,		4				},
	{	PCFILTER_NODE,			GF1TPIN_LINEIN,			CSNODE_LINEIN_MUX_VOLUME, 1				},
	{	CSNODE_LINEIN_MUX_VOLUME, 0,					CSNODE_WAVEIN_MUX,		CSMUXIN_LINE	},

	// MicIn
	{	PCFILTER_NODE,			GF1TPIN_MICIN,			IWNODE_MICIN_VOLUME,	1				},
	{	IWNODE_MICIN_VOLUME,	0,						CSNODE_MICIN_MUTE,		1				},
	{	CSNODE_MICIN_MUTE,		0,						CSNODE_MASTER_SUM,		5				},
	{	PCFILTER_NODE,			GF1TPIN_MICIN,			CSNODE_MICIN_MUX_VOLUME, 1				},
	{	CSNODE_MICIN_MUX_VOLUME, 0,						CSNODE_WAVEIN_MUX,		CSMUXIN_MIC		},

	// Master output
	{	CSNODE_MASTER_SUM,		0,						IWNODE_LINEOUT_VOLUME,	1				},
	{	IWNODE_LINEOUT_VOLUME,	0,						CSNODE_LINEOUT_MUTE,	1				},
	{	CSNODE_LINEOUT_MUTE,	0,						PCFILTER_NODE,			GF1TPIN_LINEOUT	},

	// Fake mixer in (what-u-hear)
	{	PCFILTER_NODE,			CSTPIN_MIXERIN,			CSNODE_MIXER_MUX_VOLUME, 1				},
	{	CSNODE_MIXER_MUX_VOLUME, 0,						CSNODE_WAVEIN_MUX,		CSMUXIN_MIXER	},

	// WaveIn
	{	CSNODE_WAVEIN_MUX,		0,						PCFILTER_NODE,			GF1TPIN_WAVEIN	}

	};



/************************************* Topology description ***************************************/

static PCFILTER_DESCRIPTOR gf1_descriptor =
	{
	0,
	NULL,

	// Pins
	sizeof (PCPIN_DESCRIPTOR),
	SIZEOF_ARRAY (pin) - CODEC_ONLY_PINS,
	pin,

	// Nodes
	sizeof (PCNODE_DESCRIPTOR),
	SIZEOF_ARRAY (gf1_node),
	gf1_node,

	// Connections
	SIZEOF_ARRAY (gf1_connection),
	gf1_connection,

	0,
	NULL
	};


static PCFILTER_DESCRIPTOR cs_descriptor =
	{
	0,
	NULL,

	// Pins
	sizeof (PCPIN_DESCRIPTOR),
	SIZEOF_ARRAY (pin),
	pin,

	// Nodes
	sizeof (PCNODE_DESCRIPTOR),
	SIZEOF_ARRAY (cs_node) - (IWNODE_MAX - CSNODE_MAX),
	cs_node,

	// Connections
	SIZEOF_ARRAY (cs_connection),
	cs_connection,

	0,
	NULL
	};


static PCFILTER_DESCRIPTOR iw_descriptor =
	{
	0,
	NULL,

	// Pins
	sizeof (PCPIN_DESCRIPTOR),
	SIZEOF_ARRAY (pin),
	pin,

	// Nodes
	sizeof (PCNODE_DESCRIPTOR),
	SIZEOF_ARRAY (cs_node),
	cs_node,

	// Connections
	SIZEOF_ARRAY (iw_connection),
	iw_connection,

	0,
	NULL
	};
