/*
 * cswaves.h
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


#ifndef _CSWAVES_H_
#define _CSWAVES_H_


#include "cswave.h"



/********************************** Wave cyclic stream miniport ***********************************/

class CCSWaveStream :
    public IMiniportWaveCyclicStream,
    public CUnknown

    {
private:

    // Wave miniport
	CCSWave *			miniport;

	// Playback or recording stream?
	BOOLEAN				capture;

	// 16bit?
	BOOLEAN				fmt_16bit;

	// Stereo?
	BOOLEAN				fmt_stereo;

	// Frequency (must be the same as 'miniport->frequency' unless in mode 3)
	ULONG				frequency;

	// Notification interval in samples
	ULONG				notify_length;

	// Cyclic DMA buffer (valid iff channel has been allocated by 'CGF1Common::get_dma()'
	PDMACHANNELSLAVE	dma_channel;

	// Service group
	PSERVICEGROUP		service_group;

	// State (STOP or RUN)
	KSSTATE				state;

	#if GF1_DBG
	ULONG				notifications;
	#endif /* GF1_DBG */


    /************************************ IUnknown stuff **************************************/

public:

    DECLARE_STD_UNKNOWN();

	CCSWaveStream
		(
		PUNKNOWN	unknown
		);

	~CCSWaveStream();



	/****************************** IMiniPortWaveCyclic stream ********************************/

public:

	IMP_IMiniportWaveCyclicStream;



	/************************************* Our own stuff **************************************/

private:

	/* Stop stream
	 nonpaged, synchronized
	 */
	static NTSTATUS ss_stop
		(
		PINTERRUPTSYNC,
		PVOID			context
		);


	/* Run stream
	 nonpaged, synchronized
	 */
	static NTSTATUS ss_run
		(
		PINTERRUPTSYNC,
		PVOID			context
		);


	/* Codec IRQ callback
	 nonpaged
	 */
	static void codec_irq_callback
		(
		IN PVOID	self
		);


public:

	/* Initialize stream
	 I: miniport - stream owner
	 I: capture - is this capture stream
	 I: resources - DMA resource list ([0] = recording, [1] = playback)
	 O: service_group - stream service group will be stored here (NOT addrefed)
	 O: dma_channel - dma channel ptr will be stored here (NOT addrefed)
	 N: call SetFormat() to finish initialization, it will actually validate format and
		allocate wavein/waveout
	 paged
	 */
	NTSTATUS init
		(
		IN CCSWave *			miniport,
		IN BOOLEAN				capture,
		IN PRESOURCELIST		resources,
		OUT PSERVICEGROUP *		service_group,
		OUT PDMACHANNEL *		dma_channel
		);
    };



/* Create new wave miniport stream
 paged
 */
CCSWaveStream *create_codec_wave_stream
    (
    IN POOL_TYPE    pool_type,
    IN PUNKNOWN     outer_unknown
    );


#endif /* _CSWAVES_H_ */
