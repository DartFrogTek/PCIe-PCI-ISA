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


#ifndef _GF1WAVE_H_
#define _GF1WAVE_H_


#include "stdunk.h"
#include "common.h"
#include "gf1cmn.h"


/* Pins
 */
enum GF1WavePin
	{
	GF1WPIN_WAVEOUT = 0,
	GF1WPIN_WAVEOUT_BRIDGE,
	GF1WPIN_WAVEIN,
	GF1WPIN_WAVEIN_BRIDGE
	};



/************************************* Wave cyclic miniport ***************************************/

class CGF1Wave :
	public IMiniportWaveCyclic,
	public CUnknown

	{
	friend class CGF1WavePlaybackStream;
	friend class CGF1WaveRecordStream;

private:

	// GF1 common
	IGF1Common *		gf1_common;

	// Port
	PPORTWAVECYCLIC		port;

	// WaveOut/WaveIn is allocated
	BOOLEAN				waveout_allocated;
	BOOLEAN				wavein_allocated;



    /************************************ IUnknown stuff **************************************/

public:

    DECLARE_STD_UNKNOWN();

	CGF1Wave
		(
		PUNKNOWN	unknown
		);

	~CGF1Wave();



    /******************************* IMiniportWaveCyclic stuff ********************************/

public:

	IMP_IMiniportWaveCyclic;
	};


/* Create new wave miniport object
 paged
 */
NTSTATUS create_gf1_wave
	(
	OUT PUNKNOWN *	unknown,
	IN POOL_TYPE	pool_type
	);


#endif /* _GF1WAVE_H_ */
