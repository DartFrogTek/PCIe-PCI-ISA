/*
 * gf1wavep.h
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


#ifndef _GF1WAVEP_H_
#define _GF1WAVEP_H_


#include "gf1wave.h"


/********************************** Wave cyclic stream miniport ***********************************/

class CGF1WavePlaybackStream :
	public IDmaChannel,
	public IMiniportWaveCyclicStream,
	public CUnknown

	{

private:

	// Wave miniport
	CGF1Wave *		miniport;

	// 16bit?
	BOOLEAN			fmt_16bit;

	// Stereo?
	BOOLEAN			fmt_stereo;

	// Frequency
	ULONG			frequency;

	// Fake cyclic buffer
	PVOID			buffer;					// NonPaged memory
	PBYTE			buffer_aligned;			// Dma-aligned ptr into 'buffer'
	ULONG			buffer_max_length;		// Maximum length of 'buffer_aligned'
	ULONG			buffer_cur_length;		// Current "logical" length of 'buffer_aligned'

	// Start next DMA transfer here (offset in bytes from beginning of 'buffer_aligned')
	ULONG			transfer_offset;

	// Expecting next CopyTo here
	ULONG			copyto_offset;

	/* Note:
	 Data between 'transfer_offset' and 'copyto_offset' are fresh and should
	 be sent to DRAM ASAP. Because buffer is cyclic, it's possible that
	 'copyto_offset' < 'transfer_offset'.

	 'buffer_max_length', 'buffer_cur_length', 'buffer_length' and 'transfer_offset' are
	 always dma- and dram-aligned. 'copyto_offset' may be unaligned.
	 */

	// Number of DMA requests pending
	LONG			transfers_pending;

	// Signalled when DMA transfer is completed
	KEVENT			transfer_event;

	// Notification interval in samples
	ULONG			notify_length;

	// Number of notifications (used to detect frozen waveout, but no solution to that found yet)
	ULONG			notifications;

	// Service group for notifications
	PSERVICEGROUP	service_group;

	// Stream state (KSSTATE_STOP or KSSTATE_RUN)
	KSSTATE			state;

	// Debugging stuff
	#if GF1_DBG

	// Number of volume ramp notify IRQs (should be zero)
	ULONG			notify_ramp_irqs;

	// Number of bytes copied
	ULONG			copyto_total;

	// How many times was unexpected CopyTo destination used
	ULONG			copyto_errors;

	// IRQL was >= DISPATCH_LEVEL in CopyTo
	ULONG			copyto_hilevel;

	#endif /* GF1_DBG */



    /************************************ IUnknown stuff **************************************/

public:

    DECLARE_STD_UNKNOWN();

	CGF1WavePlaybackStream
		(
		PUNKNOWN	unknown
		);

	~CGF1WavePlaybackStream();



	/*********************************** IDmaChannel stuff ************************************/

public:

	/* Allocate fake DMA cyclic buffer
	 I: buffer_size - in bytes
	 I: constraint - never used
	 N: Not used by port.
	 paged
	 */
	STDMETHODIMP AllocateBuffer
		(
		IN ULONG				buffer_size,
		IN PPHYSICAL_ADDRESS	constraint		OPTIONAL
		);


	/* Free fake DMA cyclic buffer
	 N: Not used by port.
	 paged
	 */
	STDMETHODIMP_ (void) FreeBuffer
		(
		void
		);


	/* Not used
	 nonpaged
	 */
	STDMETHODIMP_ (ULONG) TransferCount
		(
		void
		);


	/* Not used by port
	 nonpaged
	 */
	STDMETHODIMP_ (ULONG) MaximumBufferSize
		(
		void
		);


	/* Get size of allocated buffer
	 nonpaged
	 */
	STDMETHODIMP_ (ULONG) AllocatedBufferSize
		(
		void
		);


	/* Get current size of buffer
	 nonpaged
	 */
	STDMETHODIMP_ (ULONG) BufferSize
		(
		void
		);


	/* Set current size of buffer
	 nonpaged
	 */
	STDMETHODIMP_ (void) SetBufferSize
		(
		IN ULONG	size
		);


	/* Get virtual system address of allocated buffer
	 nonpaged
	 */
	STDMETHODIMP_ (PVOID) SystemAddress
		(
		void
		);


	/* Not used
	 nonpaged
	 */
	STDMETHODIMP_ (PHYSICAL_ADDRESS) PhysicalAddress
		(
		void
		);


	/* Not used
	 nonpaged
	 */
	STDMETHODIMP_ (PADAPTER_OBJECT) GetAdapterObject
		(
		void
		);


	/* Copy data from system memory to DMA buffer
	 N: extended version of IDmaBuffer::CopyTo - if 'source' is NULL, source is silence
	 nonpaged
	 */
	STDMETHODIMP_ (void) CopyTo
		(
		IN PVOID	destination,
		IN PVOID	source,
		IN ULONG	length
		);


	/* Copy data from DMA buffer to system memory
	 nonpaged
	 */
	STDMETHODIMP_ (void) CopyFrom
		(
		IN PVOID	destination,
		IN PVOID	source,
		IN ULONG	length
		);



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

	/* Timer IRQ callback
	 nonpaged
	 */
	static void timer_irq_handler
		(
		IN PVOID	self
		);


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


	/* Ramp ack callback
	 N: Silly hack, just trying to make it work, should not be needed if GUS worked as expected
	 nonpaged
	 */
	static void ramp_ack_handler
		(
		IN ULONG	voice,
		IN PVOID	self
		);


	/* Ramp action callback
	 N: Silly hack, just trying to make it work, should not be needed if GUS worked as expected
	 nonpaged
	 */
	static void ramp_action_handler
		(
		IN ULONG	voice,
		IN PVOID	self
		);


public:

	/* Initialize stream
	 I: miniport - wave miniport which owns this stream
	 O: service_group - stream service group will be stored here (NOT addrefed)
	 N: call SetFormat() to finish initialization
	 paged
	 */
	NTSTATUS init
		(
		IN CGF1Wave *			miniport,
		OUT PSERVICEGROUP *		service_group
		);
	};



/* Create new wave miniport stream
 N: DRAM DMA should be already initialized and should be freed if this fails.
	It will be freed in destructor if object is successfuly allocated.
 paged
 */
CGF1WavePlaybackStream *create_gf1_wave_playback_stream
	(
	IN POOL_TYPE	pool_type,
	IN PUNKNOWN		outer_unknown
	);


#endif /* _GF1WAVEP_H_ */
