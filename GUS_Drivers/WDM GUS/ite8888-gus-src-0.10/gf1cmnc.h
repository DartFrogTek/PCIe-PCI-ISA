/*
 * gf1cmnc.h
 *
 * GF1 common class implementation
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
 * Note: Users of IGF1Common should include "gf1cmn.h", not this
 *
 */


#ifndef _GF1CMNC_H_
#define _GF1CMNC_H_


#include "stdunk.h"
#include "gf1cmn.h"
#include "dramdma.h"


//
// Direct register to port address mapping table.
//

// Structure of Direct register to port mapping table:
//
// For GF1:
//
// 0x00 - 0x0f : 16 standart 0x2xx ports.
// 0x10 - 0x1f : 16 standart 0x3xx ports, including 3xc for Max CS4231.
// 0x20 - 0x20 : 1 GF1 specific Revision port at 0x7x6.
//
// For InterWave PnP:
//
// 0x00 - 0x0f : 16 P2XR ports.
// 0x10 - 0x17 : 8	P3XR ports
// 0x18 - 0x1b : 4  Unused (NULL).
// 0x1c - 0x1f : 4  Codec ports.
// 0x20 - 0x20 : 1  Unused (NULL).

// Mappping table length.
#define DREG_TO_PORT_TABLE_LENGTH	(16 + 16 + 1)	// len(0x2xx) + len(0x3xx) + len(0x7x6)

// Check if specified direct register (port) is mapped in CGF1Common::dreg_to_port_table.
// This macro can be used only within CGF1Common class.
#define ASSERT_VALID_DREG(reg)	ASSERT (((reg) >= 0) &&							\
										((reg)<DREG_TO_PORT_TABLE_LENGTH) &&	\
										dreg_to_port_table[(reg)] != NULL )

// Get port address of specified direct register.
// This macro can be used only within CGF1Common class.
#define GET_DREG_PORT(reg)		(dreg_to_port_table[(reg)])



/**************************************** GF1Common class *****************************************/

