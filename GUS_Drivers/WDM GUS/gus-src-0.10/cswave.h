/*
 * cswave.h
 *
 * CS4231 compatible (GUS MAX & InterWave) Wave miniport
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


#ifndef _CSWAVE_H_
#define _CSWAVE_H_


#include "stdunk.h"
#include "common.h"
#include "gf1cmn.h"


/* Pins
 */
enum CodecWavePin
	{
	CODECWPIN_WAVEOUT = 0,
	CODECWPIN_WAVEOUT_BRIDGE,
	CODECWPIN_WAVEIN,
	CODECWPIN_WAVEIN_BRIDGE
	};



/************************************* Wave cyclic miniport ***************************************/

class CCSWave :
    public IMiniportWaveCyclic,
    public CUnknown

    {
    friend class CCSWaveStream;

private:

    // GF1 common
    IGF1Common *        gf1_common;

    // Port
    PPORTWAVECYCLIC     port;

	// Rendering in use
	BOOLEAN				waveout_allocated;

	// Capture in use
	BOOLEAN				wavein_allocated;

	// Playback / recording frequency (valid only if in mode 2)
    ULONG               frequency;

	// Channel allocation / frequency change mutex (FIXME: use FAST_MUTEX instead?)
	KMUTEX				mutex;



    /************************************ IUnknown stuff **************************************/

public:

    DECLARE_STD_UNKNOWN();

    CCSWave
        (
        PUNKNOWN    unknown
        );

    ~CCSWave();



	/******************************* IMiniportWaveCyclic stuff ********************************/

public:

	IMP_IMiniportWaveCyclic;



	/************************************* Our own stuff **************************************/

private:

	/* Write value to data format register.
	 nonpaged, synchronized
	 */
	static NTSTATUS codec_write_data_format
		(
		PINTERRUPTSYNC,
		PVOID			context
		);


	/* Set codec playback frequency
	 I: freq - playback frequency in Hz
	 N: sets recording frequency too if not in mode 3
	 paged, PASSIVE_LEVEL
	 */
	void codec_set_playback_frequency
		(
		ULONG	freq
		);


	/* Set codec recording frequency
	 I: freq - recording frequency in Hz
	 N: sets playback frequency too if not in mode 3
	 paged, PASSIVE_LEVEL
	 */
	void codec_set_record_frequency
		(
		ULONG	freq
		);


	/* Set codec playback data format
	 I: compress - compression type
	 paged, PASSIVE_LEVEL
	 */
	void codec_set_playback_data_format
		(
		BOOLEAN				stereo,
		BOOLEAN				bit16,
		CODECCompressType	compress
		);


	/* Set codec record data format
	 I: compress - compression type
	 paged, PASSIVE_LEVEL
	 */
	void codec_set_record_data_format
		(
		BOOLEAN				stereo,
		BOOLEAN				bit16,
		CODECCompressType	compress
		);
	};


/* Create new CS4231 compatible wave object
 paged
 */
NTSTATUS create_codec_wave
	(
	OUT PUNKNOWN *	unknown,
	IN POOL_TYPE	pool_type
	);


#endif /* _CSWAVE_H_ */
