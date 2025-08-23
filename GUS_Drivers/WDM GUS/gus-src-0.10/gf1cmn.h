/*
 * gf1cmn.h
 *
 * GF1 common class
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


#ifndef _GF1CMN_H_
#define _GF1CMN_H_


#include "common.h"
#include "gf1regs.h"
#include "csregs.h"
#include "iwregs.h"



/************************************** Other GF1 constants ***************************************/

// Timer frequencies
#define GF1TIMER1_FREQ		12500
#define GF1TIMER2_FREQ		3125


// Sampling rate (freq in Hz -> rate) [617400 = 7^3 * 5^2 * 3^2 * 2^3]
#define GF1SMP_RATE(f)		(617400 / (f) - 2)


// Memory stuff
#define GF1MEM_ALIGNMENT	(32-1)	// DRAM DMA transfer alignment
#define GF1MEM_ALIGN_SHIFT	5
#define GF1MEM_PAGE_SHIFT	18		// DRAM page size (16bit samples can't cross boundaries)
#define GF1MEM_PAGE_SIZE	(1 << GF1MEM_PAGE_SHIFT)

// Reserved memory (32 zero-bytes for stopped voices + 32kB for fake cyclic DMA)
// FIXME: do not reserve fake DMA is revision >= MAX
#define GF1MEM_RESERVED_SILENCE		32
#define GF1MEM_RESERVED_DMA			0x4000
//#define GF1MEM_RESERVED_DMA			0x10000
#define GF1MEM_RESERVED				(GF1MEM_RESERVED_SILENCE + GF1MEM_RESERVED_DMA)



/***************************************** Common macros ******************************************/

// Delay between writes to self-modifying registers
#define GF1_DELAY(i)		KeStallExecutionProcessor (10 * i)


// Writes to self-modifying registers should be delayed on GUS (not on PnP)
#define GF1_SELFMOD(self)	\
	if ((self)->get_revision() < GF1REVISION_PNP) \
		{ \
		GF1_DELAY (1);
		// your code here

#define GF1_SELFMOD_END		\
		}



/********************************************* Types **********************************************/

/* Board revision
 N: Operators <, > should work as expected
 */
enum GF1Revision
	{
	// Pre 3.4 board (FIXME: don't know how to recognize this)
	GF1REVISION_PRE34 = 1,

	// 3.4+, pre 3.7 board
	GF1REVISION_PRE37 = 2,

	// First 3.7 with ICS-2101 mixer, flipped channels
	GF1REVISION_ICSFLIPPED = 3,

	// 3.7 with ICS-2101 mixer, channels ok
	GF1REVISION_ICS = 4,

	// GUS Max (CS4231 codec)
	GF1REVISION_MAX = 5,

	// GUS PnP (Interwave - nearly compatible with Max if not in enhanced mode)
	GF1REVISION_PNP = 6
	};


/* Voice allocation info / priority
 N: synth voices can be stolen
 N: standard voice allocation is:
	0 - left/mono GF1 waveout
	1 - right GF1 waveout
	2 - GF1 waveout notification
	3 - GF1 wavein notification
 */
enum GF1VoiceAlloc
	{
	// Unused voice
	GF1VOICE_FREE = 0,

	// Allocated by synth - can be stolen
	GF1VOICE_SYNTH,

	// Allocated by GF1 waveout (left, right, notification)
	GF1VOICE_WAVEOUT,

	// Allocated by GF1 wavein (notification)
	GF1VOICE_WAVEIN,

	// Invalid voice (if index >= # of active voices)
	GF1VOICE_INVALID
	};


/* Notification reason
 */
