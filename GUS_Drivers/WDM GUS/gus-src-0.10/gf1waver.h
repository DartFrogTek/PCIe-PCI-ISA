/*
 * gf1waver.h
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


#ifndef _GF1WAVER_H_
#define _GF1WAVER_H_


#include "gf1wave.h"


/********************************** Wave cyclic stream miniport ***********************************/

class CGF1WaveRecordStream :
	public IMiniportWaveCyclicStream,
	public CUnknown

	{

private:

	// Wave miniport
	CGF1Wave *		miniport;

	// Stereo? (always 8bit)
	BOOLEAN			fmt_stereo;

	// DMA 16bit
	BOOLEAN			dma_16bit;

	// Frequency
	ULONG			frequency;

	// Cyclic DMA buffer (valid iff channel has been allocated by 'CGF1Common::get_dma()'
	PDMACHANNELSLAVE dma_channel;

	// Notification interval in ms
	ULONG			notify_length;

	// Number of notifications (used to detect frozen waveout, but no solution to that found yet)
	ULONG			notifications;

	// Service group for notifications
	PSERVICEGROUP	service_group;

	// Stream state (KSSTATE_STOP or KSSTATE_RUN)
	KSSTATE			state;

	// Debugging stuff
	#if GF1_DBG

	#endif /* GF1_DBG */



    /************************************ IUnknown stuff **************************************/

public:

    DECLARE_STD_UNKNOWN();

	CGF1WaveRecordStream
		(
		PUNKNOWN	unknown
		);

	~CGF1WaveRecordStream();



	/****************************** IMiniPortWaveCyclic stream ********************************/

private:

	/* Stop playback
	 nonpaged, synchronized
	 */
	static NTSTATUS ss_stop
		(
		PINTERRUPTSYNC,
		PVOID			self
		);


	/* Resume playback
	 nonpaged, synchronized
	 */
	static NTSTATUS ss_run
		(
		PINTERRUPTSYNC,
		PVOID			self
		);


public:

	IMP_IMiniportWaveCyclicStream;



	/************************************* Our own stuff **************************************/

private:

	/* Wavetable acknowledge callback
	 nonpaged
	 */
	static void wave_ack_handler
		(
		IN ULONG	voice,
		IN PVOID	self
		);


	/* Wavetable action callback
	 nonpaged
	 */
	static void wave_action_handler
		(
		IN ULONG	voice,
		IN PVOID	self
		);


	/* Notification voice callback
	 nonpaged
	 */
	static void notify_callback
		(
		IN ULONG			voice,
		IN GF1VoiceReason	reason,
		IN GF1Voice *		info,
		IN PVOID			context
		);


public:

	/* Initialize stream
	 I: miniport - wave miniport which owns this stream
	 I: dma_resources - dma[1] should be recording dma
	 O: service_group - stream service group will be stored here (NOT addrefed)
	 O: dma_channel - dma channel ptr will be stored here (NOT addrefed)
	 N: call SetFormat() to finish initialization
	 paged
	 */
	NTSTATUS init
		(
		IN CGF1Wave *			miniport,
		IN PRESOURCELIST		dma_resources,
		OUT PSERVICEGROUP *		service_group,
		OUT PDMACHANNEL *		dma_channel
		);
	};



/* Create new wave miniport stream
 paged
 */
CGF1WaveRecordStream *create_gf1_wave_record_stream
	(
	IN POOL_TYPE	pool_type,
	IN PUNKNOWN		outer_unknown
	);


#endif /* _GF1WAVER_H_ */
