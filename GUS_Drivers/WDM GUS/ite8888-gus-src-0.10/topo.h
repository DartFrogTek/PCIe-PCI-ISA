/*
 * topo.h
 *
 * Topology miniport
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


#ifndef _TOPO_H_
#define _TOPO_H_


#include "stdunk.h"
#include "common.h"
#include "gf1cmn.h"


/* "Exported" pins
 Note: These are valid for both "classic" and CS4231
 */
enum GF1TopoPin
	{
	// Sources
	GF1TPIN_WAVEOUT = 0,
	GF1TPIN_SYNTH,
	GF1TPIN_LINEIN,
	GF1TPIN_MICIN,

	// Destinations
	GF1TPIN_LINEOUT,
	GF1TPIN_WAVEIN,

	// Max+ only
	CSTPIN_CDIN,
	CSTPIN_MIXERIN
	};

// # of Max+ specific pins (CDIn and MixerIn)
#define CODEC_ONLY_PINS		2


/* GUS classic nodes
 */
enum GF1Node
	{
	GF1NODE_LINEIN_MUTE = 0,
	GF1NODE_MICIN_MUTE,
	GF1NODE_LINEOUT_MUTE,
	GF1NODE_LINEOUT_SUM,
	GF1NODE_WAVEIN_SUM,

	GF1NODE_MAX
	};


/* CS4231 mixer nodes
 */
enum CSNode
	{
	// WaveOut -> Sum
	CSNODE_WAVEOUT_VOLUME = 0,
	CSNODE_WAVEOUT_MUTE,

	// Aux1 -> Sum
	CSNODE_SYNTH_VOLUME,
	CSNODE_SYNTH_MUTE,

	// CDIn -> Sum
	CSNODE_CDIN_VOLUME,
	CSNODE_CDIN_MUTE,

	// LineIn -> Sum
	CSNODE_LINEIN_VOLUME,
	CSNODE_LINEIN_MUTE,

	// MicIn -> Mux gain (FIXME: Max-only)
	//CSNODE_MICIN_GAIN,

	CSNODE_MICIN_MUTE,

	// Master mix
	CSNODE_MASTER_SUM,

	// Master mute
	CSNODE_LINEOUT_MUTE,

	// Fake MuxIn volumes
	CSNODE_SYNTH_MUX_VOLUME,
	CSNODE_LINEIN_MUX_VOLUME,
	CSNODE_MICIN_MUX_VOLUME,
	CSNODE_MIXER_MUX_VOLUME,

	// WaveIn Mux
	CSNODE_WAVEIN_MUX,

	// Interwave stuff follows...

	// MicIn -> Sum
	IWNODE_MICIN_VOLUME,
	//CSNODE_MICIN_MUTE - reused

	// LineOut
	IWNODE_LINEOUT_VOLUME,
	//CSNODE_LINEOUT_MUTE - reused

	IWNODE_MAX
	};

#define CSNODE_MAX	(IWNODE_MAX - 2)


/* WaveIn Mux source
 */
enum CSMuxIn
	{
	//CSMUXOUT = 0
	CSMUXIN_MIXER = 1,
	CSMUXIN_SYNTH = 2,
	CSMUXIN_LINE = 3,
	CSMUXIN_MIC = 4
	};


/* Per-node info
 Note: CS4231 mixer is "owned" exclusively by the topology miniport, so nothing should be changed
 by anyonce once 'valid' flag is set to TRUE. The same applies for "mixer" bits in GF1
 mixer control register.
 */
struct GF1NodeInfo
	{
	union
		{
		// Mute controls
		struct
			{
			// Left / right mute
			BOOLEAN 		lmute;
			BOOLEAN			rmute;
			} mute;

		// Volume controls
		struct
			{
			// Left / right volumes in dB
			LONG			lvol;
			LONG			rvol;
			} volume;

		// WaveIn mux
		struct
			{
			// Mux source
			CSMuxIn		source;
			} mux;
		};

	// Is info valid? (only 'left_valid' applies for mono control)
	BOOLEAN		left_valid;
	BOOLEAN		right_valid;
	};


/* Node names
 */
struct GF1NodeName
	{
	// Left control (may be NULL)
	PCWSTR	left;

	// Right control (may be NULL)
	PCWSTR	right;
	};



/*************************************** Topology miniport ****************************************/

class CGF1Topology :
	public IMiniportTopology,
	public CUnknown

	{
private:

	// GF1 common
	IGF1Common *		gf1_common;

	// Topology port
	PPORTTOPOLOGY		port;

	// Node info (paged array)
	GF1NodeInfo *		node_info;

	// Number of nodes
	ULONG				nodes;



    /************************************ IUnknown stuff **************************************/

public:

    DECLARE_STD_UNKNOWN();

	CGF1Topology
		(
		PUNKNOWN	unknown
		);

	~CGF1Topology();



    /*********************************** IMiniport stuff *************************************/

public:

	/* Get topology description
	 paged
	 */
	STDMETHODIMP GetDescription
		(
		OUT PPCFILTER_DESCRIPTOR *		descriptor
		);


	/* Get data range intersection
	 paged
	 */
    STDMETHODIMP DataRangeIntersection
		(
		IN ULONG			pin,
		IN PKSDATARANGE		data_range,
		IN PKSDATARANGE		matching_range,
		IN ULONG			buffer_length,
		OUT PVOID			res_format			OPTIONAL,
		OUT PULONG			res_format_length
		)

		{
		// Let portcls do the work (taky bylo na case)
		return STATUS_NOT_IMPLEMENTED;
		}



    /******************************** IMiniportTopology stuff *********************************/

public:

	/* Initialize miniport
	 paged
	 */
	STDMETHODIMP Init
		(
		IN PUNKNOWN			common_unknown,
		IN PRESOURCELIST	resources,
		IN PPORTTOPOLOGY	port
		);



    /*********************************** Property handlers ************************************/

public:

	/* On/Off controls (mute)
	 paged
	 */
	static NTSTATUS prophandler_onoff
		(
		IN PPCPROPERTY_REQUEST		request
		);


	/* CPU resources
	 paged
	 */
	static NTSTATUS prophandler_cpu
		(
		IN PPCPROPERTY_REQUEST		request
		);


	/* Attenuation / gain
	 paged
	 */
	static NTSTATUS prophandler_volume
		(
		IN PPCPROPERTY_REQUEST		request
		);


	/* CS4231 record multiplexer - select recording source
	 paged
	 */
	static NTSTATUS prophandler_mux_source
		(
		IN PPCPROPERTY_REQUEST		request
		);



	/************************************* Our own stuff **************************************/

private:

	/* Read mixer settings from registry
	 N: uses defaults when not found
	 N: and applies these settings
	 paged
	 */
	void read_from_registry
		(
		void
		);


	/* Write mixer settings to registry
	 paged
	 */
	void write_to_registry
		(
		void
		);
	};


/* Create new topology miniport object
 paged
 */
NTSTATUS create_gf1_topology
	(
	OUT PUNKNOWN *	unknown,
	IN POOL_TYPE	pool_type
	);


#endif /* _TOPO_H_ */