enum GF1VoiceReason
	{
	/* Voice has been stolen by someone. The one who stole it is responsible for
	 calling this. Caller must supply locked voice info array. 'alloc' will contain
	 new owner.

	 Warning: Caller might held various locks (or something like that) while calling
	 this, so be careful.

	 Note: This is may be called if voice becomes invalid because of call to set_voices()
	 */
	GF1VREASON_STOLEN,

	/* Voice has been stopped and GF1 frequency has changed, and this voice is still
	 valid. State of GF1REGV_VOICE_CTRL and GF1REGV_RAMP_CTRL has changed and you must
	 restore them. Contents of GF1REGV_FREQUENCY don't make sense now and it must be
	 changed too.
	 */
	GF1VREASON_FREQUENCY_CHANGED
	};


struct GF1Voice;


/* Notification callback
 I: voice - voice #
 I: reason - callback reason
 I: info - locked voice info array
 I: context - user context
 N: Voice info spin lock will be always held while calling this. Callbacks should
	be nonpaged and short (they run at DISPATCH_LEVEL). See GF1VoiceReason description
	for per-reason notes.
 */
typedef void GF1VoiceNotificationFn
	(
	IN ULONG			voice,
	IN GF1VoiceReason	reason,
	IN GF1Voice *		info,
	IN PVOID			context
	);


/* Voice description
 */
struct GF1Voice
	{
	// Voice allocation
	GF1VoiceAlloc				alloc;

	// Notification callback (may be NULL, must be NULL if voice is FREE or INVALID)
	GF1VoiceNotificationFn *	callback;

	// Notification context
	PVOID						context;
	};


/* Wavetable / volume ramp IRQ handler
 I: voice - # of voice which caused this interrupt
 I: context - anything
 N: this routine will be called at raised IRQL, so finish ASAP (& no paged stuff)
 */
typedef void GF1VoiceIrqFn
	(
	IN ULONG	voice,
	IN PVOID	context
	);


/* Timer handler
 I: context - anything
 N: this routine will be called at raised IRQL, so finish ASAP (& no paged stuff)
 N: to get another timer interrupt, you must restart it manually
 */
typedef void GF1TimerIrqFn
	(
	IN PVOID	context
	);


/* DMA channel
 */
enum GF1DmaChannel
	{
	// Memory access
	GF1DMACHANNEL_DRAM,

	// GF1 recording (not available on PnP)
	GF1DMACHANNEL_RECORD,

	// CS4231 playback (Max+)
	GF1DMACHANNEL_CODEC_PLAYBACK,

	// CS4231 recording (Max+)
	GF1DMACHANNEL_CODEC_RECORD
	};


/* Codec compression formats
 */
enum CODECCompressType
	{
	CODEC_COMPRESS_NONE,
	CODEC_COMPRESS_ADPCM,
	CODEC_COMPRESS_ULAW,
	CODEC_COMPRESS_ALAW
	};


/* Codec handler (codec playback, codec record, codec timer)
 I: context - anything
 N: this routine will be called at raised IRQL, so finish ASAP (& no paged stuff)
 */
typedef void GF1CodecIrqFn
	(
	IN PVOID	context
	);



/******************************************** Tables **********************************************/

// Number of voices -> mixing rate
extern const ULONG gf1_voices2frequency[32-14+1];


// GF1 recording frequencies
#define GF1_RECFREQ_COUNT	24
extern const ULONG gf1_recfreq[GF1_RECFREQ_COUNT];


// Frequencies supported by codec playback and record
#define CODEC_FREQ_COUNT	14
extern const ULONG codec_freq[CODEC_FREQ_COUNT];
extern const BYTE codec_freq_format[CODEC_FREQ_COUNT];



/********************************** GF1Common class interface *************************************/

// {D655AEA5-18FE-4f37-A496-08EF11FD4ED0}
DEFINE_GUID (IID_IGF1Common,
	0xd655aea5, 0x18fe, 0x4f37, 0xa4, 0x96, 0x8, 0xef, 0x11, 0xfd, 0x4e, 0xd0);



/* GF1Common interface
 */