class CGF1Common :
	public IGF1Common,
	public CUnknown

	{
private:
    // ITE8888F Bridge support
    ITE8888F_CONTEXT    bridge_context;
    BOOLEAN             using_bridge;

	/*** Windows stuff ***/

	// Physical device object
	PDEVICE_OBJECT		physical_device_object;

	// GUS device object (used as "handle" in portcls calls)
	PDEVICE_OBJECT		device_object;


	/*** GUS config ***/

	// Direct register to port address mapping table
	// Maps direct registers (GF1R_*, IWR_*, CR_*) to corresponding ports addresses.
	PUCHAR				dreg_to_port_table[DREG_TO_PORT_TABLE_LENGTH];

	// IRQs and DMAs
	ULONG				gf1_irq;	// GF1 IRQ
	ULONG				midi_irq;	// MIDI IRQ
	ULONG				dram_dma;	// DRAM / Codec recording DMA channel
	ULONG				rec_dma;	// Recording / Codec playback DMA channel

	// DRAM DMA [0] and recording DMA [1] resource descriptors
	PRESOURCELIST		dma_resources;

	// DRAM size in bytes
	ULONG				dram_size;

	// Board revision number (must be valid ASAP)
	GF1Revision			board_revision;


	/*** Interrupt stuff ***/

	// Interrupt synchronisation objects
	PINTERRUPTSYNC		interrupt_sync;
	PINTERRUPTSYNC		midi_interrupt_sync;

	// IRQ handlers
	GF1VoiceIrqFn *		wavetable_ack_handler[32];
	GF1VoiceIrqFn *		wavetable_action_handler[32];
	PVOID				wavetable_context[32];
	GF1VoiceIrqFn *		ramp_ack_handler[32];
	GF1VoiceIrqFn *		ramp_action_handler[32];
	PVOID				ramp_context[32];
	GF1TimerIrqFn *		gf1timer_ack_handler;
	GF1TimerIrqFn *		gf1timer_action_handler;
	PVOID				gf1timer_context;
	GF1CodecIrqFn *		playback_handler;
	PVOID				playback_context;
	GF1CodecIrqFn *		record_handler;
	PVOID				record_context;
	GF1CodecIrqFn *		cstimer_handler;
	PVOID				cstimer_context;


	/*** DRAM DMA ***/

	// DRAM DMA adapter handle
	PDMA_ADAPTER		ddma_adapter;

	// Is DMA enabled?
	BOOLEAN				ddma_enabled;

	// Is DMA busy?
	BOOLEAN				ddma_busy;

	// Maximum number of map registers
	ULONG				ddma_max_map_regs;

	// Source alignment mask
	ULONG				ddma_alignment;

	// DRAM DMA DPC
	KDPC				ddma_dpc;

	// Request list lock
	KSPIN_LOCK			ddma_lock;

	// Request list
	GF1DDRequest		ddma_request[GF1DDMA_REQUESTS];

	// First valid item in request list, -1 if list is empty
	LONG				ddma_first;

	// Last valid item in request list, -1 if list is empty
	LONG				ddma_last;

	// Current request info (valid if 'ddma_first' != -1)
	PVOID				ddma_reg_base;		// Map register base (NULL if not mapped)
	ULONG				ddma_offset;		// Current block starting offset (dram-aligned)
	PVOID				ddma_currentva;		// Current mdl virtual address
	ULONG				ddma_sending;		// Current block length (dram-aligned)


	/*** GUS state ***/

	// Current number of voices and mixing frequency (changed by 'set_voices()')
	ULONG				gf1_voices;
	ULONG				gf1_mixing_frequency;

	// Mixer settings (changed by 'set_mixer()')
	ULONG				mixer_settings;

	// Enable debugging / info messages? (changed in 'init()')
	BOOLEAN				info_enabled;

	// InterWave internal mode (GUS comaptible or Enhanced mode)
	// Note: enhanced mode not supported yet
	ULONG				iw_mode;

	// Codec mode
	ULONG				codec_mode;

	// Voice info
	GF1Voice			voice[32];
	KSPIN_LOCK			voice_lock;

	// DMA allocation (everything guarded by 'ddma_lock')
	ULONG				ddma_inits;				// DRAM access
	BOOLEAN				dma_rec_allocated;		// GF1 recording
	BOOLEAN				dma_cplay_allocated;	// Codec playback
	BOOLEAN				dma_crec_allocated;		// Codec recording


	/*** Debugging info ***/

	// Initial check (FIXME: single ULONG flags)
	BOOLEAN				check_wave_irq;
	BOOLEAN				check_ramp_irq;
	BOOLEAN				check_midi_irq;
	BOOLEAN				check_timer_irq;
	PBYTE				check_dma_irq;

	#if GF1_DBG

	// Number of handled IRQs
	ULONG				wave_irqs_handled;
	ULONG				ramp_irqs_handled;
	ULONG				timer_irqs_handled;
	ULONG				ddma_irqs_handled;
	ULONG				rdma_irqs_handled;
	ULONG				xmit_irqs_handled;
	ULONG				recv_irqs_handled;
	ULONG				cplay_irqs_handled;
	ULONG				crec_irqs_handled;
	ULONG				ctimer_irqs_handled;

	// Number of bytes sent to DRAM
	ULONG				ddma_transferred;

	#endif /* GF1_DBG */

	#ifdef BRIDGE_SUPPORT_ENABLED
		/*** ITE8888F Bridge support ***/
		
		// Bridge context and status
		ITE8888F_CONTEXT    bridge_context;
		BOOLEAN             using_bridge;
		
		// Bridge configuration methods
		NTSTATUS setup_bridge_support(IN PRESOURCELIST resources);
		void setup_bridge_port_mapping(void);
		BOOLEAN detect_picogus_via_bridge(void);
		NTSTATUS init_with_bridge_support(IN PRESOURCELIST resources, 
										IN PDEVICE_OBJECT physical_device, 
										IN PDEVICE_OBJECT device);
	#endif

/**************************************** IUnknown stuff ******************************************/

public:

    DECLARE_STD_UNKNOWN();

	CGF1Common
		(
		PUNKNOWN	unknown
		);

    ~CGF1Common();



/*************************************** IGF1Common stuff *****************************************/

    /************************************* Init thingies **************************************/
	// iwinit.cpp

private:
	/* Note:
	 All these functions are specific GUS PnP (Interwave). They are not synchronized
	 and must be called before master reset (reset_gf1()) and never again.
	 */

	/* Global InterWave reset
	 N: Resets InterWave to enhanced mode and enables some features it supports.
 	    Currently it does:
	    - Set IW to enhanced mode (doesn't reset IW to GUS compatible mode)
	    - Set all voices to offset modes.
	 paged
	 */
	void iw_global_reset
		(
		void
		);


	/* Reset InterWave to GUS compatible mode.
	 Same as reseting GF1.
	 The following items are reset by this process:
	� Interrupt associated with a write to the Sound Blaster 2X6 register
	  (U2X6R)
	� Interrupt associated with a write to the Sound Blaster IRQ 2XC register
	  (UI2XCR)
	� Any DMA or I/O activity to local memory (including IOCHRDY)
	� LMC DMA Control register (LDMACI)
	� DRAM/ROM Select and Auto Increment bits of the LMC Control register
	  (LMCI[1] and LMCI[0])
	� LMC FIFO Size register (LMFSI)
	� LMC DMA Interleave Control register (LDICI)
	� Synthesizer Global Mode register (SGMI)
	� Synthesizer LFO Base Address register (SLFOBI)
	� Synthesizer Voices IRQ register (SVII)
	� Synthesizer Voices IRQ Read register (SVIRI)
	� Flip-flop that drives the DMA Terminal Count IRQ bit of the IRQ Status
	  register (UISR[7])
	� Synthesizer Interrupt Enable bit and DAC Enable bit of the GUS Reset
	  register (URSTI[2:1])
	� AdLib�Sound Blaster Control register (UASBCI)
	� Interrupt associated with a write to the AdLib Data register (UADR)
	� AdLib Data register (UADR)
	� Flip-flops that drive the AdLib Status Read register (UASRR)
	� ADC Sample Control register (USCI)
	  Also, while this bit is Low, the following conditions exist:
	� It is not possible to write to the synthesizer�s register array
	� The synthesizer IRQs are all cleared
	� All synthesizer state machines are prevented from operating; they stay
	  frozen and no sound is generated.
	 paged
	 */
	void iw_reset_to_gus_mode
		(
		void
		);


	/* Set Interwave from enhanced mode back to GUS mode.
	paged
	 */
	void iw_set_gus_mode
		(
		void
		);


	/* Set InterWave to enhanced mode
	 N: InterWave's native mode with all features it supports available
	 paged
	 */
	void iw_set_enhanced_mode
		(
		void
		);


	/* Detect size of DRAM attached to InterWave.
	 N: Before calling this func you should call iw_configure_mem(),
	    otherwise this function will report only 256Kb of DRAM.
	 N: If InterWave works in GUS compatible mode, max 1MB will
        be reported although more memory (up to 16MB) can be attached to InterWave.
	 N: Will be called automatically from detect_dram_size()
	 R: DRAM size in bytes
	 paged
	 */
	ULONG iw_detect_dram_size
		(
		void
		);


	/* Set up InterWave's DRAM bank configuration.
	 N: Interwave can't detect size of DRAM attached to it by itself,
	    so we must do it.
	 paged
	 */
	void iw_configure_mem
		(
		void
		);


	/* Reset InterWave Codec
	 N: Must be in mode 3.
	 N: chains synth out to aux1 in, enables lineout and some stuff
	 FIXME: part of this should be moved to topology
	 paged
	 */
	void iw_reset_codec
		(
		void
		);



	/************************************ InterWave voices stuff ******************************/
	// iwcmn.cpp

	/* Switch voice to pan mode.
	 N: Voice must be selected.  InterWave doesn't need to be in enhanced mode.
	 paged
	 */
	void iw_voice_set_pan_mode
		(
		void
		);


	/* Switch voice to offset mode.
	 N: Voice must be selected and InterWave must be in enhanced mode.
	 paged
	 */
	void iw_voice_set_offset_mode
		(
		void\
		);


	/* Set voice stereo offsets gradually.
	 The synthesizer increments or decrements the current values in voice's left and right
	 stereo offsets (SROI and SLOI) by 1 each sample until until they reach the values
	 contained in 'left' and 'right' arguments (SROFI) and (SLOFI).
	 I: left - left offset value (0-4096)
	    right - right offset value (0-4096)
	 N: Voice must be selected, InterWave must be in enhanced mode and voice
		must be in offset mode.
		Offsets must be from 0-4095 (12 bits) range.
	 nonpaged
	 */
	void iw_voice_set_offsets_gradually
		(
		IN WORD	left,
		IN WORD	right
		);


	/* Set voice stereo offsets immediately.
	 I: left - left offset value (0-4096)
	    right - right offset value (0-4096)
	 N: Voice must be selected, InterWave must be in enhanced mode and voice
		must be in offset mode.
		Offsets must be from 0-4095 (12 bits) range.
	 nonpaged
	 */
	void iw_voice_set_offsets_immediately
		(
		IN WORD	left,
		IN WORD	right
		);



	/************************************ Codec stuff *****************************************/
	// codec.cpp

private:

	/* Close codec
	 paged, PASSIVE_LEVEL
	 */
	void codec_close
		(
		void
		);


	/* Initialize Codec
	 N: Board revision must be valid.
	 paged, PASSIVE_LEVEL
	 */
	void codec_init
		(
		void
		);


public:

	/* Get codec mode
	 */
	STDMETHODIMP_ (ULONG) get_codec_mode
		(
		void
		);


    /************************************* Init thingies **************************************/
	// gf1init.cpp

private:

	/* Clear Direct register to port address mapping table.
	 */
	void clear_dreg_to_port_table
		(
		void
		);


	/* Check port settings by writing/reading DRAM
	 R: TRUE on success, FALSE on error
	 N: Does not care about "board_revision".
	 N: does master reset as well
	 FIXME: I think there are some Interwave cards without RAM, but I'm not sure
	 paged
	 */
	BOOLEAN check_port
		(
		void
		);


	/* Detect size of GUS DRAM
	 R: DRAM size in bytes (256kB, 512kB, 768kB or 1MB)
	 N: Should not fail if 'check_port' succeeded.
	 N: Calls iw_detect_dram_size() if board revision is PNP.
	 paged
	 */
	ULONG detect_dram_size
		(
		void
		);


	/* Program GF1 IRQ and DMA latches
	 I: mix - MIX_CTRL register contents
	 I: irql - value to write to IRQ control latches
	 I: dmal - value to write to DMA control latches
	 N: GF1-only (do not call this if board revision is PNP)
	 N: this runs at raised IRQL
	 nonpaged
	 */
	void program_latches
		(
		BYTE	mix,
		BYTE	irql,
		BYTE	dmal
		);


	/* Find out GF1 board revision
	 N: silly PnP detection - GF1R_MAX_CTRL must not be valid (table[0x16] == NULL)
	 paged
	 */
	void find_board_revision
		(
		void
		);


	/* Master reset (any board)
	 */
	BOOLEAN reset_gus
		(
		void
		);


	/* Perform initial checks
	 N: in cooperation with isrs, this should check:
		- wavetable irq (rollover feature)
		- volume ramp irq
		- midi transmit irq
		- timer irq
		- dma tc irq
	 nonpaged synchronized
	 */
	static NTSTATUS init_check
		(
		IN PINTERRUPTSYNC	isync,
		IN PVOID			self
		);


public:

	/* Initialize GF1
	 */
	STDMETHODIMP init
		(
		IN PRESOURCELIST	resources,
		IN PDEVICE_OBJECT	physical_device,
		IN PDEVICE_OBJECT	device
		);



	/************************************ Variable access *************************************/
	// gf1cmn.cpp

private:

	/* Write to mixer
	 synchronized
	 */
	static NTSTATUS write_mixer
		(
		IN PINTERRUPTSYNC	isync,
		IN PVOID			self
		);

	/* Read indirect codec register
	 */
	static NTSTATUS read_codec_reg
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


	/* Write indirect codec register
	 */
	static NTSTATUS write_codec_reg
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


public:

	/* Get mixer settings
	 */
	STDMETHODIMP_ (BYTE) get_mixer
		(
		void
		);


	/* Set mixer settings
	 */
	STDMETHODIMP_ (void) set_mixer
		(
		IN BYTE		mix
		);


	/* Get indirect codec register
	 */
	STDMETHODIMP_ (BYTE) get_codec_reg
		(
		IN BYTE		reg
		);


	/* Set indirect codec register
	 */
	STDMETHODIMP_ (void) set_codec_reg
		(
		IN BYTE		reg,
		IN BYTE		mask,
		IN BYTE		value
		);


	/* Get board revision
	 */
	STDMETHODIMP_ (GF1Revision) get_revision
		(
		void
		);


	/* Get mixing frequency
	 */
	STDMETHODIMP_ (ULONG) get_frequency
		(
		void
		);


	/* Get DMA resource list
	 */
	STDMETHODIMP_ (PRESOURCELIST) get_dma_resources
		(
		void
		);



	/********************************* Device registry stuff **********************************/
	// registry.cpp

private:

	/* Delete registry subkey
	 I: subkey - key to delete
	 paged
	 */
	NTSTATUS delete_registry_key
		(
		IN PCWSTR	subkey
		);


	/* Read something from registry
	 O: info - info about value is returned here, free it with ExFreePool()
	 I: subkey - optional subkey name
	 I: value_name - name of value key
	 I: value_length - max value length to allocate
	 paged
	 */
	NTSTATUS read_registry
		(
		OUT PKEY_VALUE_PARTIAL_INFORMATION *	info,
		IN PCWSTR								subkey	OPTIONAL,
		IN PCWSTR								value_name,
		IN ULONG								value_length
		);


	/* Write something to registry
	 I: subkey - optional subkey name
	 I: value_name - value key name
	 I: value_type, value_data, value_length - value data
	 I: volat - volatile subkey?
	 paged
	 */
	NTSTATUS write_registry
		(
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	value_name,
		IN ULONG	value_type,
		IN PVOID	value_data,
		IN ULONG	value_length,
		IN BOOLEAN	volat
		);


public:

	/* Compare string in registry
 	*/
	STDMETHODIMP_ (LONG) registry_stricmp
		(
		IN PCWSTR		subkey	OPTIONAL,
		IN PCWSTR		valkey,
		IN PCWSTR		string2 OPTIONAL
		);


	/* Read integer from registry
	 */
	STDMETHODIMP read_registry_int
		(
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	valkey,
		IN BOOLEAN	volat,
		OUT PULONG	value
		);


	/* Write string to registry
	 */
	STDMETHODIMP write_registry_str
		(
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	valkey,
		IN PCWSTR	value,
		IN BOOLEAN	volat
		);


	/* Write integer to registry
	 */
	STDMETHODIMP write_registry_int
		(
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	valkey,
		IN ULONG	value,
		IN BOOLEAN	volat
		);


	/* Write info to registry
	 */
	STDMETHODIMP_ (void) reg_putstr
		(
		IN ULONG	type,
		IN PCWSTR	name,
		IN PCWSTR	value
		);

	STDMETHODIMP_ (void) reg_putint
		(
		IN ULONG	type,
		IN PCWSTR	name,
		IN ULONG	value
		);

	/* Read info from registry
	 */
	STDMETHODIMP reg_getint
		(
		IN ULONG	type,
		IN PCWSTR	name,
		OUT PULONG	value
		);


	/* Write debugging info to registry
	 */
	STDMETHODIMP_ (void) debug_to_registry
		(
		IN PCWSTR	from
		);



	/************************************ Register stuff **************************************/
	// gf1cmn.cpp

public:

	/* Write to direct register / port
	 */
	STDMETHODIMP_ (void) dwrite
		(
		IN DWORD	reg,
		IN BYTE		val
		);


	/* Read direct register / port
	 */
	STDMETHODIMP_ (BYTE) dread
		(
		IN DWORD	reg
		);


	/* Write to indirect register (8bit)
	 */
	STDMETHODIMP_ (void) iwrite8
		(
		IN BYTE		reg,
		IN BYTE		val
		);


	/* Write to indirect register (16bit)
	 */
	STDMETHODIMP_ (void) iwrite16
		(
		IN BYTE		reg,
		IN WORD		val
		);


	/* Read indirect register (8bit)
	 */
	STDMETHODIMP_ (BYTE) iread8
		(
		IN BYTE		reg
		);


	/* Read indirect register (16bit)
	 */
	STDMETHODIMP_ (WORD) iread16
		(
		IN BYTE		reg
		);


	/* Write byte to DRAM
	 */
	STDMETHODIMP_ (void) poke
		(
		IN DWORD	addr,
		IN BYTE		val
		);


	/* Read byte from DRAM
	 */
	STDMETHODIMP_ (BYTE) peek
		(
		IN DWORD	addr
		);

	/* Write to indirect codec register (8bit)
	 */
	STDMETHODIMP_ (void) ciwrite8
		(
		IN BYTE		reg,
		IN BYTE		val
		);
	/* Read indirect codec register (8bit)
	 */
	STDMETHODIMP_ (BYTE) ciread8
		(
		IN BYTE		reg
		);


	/********************************** Voice manipulation ************************************/
	// gf1voice.cpp

private:

	/* set_voices() synchronized stuff
	 */
	static NTSTATUS set_voices_synchronized
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


public:

	/* Change playback frequency of current voice
	 */
	STDMETHODIMP_ (void) set_voice_frequency
		(
		IN ULONG	freq
		);


	/* Change number of active voices
	 */
	STDMETHODIMP_ (void) set_voices
		(
		IN ULONG	voices
		);


	/* Select voice.
	 I: voice - voice number (0-31)
	 N: On InterWave selecting voice will disable Auto-increment mode.
	 nonpaged
	 */
	STDMETHODIMP_ (void) select_voice
		(
		IN BYTE voice
		);


	/* Set voice panning
	 I: pan - must be in 0-255 range (0==left, 127==middle, 255==right)
	 N: Voice must be selected.
	    Assumes that if InterWave is present and is in enhanced mode then
	    all voices are in offset mode.
	 nonpaged
	 */
	STDMETHODIMP_ (void) voice_set_panning
		(
		IN BYTE	pan
		);


	/* Lock voice info
	 */
	STDMETHODIMP_ (GF1Voice *) lock_voices
		(
		OUT PKIRQL	old_irql
		);


	/* Unlock voice info
	 */
	STDMETHODIMP_ (void) unlock_voices
		(
		IN KIRQL	old_irql
		);



	/********************************** Interrupt handling ************************************/
	// gf1int.cpp

private:

	/* Generic interrupt service routine
	 I: isync - associated interrupt synchronization object
	 I: self - ptr to CGF1Common object
	 nonpaged
	 */
	static NTSTATUS isr
		(
		IN PINTERRUPTSYNC	isync,
		IN PVOID			self
		);


	/* MIDI interrupt service routine
	 I: isync - associated interrupt synchronization object
	 I: self - ptr to CGF1Common object
	 nonpaged
	 */
	static NTSTATUS midi_isr
		(
		IN PINTERRUPTSYNC	isync,
		IN PVOID			self
		);


	/* set_wavetable_handler() synchronized stuff
	 nonpaged
	 */
	static NTSTATUS swh_synchronized
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


	/* set_ramp_handler() synchronized stuff
	 nonpaged
	 */
	static NTSTATUS srh_synchronized
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


	/* set_gf1timer_handler() synchronized stuff
	 nonpaged
	 */
	static NTSTATUS sth_synchronized
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


	/* Set codec IRQ handler synchronized stuff
	 nonpaged
	 */
	static NTSTATUS sch_synchronized
		(
		IN PINTERRUPTSYNC,
		IN PVOID			context
		);


public:

	/* Call routine synchronized with interrupts
	 */
	STDMETHODIMP_ (void) call_synchronized
		(
		IN PINTERRUPTSYNCROUTINE	routine,
		IN PVOID					context
		);


	/* Install wavetable IRQ handler
	 */
	STDMETHODIMP_ (BOOLEAN) set_wavetable_handler
		(
		IN ULONG			voice,
		IN GF1VoiceIrqFn *	ack_handler,
		IN GF1VoiceIrqFn *	action_handler,
		IN PVOID			context
		);


	/* Install volume ramp IRQ handler
	 */
	STDMETHODIMP_ (BOOLEAN) set_ramp_handler
		(
		IN ULONG			voice,
		IN GF1VoiceIrqFn *	ack_handler,
		IN GF1VoiceIrqFn *	action_handler,
		IN PVOID			context
		);


	/* Install AdLib/GF1 timer IRQ handler
	 */
	STDMETHODIMP_ (BOOLEAN) set_gf1timer_handler
		(
		IN GF1TimerIrqFn *	ack_handler,
		IN GF1TimerIrqFn *	action_handler,
		IN PVOID			context
		);


	/* Install CS4231 playback IRQ handler
	 */
	STDMETHODIMP_ (BOOLEAN) set_playback_handler
		(
		IN GF1CodecIrqFn *	handler,
		IN PVOID			context
		);


	/* Install CS4231 record IRQ handler
	 */
	STDMETHODIMP_ (BOOLEAN) set_record_handler
		(
		IN GF1CodecIrqFn *	handler,
		IN PVOID			context
		);


	/* Install CS4231 timer IRQ handler
	 */
	STDMETHODIMP_ (BOOLEAN) set_cstimer_handler
		(
		IN GF1CodecIrqFn *	handler,
		IN PVOID			context
		);



	/****************************************** DMA *******************************************/

	// gf1cmn.cpp

public:

	/* Grab DMA channel
	 */
	STDMETHODIMP_ (BOOLEAN) get_dma
		(
		IN GF1DmaChannel	channel
		);


	/* Free DMA channel
	 */
	STDMETHODIMP_ (void) put_dma
		(
		IN GF1DmaChannel	channel
		);


	// dramdma.cpp

private:

	/* Initialize DRAM DMA stuff
	 N: "reference" counting is not done here - allocate ddma via 'get_dma()'
	 nonpaged
	 */
	NTSTATUS init_dram_dma
		(
		void
		);


	/* Free DRAM DMA structures
	 N: Should be called only by 'put_dma()'
	 nonpaged
	 */
	void close_dram_dma
		(
		void
		);


	/* DMA TC DPC
	 N: starts new transfer if this wasn't last request
	 nonpaged
	 */
	static void ddma_tc_dpc
		(
		IN PKDPC	dpc,
		IN PVOID	self,
		IN PVOID	context1,
		IN PVOID	context2
		);


	/* Adapter control callback - start DMA transfer
	 I: register_base - DMA transfer "context"
	 I: self - ptr to CGF1Common
	 nonpaged
	 */
	static IO_ALLOCATION_ACTION ddma_adapter_control
		(
		IN PDEVICE_OBJECT,
		IN PIRP,
		IN PVOID            register_base,
		IN PVOID			self
		);


	/* Send next part of current request
	 N: first request must be valid, DRAM DMA must be stopped
	 nonpaged
	 */
	void ddma_send
		(
		void
		);


	/* Schedule DRAM download
	 I: dram_address - GF1 DRAM physical address
	 I: src_address - source virtual address
	 I: length - length in bytes, DRAM-aligned
	 I: flags - transfer flags (GF1DMA_DATA16, GF1DMA_UNSIGNED)
	 I: counter - decrement this when transfer is done
	 I: event - signal this after decrementing 'counter'
	 I: type - source memory type (GF1DDMA_TYPE_*)
	 nonpaged
	 */
	BOOLEAN ddma_download
		(
		IN ULONG		dram_address,
		IN PVOID		src_address,
		IN ULONG		length,
		IN ULONG		flags,
		IN PLONG		counter			OPTIONAL,
		IN PRKEVENT		event			OPTIONAL,
		IN ULONG		type
		);


public:

	/* Get DRAM DMA alignment
	 */
	STDMETHODIMP_ (ULONG) ddma_get_alignment
		(
		void
		);


	/* Schedule DRAM download from nonpaged memory
	 */
	STDMETHODIMP_ (BOOLEAN) ddma_download_nonpaged
		(
		IN ULONG		dram_address,
		IN PVOID		src_address,
		IN ULONG		length,
		IN ULONG		flags,
		IN PLONG		counter			OPTIONAL,
		IN PRKEVENT		event			OPTIONAL
		);



	/***************************************** Hacks ******************************************/

private:

	// All hacks are official now :-)



	/**************************************** Friends *****************************************/

//friends:

	friend NTSTATUS create_gf1common
		(
		OUT PUNKNOWN *,
		IN POOL_TYPE
		);
	};


#endif /* _GF1CMNC_H_ */