DECLARE_INTERFACE_ (IGF1Common, IUnknown)
	{
    /************************************* Init thingies **************************************/

	/* Initialize GF1
	 I: resources - GF1 resources:
		 port(0) - 2x0-2xf range
		 port(1) - 3x0-3xf range
		 port(2) - 726
		 irq(0) - GF1 IRQ
		 irq(1) - MIDI IRQ
		 dma(0) - GF1 DRAM (playback) / CS4231 record DMA
		 dma(1) - GF1 record DMA / CS4231 playback DMA
	 I: physical_device - ptr to physical device object (as passed to 'add_device')
	 I: device - ptr to device object (as passed to 'start_device')
	 N: IRQs must be shared (actually irq(1) is ignored now)
	 N: DMAs should be different
	 paged, PASSIVE_LEVEL
	 */
    STDMETHOD (init)
		(
		THIS_
		IN PRESOURCELIST	resources,
		IN PDEVICE_OBJECT	physical_device,
		IN PDEVICE_OBJECT	device
		) PURE;



	/************************************ Variable access *************************************/

	/* Get mixer settings
	 R: mixer settigs (GF1MIX_* mute bits only)
	 nonpaged, any level
	 */
	STDMETHOD_ (BYTE, get_mixer)
		(
		void
		) PURE;


	/* Set mixer settings
	 I: mix - mixer settigs (GF1MIX_* mute bits only)
	 N: this updates internal variable AND writes changes to GUS
	 nonpaged, synchronized, <=DISPATCH_LEVEL
	 */
	STDMETHOD_ (void, set_mixer)
		(
		IN BYTE		mix
		) PURE;


	/* Get indirect codec register
	 I: reg - codec register index
	 nonpaged, synchronized
	 */
	STDMETHOD_ (BYTE, get_codec_reg)
		(
		IN BYTE		reg
		) PURE;


	/* Set indirect codec register
	 I: reg - codec register index
	 I: mask - mask to be ANDed with old reg value (which bits should be PRESERVED)
	 I: value - value to be ORed with old reg value ANDed with mask
	 N: simply: new_reg = ((old_reg) & mask) | value);
	 nonpaged, synchronized
	 */
	STDMETHOD_ (void, set_codec_reg)
		(
		IN BYTE		reg,
		IN BYTE		mask,
		IN BYTE		value
		) PURE;


	/* Get board revision
	 R: revision
	 nonpaged, any level
	 */
	STDMETHOD_ (GF1Revision, get_revision)
		(
		void
		) PURE;


	/* Get mixing frequency
	 R: frequency in Hz
	 nonpaged, any level
	 */
	STDMETHOD_ (ULONG, get_frequency)
		(
		void
		) PURE;


	/* Get DMA resource list
	 R: resource list containing two DMA resources ([0]-DRAM, [1]-recording)
	 N: Descriptors may be the same if DMAs are shared.
	 nonpaged, any level (!operations with resources must be done at PASSIVE_LEVEL)
	 */
	STDMETHOD_ (PRESOURCELIST, get_dma_resources)
		(
		void
		) PURE;


	/* Get codec mode
	 nonpaged, any level
	 */
	STDMETHOD_ (ULONG, get_codec_mode)
		(
		void
		) PURE;



	/********************************* Device registry stuff **********************************/

	/* Compare string in registry
	 I: subkey - optional subkey name
	 I: valkey - value name
	 I: string2 - compare string in registry to this string
	 R: if value is not found, -1 is returned
		if value is string and 'string2' is NULL, 1 is returned
		otherwise standard case-insensitive strcmp result is returned
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD_ (LONG, registry_stricmp)
		(
		THIS_
		IN PCWSTR		subkey	OPTIONAL,
		IN PCWSTR		valkey,
		IN PCWSTR		string2 OPTIONAL
		) PURE;


	/* Read integer from registry
	 I: subkey - subkey name (NULL - no subkey)
	 I: valkey - value name
	 I: volat - volatile keys?
	 O: value - value data
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD (read_registry_int)
		(
		THIS_
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	valkey,
		IN BOOLEAN	volat,
		OUT PULONG	value
		) PURE;


	/* Write string to registry
	 I: subkey - subkey name (NULL - no subkey)
	 I: valkey - value name
	 I: value - value data
	 I: volat - volatile keys?
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD (write_registry_str)
		(
		THIS_
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	valkey,
		IN PCWSTR	value,
		IN BOOLEAN	volat
		) PURE;


	/* Write integer to registry
	 I: subkey - subkey name (NULL - no subkey)
	 I: valkey - value name
	 I: value - value data
	 I: volat - volatile keys?
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD (write_registry_int)
		(
		THIS_
		IN PCWSTR	subkey	OPTIONAL,
		IN PCWSTR	valkey,
		IN ULONG	value,
		IN BOOLEAN	volat
		) PURE;


	/* Write info to registry
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD_ (void, reg_putstr)
		(
		THIS_
		IN ULONG	type,
		IN PCWSTR	name,
		IN PCWSTR	value
		) PURE;

	STDMETHOD_ (void, reg_putint)
		(
		THIS_
		IN ULONG	type,
		IN PCWSTR	name,
		IN ULONG	value
		) PURE;

	/* Read info from registry
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD (reg_getint)
		(
		THIS_
		IN ULONG	type,
		IN PCWSTR	name,
		OUT PULONG	value
		) PURE;

	#define GF1REGPUT_INFO		1
	#define GF1REGPUT_DBG		2
	#define GF1REGPUT_CONFIG	3

	#define GF1REGGET_INFO		GF1REGPUT_INFO
	#define GF1REGGET_DBG		GF1REGPUT_DBG
	#define GF1REGGET_CONFIG	GF1REGPUT_CONFIG


	// Debugging registry writes
	#if GF1_DBG

	#define GF1_DBGSTR_(o,n,v)	(o)->reg_putstr (GF1REGPUT_DBG, (n), (v))
	#define GF1_DBGINT_(o,n,v)	(o)->reg_putint (GF1REGPUT_DBG, (n), (v))
	#define GF1_DBGSTR(n,v)		reg_putstr (GF1REGPUT_DBG, (n), (v))
	#define GF1_DBGINT(n,v)		reg_putint (GF1REGPUT_DBG, (n), (v))

	#else /* GF1_DBG */

	#define GF1_DBGSTR_(o,n,v)
	#define GF1_DBGINT_(o,n,v)
	#define GF1_DBGSTR(n,v)
	#define GF1_DBGINT(n,v)

	#endif /* GF1_DBG */


	// Information registry writes
	#define GF1_INFSTR_(o,n,v)	(o)->reg_putstr (GF1REGPUT_INFO, (n), (v))
	#define GF1_INFINT_(o,n,v)	(o)->reg_putint (GF1REGPUT_INFO, (n), (v))
	#define GF1_INFSTR(n,v)		reg_putstr (GF1REGPUT_INFO, (n), (v))
	#define GF1_INFINT(n,v)		reg_putint (GF1REGPUT_INFO, (n), (v))


	/* Write std. debugging info to registry
	 I: from - debugging "source"
	 paged, PASSIVE_LEVEL
	 */
	STDMETHOD_ (void, debug_to_registry)
		(
		THIS_
		IN PCWSTR	from
		) PURE;



    /************************************ Register stuff **************************************/
	// gf1cmn.cpp

	/* Write to direct register / port
	 I: reg - register number (GF1R_*, CR_*)
	 I: val - value to write
	 nonpaged, any level
	 */
	STDMETHOD_ (void, dwrite)
		(
		THIS_
		IN DWORD	reg,
		IN BYTE		val
		) PURE;


	/* Read direct register / port
	 I: reg - register number (GF1R_*, CR_*)
	 R: value
	 nonpaged, any level
	 */
	STDMETHOD_ (BYTE, dread)
		(
		THIS_
		IN DWORD	reg
		) PURE;


	/* Write to indirect register (8bit)
	 I: reg - register number (GF1REG_* or GF1REGV_*)
	 I: val - value to write
	 nonpaged, any level
	 */
	STDMETHOD_ (void, iwrite8)
		(
		THIS_
		IN BYTE		reg,
		IN BYTE		val
		) PURE;


	/* Write to indirect register (16bit)
	 I: reg - register number (GF1REG_* or GF1REGV_*)
	 I: val - value to write
	 nonpaged, any level
	 */
	STDMETHOD_ (void, iwrite16)
		(
		THIS_
		IN BYTE		reg,
		IN WORD		val
		) PURE;


	/* Read indirect register (8bit)
	 I: reg - register number (GF1REG_* or GF1REGV_*)
	 R: register value
	 nonpaged, any level
	 */
	STDMETHOD_ (BYTE, iread8)
		(
		THIS_
		IN BYTE		reg
		) PURE;


	/* Read indirect register (16bit)
	 I: reg - register number (GF1REG_* or GF1REGV_*)
	 R: register value
	 nonpaged, any level
	 */
	STDMETHOD_ (WORD, iread16)
		(
		THIS_
		IN BYTE		reg
		) PURE;


	/* Write byte to DRAM
	 I: addr - physical DRAM address
	 I: val - value
	 nonpaged, any level
	 */
	STDMETHOD_ (void, poke)
		(
		THIS_
		IN DWORD	addr,
		IN BYTE		val
		) PURE;


	/* Read byte from DRAM
	 I: addr - physical DRAM address
	 R: value
	 nonpaged, any level
	 */
	STDMETHOD_ (BYTE, peek)
		(
		THIS_
		IN DWORD	addr
		) PURE;


	/* Write to indirect codec register (8bit)
	 I: reg - register number (CREG_*)
	 I: val - value to write
	 nonpaged, any level
	 */
	STDMETHOD_ (void, ciwrite8)
		(
		THIS_
		IN BYTE		reg,
		IN BYTE		val
		) PURE;


	/* Read indirect codec register (8bit)
	 I: reg - register number (CREG_*)
	 R: register value
	 nonpaged, any level
	 */
	STDMETHOD_ (BYTE, ciread8)
		(
		THIS_
		IN BYTE		reg
		) PURE;



	/********************************** Voice manipulation ************************************/

	// Convert physical address to logical address (FIXME: only >> 1 on interwave)
	#define GF1_16BIT_ADDR(a)	(((a) & 0xc0000) | (((a) >> 1) & 0x1ffff))	// For 16bit voices
	#define GF1_8BIT_ADDR(a)	(a)											// For 8bit voices

	// Voice positions (logical addresses) (FIXME: more bits on interwave)
	#define GF1VOICE_START(o,a) \
		(o)->iwrite16 (GF1REGV_STARTH, (WORD) (((a) >> 7) & 0x1fff)); \
		(o)->iwrite16 (GF1REGV_STARTL, (WORD) (((a) & 0x7f) << 9));

	#define GF1VOICE_END(o,a) \
		(o)->iwrite16 (GF1REGV_ENDH, (WORD) (((a) >> 7) & 0x1fff)); \
		(o)->iwrite16 (GF1REGV_ENDL, (WORD) (((a) & 0x7f) << 9));

	#define GF1VOICE_POS(o,a) \
		(o)->iwrite16 (GF1REGV_POSH, (WORD) (((a) >> 7) & 0x1fff)); \
		(o)->iwrite16 (GF1REGV_POSL, (WORD) (((a) & 0x7f) << 9));


	/* Change playback frequency of current voice
	 I: freq - frequency in Hz
	 nonpaged (not synchronized), any level
	 */
	STDMETHOD_ (void, set_voice_frequency)
		(
		IN ULONG	freq
		) PURE;


	/* Change number of active voices
	 I: voices - new number of active voices
	 N: this is synchronized AND locks voice infos - make sure you don't hold the lock
	 nonpaged (synchronized), <=DISPATCH_LEVEL
	 */
	STDMETHOD_ (void, set_voices)
		(
		IN ULONG	voices
		) PURE;


	/* Select voice.
	 I: voice - voice number (0-31)
	 N: On InterWave selecting voice will disable Auto-increment mode.
	 nonpaged (not synchronized), any level
	 */
	STDMETHOD_ (void, select_voice)
		(
		IN BYTE voice
		) PURE;


	/* Set voice panning
	 I: pan - must be in 0-255 range (0==left, 127==middle, 255==right)
	 N: Voice must be selected.
	    Assumes that if InterWave is present and is in enhanced mode then
	    all voices are in offset mode.
	 nonpaged (not synchronized), any level
	 */
	STDMETHOD_ (void, voice_set_panning)
		(
		IN BYTE	pan
		) PURE;


	/* Lock voice info
	 O: old_irql - pass this back to 'unlock_voices()'
	 R: pointer to array describing voices ([32])
	 N: beware - this acquires spin-lock, so be quick
	 nonpaged, <=DISPATCH_LEVEL
	 */
	STDMETHOD_ (GF1Voice *, lock_voices)
		(
		THIS_
		OUT PKIRQL	old_irql
		) PURE;


	/* Unlock voice info
	 I: old_irql - value stored by 'lock_voices()'
	 N: beware - this releases spin-lock, so be sure you actually locked it
	 nonpaged, DISPATCH_LEVEL
	 */
	STDMETHOD_ (void, unlock_voices)
		(
		THIS_
		IN KIRQL	old_irql
		) PURE;



	/********************************** Interrupt handling ************************************/

	/* Call routine synchronized with interrupts
	 I: routine - nonpaged routine to call
	 I: context - anything
	 N: every access to GUS hardware must be synchronized
	 nonpaged, <=DISPATCH_LEVEL (?)
	 */
	STDMETHOD_ (void, call_synchronized)
		(
		IN PINTERRUPTSYNCROUTINE	routine,
		IN PVOID					context
		) PURE;


	/* Install wavetable IRQ handler
	 I: voice - which voice should this handle
	 I: ack_handler - acknowledge IRQ function (stop generating ints), use NULL to remove handler
	 I: action_handler - callback function (called at eoi), use NULL to remove handler
	 I: context - anything
	 R: TRUE on success, FALSE if this voice already has an handler
	 partially paged, synchronized, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, set_wavetable_handler)
		(
		IN ULONG			voice,
		IN GF1VoiceIrqFn *	ack_handler,
		IN GF1VoiceIrqFn *	action_handler,
		IN PVOID			context
		) PURE;


	/* Install volume ramp IRQ handler
	 I: voice - which voice should this handle
	 I: ack_handler - acknowledge IRQ function (stop generating ints), use NULL to remove handler
	 I: action_handler - callback function (called at eoi), use NULL to remove handler
	 I: context - anything
	 R: TRUE on success, FALSE if this voice already has an handler
	 partially paged, synchronized, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, set_ramp_handler)
		(
		IN ULONG			voice,
		IN GF1VoiceIrqFn *	ack_handler,
		IN GF1VoiceIrqFn *	action_handler,
		IN PVOID			context
		) PURE;


	/* Install AdLib/GF1 timer IRQ handler
	 I: ack_handler - acknowledge IRQ function (stop generating ints), use NULL to remove handler
	 I: action_handler - callback function (called at eoi), use NULL to remove handler
	 I: context - anything
	 R: TRUE on success, FALSE if the timer already has an handler
	 N: this handles both AdLib timers
	 partially paged, synchronized, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, set_gf1timer_handler)
		(
		IN GF1TimerIrqFn *	ack_handler,
		IN GF1TimerIrqFn *	action_handler,
		IN PVOID			context
		) PURE;


	/* Install CS4231 playback IRQ handler
	 I: handler - action IRQ function, use NULL to remove handler
	 I: context - anything
	 R: TRUE on success, FALSE if the timer already has an handler
	 N: IRQs are acked automatically BEFORE call to this
	 partially paged, synchronized, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, set_playback_handler)
		(
		IN GF1CodecIrqFn *	handler,
		IN PVOID			context
		) PURE;


	/* Install CS4231 record IRQ handler
	 I: handler - action IRQ function, use NULL to remove handler
	 I: context - anything
	 R: TRUE on success, FALSE if the timer already has an handler
	 N: IRQs are acked automatically BEFORE call to this
	 partially paged, synchronized, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, set_record_handler)
		(
		IN GF1CodecIrqFn *	handler,
		IN PVOID			context
		) PURE;


	/* Install CS4231 timer IRQ handler
	 I: handler - action IRQ function, use NULL to remove handler
	 I: context - anything
	 R: TRUE on success, FALSE if the timer already has an handler
	 N: IRQs are acked automatically BEFORE call to this
	 partially paged, synchronized, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, set_cstimer_handler)
		(
		IN GF1CodecIrqFn *	handler,
		IN PVOID			context
		) PURE;



	/****************************************** DMA *******************************************/

	/* Grab DMA channel
	 I: channel - channel to allocate
	 R: TRUE on success, FALSE if this channel can't be allocated
	 N: GF1DMACHANNEL_DRAM can be allocated multiple times.
		GF1DMACHANNEL_DRAM & GF1DMACHANNEL_CODEC_RECORD can't be used at the same time.
		GF1DMACHANNEL_RECORD & GF1DMACHANNEL_CODEC_PLAYBACK can't be used at the same time.
		If DMAs are shared, only one channel may be used.
		Daughterboard CODEC is not supported.
	 N: This initializes DRAM DMA manager if needed.
	 nonpaged, PASSIVE_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, get_dma)
		(
		IN GF1DmaChannel	channel
		) PURE;


	/* Free DMA channel
	 I: channel - channel to free
	 nonpaged, PASSIVE_LEVEL
	 */
	STDMETHOD_ (void, put_dma)
		(
		IN GF1DmaChannel	channel
		) PURE;


	/* Get DRAM DMA alignment
	 R: alignment mask
	 nonpaged, any level
	 */
	STDMETHOD_ (ULONG, ddma_get_alignment)
		(
		void
		) PURE;


	/* Schedule DRAM download from nonpaged memory
	 I: dram_address - GF1 DRAM physical address
	 I: src_address - source virtual address
	 I: length - length in bytes, DRAM-aligned
	 I: flags - transfer flags (GF1DMA_DATA16, GF1DMA_UNSIGNED)
	 I: counter - decrement this when transfer is done
	 I: event - signel this after decrementing 'counter'
	 N: Sheduled transfer can't be cancelled. You will have to use 'counter' and/or 'event'.
	 nonpaged, <=DISPATCH_LEVEL
	 */
	STDMETHOD_ (BOOLEAN, ddma_download_nonpaged)
		(
		IN ULONG		dram_address,
		IN PVOID		src_address,
		IN ULONG		length,
		IN ULONG		flags,
		IN PLONG		counter			OPTIONAL,
		IN PRKEVENT		event			OPTIONAL
		) PURE;
	};


/* Create new GF1 common object
 O: unknown - ptr to unknown interface of GF1Common
 I: pool_type - pool type (preferably NonPagedPool)
 paged, PASSIVE_LEVEL
 */
NTSTATUS create_gf1_common
	(
	OUT PUNKNOWN *	unknown,
    IN POOL_TYPE	pool_type
	);


#endif  /* _GF1CMN_H_ */
