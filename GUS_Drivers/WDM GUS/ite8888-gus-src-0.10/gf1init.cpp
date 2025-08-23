/*
 * gf1init
 *
 * GUS initialization
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


#include "gf1cmnc.h"


#pragma code_seg("PAGE")


/* Create new GF1 common object
 */
NTSTATUS create_gf1_common
	(
    OUT PUNKNOWN *	unknown,
    IN POOL_TYPE	pool_type
	)

	{
    PAGED_CODE();

    ASSERT (unknown);

    STD_CREATE_BODY_ (CGF1Common, unknown, NULL, pool_type, IGF1Common *);
	}



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg("PAGE")


/* Constructor
 */
CGF1Common::CGF1Common
	(
	PUNKNOWN unknown
	) : CUnknown (unknown)

	{
	PAGED_CODE();

	ULONG v;

	physical_device_object = NULL;
	device_object = NULL;

	clear_dreg_to_port_table();
	gf1_irq = 0;
	midi_irq = 0;
	dram_dma = 0;
	rec_dma = 0;
	dram_size = 0;
	dma_resources = NULL;
	board_revision = GF1REVISION_PRE37;

	interrupt_sync = NULL;
	midi_interrupt_sync = NULL;
	for (v = 0; v < 32; v++)
		{
		wavetable_ack_handler[v] = NULL;
		wavetable_action_handler[v] = NULL;
		ramp_ack_handler[v] = NULL;
		ramp_action_handler[v] = NULL;
		}
	gf1timer_ack_handler = NULL;
	gf1timer_action_handler = NULL;
	playback_handler = NULL;
	record_handler = NULL;
	cstimer_handler = NULL;

	ddma_adapter = NULL;
	ddma_enabled = FALSE;
	ddma_busy = FALSE;
	ddma_max_map_regs = 0;
	ddma_alignment = 0;
	//ddma_lock
	//ddma_request
	ddma_first = -1;
	ddma_last = -1;
	ddma_offset = 0;
	ddma_sending = 0;
	ddma_reg_base = NULL;
	ddma_currentva = NULL;

	gf1_voices = 14;
	gf1_mixing_frequency = 44100;
	mixer_settings = 0;
	info_enabled = TRUE;
	iw_mode = IW_GUS_COMPATIBLE_MODE;
	codec_mode = CODEC_MODE1;
	for (v = 0; v < 32; v++)
		{
		voice[v].alloc = GF1VOICE_INVALID;
		voice[v].callback = NULL;
		voice[v].context = NULL;
		}
	ddma_inits = 0;
	dma_rec_allocated = FALSE;
	dma_cplay_allocated = FALSE;
	dma_crec_allocated = FALSE;

	check_wave_irq = TRUE;
	check_ramp_irq = TRUE;
	check_midi_irq = TRUE;
	check_timer_irq = TRUE;
	check_dma_irq = NULL;

	#if GF1_DBG
	wave_irqs_handled = 0;
	ramp_irqs_handled = 0;
	timer_irqs_handled = 0;
	ddma_irqs_handled = 0;
	rdma_irqs_handled = 0;
	xmit_irqs_handled = 0;
	recv_irqs_handled = 0;
	cplay_irqs_handled = 0;
	crec_irqs_handled = 0;
	ctimer_irqs_handled = 0;
	ddma_transferred = 0;
	#endif /* GF1_DBG */
	}


#pragma code_seg ("PAGE")


/* Destructor
 */
CGF1Common::~CGF1Common()
	{
	PAGED_CODE();

	#if GF1_DBG
	GF1_DBGINT (L"DestroyingCGF1Common", 1);
	#endif /* GF1_DBG */

	close_dram_dma();

	if (board_revision >= GF1REVISION_MAX)
		{
		codec_close();
		}

	if (check_dma_irq)
		{
		ExFreePool (check_dma_irq);
		check_dma_irq = NULL;
		}

	// Clear pending interrupts
	if (dreg_to_port_table[0] && interrupt_sync)
		{
		// FIXME: only clear pending interrupts, and synchronized
		reset_gus();
		}

	// Clear Direct register to port address mapping table.
	clear_dreg_to_port_table();

	if (dma_resources)
		{
		dma_resources->Release();
		}

	if (midi_interrupt_sync)
		{
		midi_interrupt_sync->Disconnect();
		midi_interrupt_sync->Release();
		}

	if (interrupt_sync)
		{
		interrupt_sync->Disconnect();
		interrupt_sync->Release();
		}
	}


#pragma code_seg ("PAGE")


/* Obtain an interface
 */
STDMETHODIMP CGF1Common::NonDelegatingQueryInterface
	(
	REFIID		iface,
    PVOID *		object
	)

	{
	PAGED_CODE();

	ASSERT (object);

	if (IsEqualGUIDAligned (iface, IID_IUnknown))
		{
		*object = (PVOID) ((PUNKNOWN) ((IGF1Common *) this));
		}
	else if (IsEqualGUIDAligned (iface, IID_IGF1Common))
		{
		*object = (PVOID) ((IGF1Common *) this);
		}
	//else if (IsEqualGUIDAligned (iface, IID_IAdapterPowerManagment))
	//	{
	//	*object = (PVOID) ((PADAPTERPOWERMANAGMENT) this);
	//	}
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



/*************************************** IGF1Common stuff *****************************************/

    /************************************* Init thingies **************************************/

#pragma code_seg("PAGE")


/* Clear Direct register to port address mapping table.
 */
void CGF1Common::clear_dreg_to_port_table
	(
	void
	)
	{
	ULONG i;

	for (i=0; i<DREG_TO_PORT_TABLE_LENGTH; i++)
		dreg_to_port_table[i] = NULL;
	}


#pragma code_seg("PAGE")


/* Check port settings by writing/reading DRAM
 */
BOOLEAN CGF1Common::check_port (void)
	{
	PAGED_CODE();

	// FIXME: Set IWREG_MEM_CTRL to access DRAM memory.
	//		  ? Is GUS reset doing it for us?

	// Reset GF1
	// On InterWave cards this will reset InterWave to GUS compatible mode.

	iw_mode = IW_GUS_COMPATIBLE_MODE;

	iwrite8 (GF1REG_RESET, 0);
	GF1_DELAY (5);
	iwrite8 (GF1REG_RESET, GF1RES_RESET);
	GF1_DELAY (5);

	// Write something to DRAM
	poke (0, 0xde);
	poke (123, 0xad);
	if (peek (0) != 0xde || peek (123) != 0xad)
		{
		return FALSE;
		}

	// Ok, port settings seem ok
	return TRUE;
	}


#pragma code_seg ("PAGE")


/* Detect size of GUS DRAM
 */
ULONG CGF1Common::detect_dram_size
	(
	void
	)

	{
	PAGED_CODE();

	ULONG res;

	if (board_revision == GF1REVISION_PNP)
		{
		res = iw_detect_dram_size();
		}
	else
		{
		poke (0x01f1f, 123);
		poke (0x41f1f, 125);
		poke (0x81f1f, 127);
		poke (0xc1f1f, 129);

		res = 0x100000;
		if (peek (0xc1f1f) != 129)
			res = 0xc0000;
		if (peek (0x81f1f) != 127)
			res = 0x80000;
		if (peek (0x41f1f) != 125)
			res = 0x40000;
		if (peek (0x01f1f) != 123)
			res = 0;
		}
	GF1_INFINT (L"DRAMSize", res);

	return res;
	}


#pragma code_seg()


/* Program GF1 IRQ and DMA latches
 N: This order is directly copied from GUS SDK 2.22. I don't really know
    what that 'dmal | 0x80' could mean (it sets bit that is described as "reserved"),
 N: No IOW may occur between write to MIX_CTRL and to CONTROL reg. I don't know
    if raising IRQL is the right way to do it, and I don't know whether it works or not.
 */
void CGF1Common::program_latches
	(
	BYTE	mix,
	BYTE	irql,
	BYTE	dmal
	)

	{
	KIRQL old_irql;

	// FIXME: Is SYNCH_LEVEL the right level?
	KeRaiseIrql (SYNCH_LEVEL, &old_irql);

	dwrite (GF1R_MIX_CTRL, mix);
	dwrite (GF1R_CONTROL, dmal | 0x80);
	GF1_DELAY (1);

	dwrite (GF1R_MIX_CTRL, mix | GF1MIX_CONTROL);
	dwrite (GF1R_CONTROL, irql);
	GF1_DELAY (1);

	dwrite (GF1R_MIX_CTRL, mix);
	dwrite (GF1R_CONTROL, dmal);
	GF1_DELAY (1);

	dwrite (GF1R_MIX_CTRL, mix | GF1MIX_CONTROL);
	dwrite (GF1R_CONTROL, irql);
	GF1_DELAY (1);

	KeLowerIrql (old_irql);
	}


#pragma code_seg ("PAGE")


/* Find out board revision
 */
void CGF1Common::find_board_revision
	(
	void
	)

	{
	PAGED_CODE();

	BYTE rev;

	// Stupid PnP "detection"
	if (!dreg_to_port_table[GF1R_MAX_CTRL])
		{
		board_revision = GF1REVISION_PNP;
		GF1_INFSTR (L"BoardRevision", L"PnP");
		return;
		}

	rev = dread (GF1R_REVISION);
	if (rev == GF1REV_PRE37)
		{
		// Revision register not supported - older than 3.7 - is it 3.4?
		board_revision = GF1REVISION_PRE37;

		/*
		 Obviously neither 2xb nor 2xf is readable. It seems there's no reliable way
		 to detect 3.4 vs. pre3.4 boards. Assume it's 3.4, it can't hurt.
		 /
		dwrite (GF1R_REG_CTRL, GF1REGCTL_JUMPER);
		if (dread (GF1R_REG_CTRL) != GF1REGCTL_JUMPER)
			rev = GF1REVISION_PRE34;
		dwrite (GF1R_REG_CTRL, 0);
		if (dread (GF1R_REG_CTRL) != 0)
			rev = GF1REVISION_PRE34;
		 */

		if (rev == GF1REVISION_PRE34)
			GF1_INFSTR (L"BoardRevision", L"pre3.4");
		else
			GF1_INFSTR (L"BoardRevision", L"pre3.7");
		}
	else if (rev >= GF1REV_ICSMIN && rev <= GF1REV_ICSMAX)
		{
		// Classic with ICS mixer
		if (rev == GF1REV_ICSMIN)
			{
			board_revision = GF1REVISION_ICSFLIPPED;
			GF1_INFSTR (L"BoardRevision", L"3.7 flipped ICS2101");
			}
		else
			{
			board_revision = GF1REVISION_ICS;
			GF1_INFSTR (L"BoardRevision", L"3.7 ICS2101");
			}
		}
	else if (rev >= GF1REV_MAXMIN && rev <= GF1REV_MAXMAX)
		{
		// UltraSound MAX
		board_revision = GF1REVISION_MAX;
		GF1_INFSTR (L"BoardRevision", L"Max");
		}
	else if (rev >= GF1REV_ACEMIN && rev <= GF1REV_ACEMAX)
		{
		// I don't know how much these boards are compatible
		board_revision = GF1REVISION_PRE37;
		GF1_INFSTR (L"BoardRevision", L"Ace");
		}
	else if (rev >= GF1REV_VIPERMIN && rev <= GF1REV_VIPERMIN)
		{
		// I don't know how much these boards are compatible
		board_revision = GF1REVISION_PRE37;
		GF1_INFSTR (L"BoardRevision", L"Viper");
		}
	else if (rev >= GF1REV_2102MIN && rev <= GF1REV_2102MAX)
		{
		// I don't know how much these boards are compatible
		board_revision = GF1REVISION_PRE37;
		GF1_INFSTR (L"BoardRevision", L"ICS2102");
		}
	}


#pragma code_seg ("PAGE")


/* Master reset
 */
BOOLEAN CGF1Common::reset_gus
	(
	void
	)

	{
	PAGED_CODE();

	mixer_settings = GF1MIX_NOLINEIN | GF1MIX_NOLINEOUT | GF1MIX_LATCHES;

	// Note: Reset must be already done by check_port()

	// Mute
	dwrite (GF1R_MIX_CTRL, (BYTE) mixer_settings);
	GF1_DELAY (1);


	// Read IRQ status and other registers to clear some interrupts
	dread (GF1R_IRQ_STATUS);
	iread8 (GF1REG_DMA);
	iread8 (GF1REG_SMP_CTRL);
	iread8 (GF1REGV_VIRQ);

	// Reset MIDI port (and disable IRQs)
	dwrite (GF1R_MIDI_CTRL, GF1MIDIC_RESET);
	GF1_DELAY (2);
	dwrite (GF1R_MIDI_CTRL, 0);

	// Disable DMA
	iwrite8 (GF1REG_DMA, 0);

	// Disable timer
	dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
	dwrite (GF1R_TIMER_DATA, GF1TDATA_CLEAR_IRQ);
	iwrite8 (GF1REG_TIMER, 0);

	// Disable GF1 sampling
	if (board_revision < GF1REVISION_PNP)
		iwrite8 (GF1REG_SMP_CTRL, 0);

	// Set number of voices to 32
	iwrite8 (GF1REGV_VOICES, (32 - 1) | 0xc0);

	// Stop all voices
	for (ULONG i = 0; i < 32; i++)
		{
		dwrite (GF1R_VSELECT, (BYTE) i);

		// Stop voice and volume ramp
		iwrite16 (GF1REGV_VOLUME, 0);
		iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
		GF1_SELFMOD(this)
			iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
			iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
		GF1_SELFMOD_END
		GF1VOICE_START (this, 0);
		GF1VOICE_END (this, 0);
		GF1VOICE_POS (this, 0);

		iread8 (GF1REGV_VIRQ);

		if (i < 14)
			{
			voice[i].alloc = GF1VOICE_FREE;
			}
		else
			{
			voice[i].alloc = GF1VOICE_INVALID;
			}
		voice[i].callback = NULL;
		voice[i].context = NULL;
		}

	// Set number of voices to 14
	iwrite8 (GF1REGV_VOICES, (14 - 1) | 0xc0);
	gf1_voices = 14;
	gf1_mixing_frequency = 44100;

	// Read IRQ status and other registers to clear some interrupts again
	dread (GF1R_IRQ_STATUS);
	iread8 (GF1REG_DMA);
	iread8 (GF1REG_SMP_CTRL);
	iread8 (GF1REGV_VIRQ);


	// Enable MIDI and clear IRQs (3.4+ only)
	if (board_revision > GF1REVISION_PRE34)
		{
		dwrite (GF1R_REG_CTRL, GF1REGCTL_JUMPER);
		dwrite (GF1R_CONTROL, GF1JUMP_MIDI); // FIXME: enable gameport?
		dwrite (GF1R_REG_CTRL, GF1REGCTL_CLRINT);
		dwrite (GF1R_CONTROL, 0);
		dwrite (GF1R_REG_CTRL, 0);
		}


	// Enable interrupts and DAC
	iwrite8 (GF1REG_RESET, GF1RES_ALL);
	GF1_DELAY (5);


	// Setup DMA and IRQ
	if (board_revision < GF1REVISION_PNP)
		{
		static const BYTE irq_latch[] =
			{
			0, 0, 0,
			3, // 3
			0,
			2, // 5
			0,
			4, // 7
			0,
			1, // 9 (aka 2)
			0,
			5, // 11
			6, // 12
			0, 0,
			7  // 15
			};
		static const BYTE dma_latch[] =
			{
			0,
			1, // 1
			0,
			2, // 3
			0,
			3, // 5
			4, // 6
			5  // 7
			};
		BYTE irql;
		BYTE dmal;

		// Compute IRQ latch value
		if (!irq_latch[gf1_irq] || !irq_latch[midi_irq])
			{
			GF1_INFSTR (L"ErrorMessage", L"Invalid IRQ level definition");
			return FALSE;
			}
		irql = irq_latch[gf1_irq];
		if (gf1_irq == midi_irq)
			{
			irql |= 0x40;
			mixer_settings |= GF1MIX_COMBINE;
			}
		else
			{
			irql |= irq_latch[midi_irq] << 3;
			}

		// Compute DMA latch value
		if (!dma_latch[dram_dma] || !dma_latch[rec_dma])
			{
			GF1_INFSTR (L"ErrorMessage", L"Invalid DMA channel definition");
			return FALSE;
			}
		dmal = dma_latch[dram_dma];
		if (dram_dma == rec_dma)
			{
			dmal |= 0x40;
			}
		else
			{
			dmal |= dma_latch[rec_dma] << 3;
			}

		// Program IRQ and DMA
		program_latches ((BYTE) mixer_settings, irql, dmal);
		}


	// Enable output and input
	mixer_settings &= ~(GF1MIX_NOLINEOUT | GF1MIX_NOLINEIN);
	mixer_settings |= GF1MIX_MICIN;// | GF1MIX_LATCHES;
	dwrite (GF1R_MIX_CTRL, (BYTE) mixer_settings);
	GF1_DELAY (1);

	// Write to voice select to disable writes to IRQ/DMA latches
	dwrite (GF1R_VSELECT, 0);
	GF1_DELAY (1);

	// FIXME: this read should probably be already synchronized with ISR
	if (iread8 (GF1REG_RESET) != GF1RES_ALL)
		{
		GF1_INFSTR (L"ErrorMessage", L"GF1 reset failed");
		return FALSE;
		}

	return TRUE;
	}


#pragma code_seg()


/* Perform initial checks
 */
NTSTATUS CGF1Common::init_check
	(
	IN PINTERRUPTSYNC	isync,
	IN PVOID			context
	)

	{
	CGF1Common *self = (CGF1Common *) context;

	self->check_wave_irq = TRUE;
	self->check_ramp_irq = TRUE;
	self->check_midi_irq = TRUE;
	self->check_timer_irq = TRUE;

	// Play part of memory with rollover enabled and generate IRQ (should stop voice)
	self->dwrite (GF1R_VSELECT, 0);

	self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOP | GF1VC_STOPPED);
	self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOP | GF1RC_STOPPED | GF1RC_ROLLOVER);
	GF1_SELFMOD(self)
		self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP | GF1RC_ROLLOVER);
	GF1_SELFMOD_END

	GF1VOICE_POS (self, 0);
	GF1VOICE_START (self, 0);
	GF1VOICE_END (self, 3000); // 3000 frames = ~64ms
	self->set_voice_frequency (44100);
	self->iwrite16 (GF1REGV_VOLUME, 0xf800);
	self->iwrite8 (GF1REGV_PANNING, 7);

	self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_WAVE_IRQ);
	GF1_SELFMOD(self)
		self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_WAVE_IRQ);
	GF1_SELFMOD_END

	// Volume ramp
	self->iwrite8 (GF1REGV_RAMP_START, 0x88);
	self->iwrite8 (GF1REGV_RAMP_END, 0xf8);
	self->iwrite8 (GF1REGV_RAMP_RATE, 1 | GF1RR_PERIOD_1); // 0x700 = 1792 frames = ~40ms

	self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_ROLLOVER | GF1RC_RAMP_IRQ | GF1RC_DOWN);
	GF1_SELFMOD(self)
		self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_ROLLOVER | GF1RC_RAMP_IRQ | GF1RC_DOWN);
	GF1_SELFMOD_END

	// Enable midi interrupts
	self->dwrite (GF1R_MIDI_CTRL, GF1MIDIC_XMIT_IRQ);

	// Start timer and enable timer IRQ
	self->iwrite8 (GF1REG_TIMER, 0);
	self->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
	self->dwrite (GF1R_TIMER_DATA, GF1TDATA_CLEAR_IRQ);
	self->iwrite8 (GF1REG_COUNT2, 0); // 0.32 * 256 ms = ~82ms
	self->iwrite8 (GF1REG_TIMER, GF1TIMER_IRQ2);
	self->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
	self->dwrite (GF1R_TIMER_DATA, GF1TDATA_T2START | GF1TDATA_T1MASK);

	return STATUS_SUCCESS;
	}


#pragma code_seg ("PAGE")


/* Initialize GF1
 */
NTSTATUS CGF1Common::init
	(
	IN PRESOURCELIST	resources,
	IN PDEVICE_OBJECT	physical_device,
	IN PDEVICE_OBJECT	device
	)

	{
	PAGED_CODE();

	NTSTATUS status;

	ASSERT (resources);
	if (!physical_device)
		{
		/*
		 FIXME: this may happen if multiple GUS devices are installed
		 (because of that silly global-variable-hack).
		 */
		return STATUS_INSUFFICIENT_RESOURCES;
		}
	ASSERT (device);

	// Initialize variables
	physical_device_object = physical_device;
	device_object = device;
	KeInitializeSpinLock (&voice_lock);
	KeInitializeDpc (&ddma_dpc, &ddma_tc_dpc, this);
	KeInitializeSpinLock (&ddma_lock);

	// Check if error occurred
	if (registry_stricmp (L"Info", L"ErrorMessage", NULL) == 1)
		{
		// Disable changes to info strings until 'ErrorMessage' value is delete
		info_enabled = FALSE;
		}
	else
		{
		// Delete registry info keys so that new info is not mixed with old stuff
		info_enabled = TRUE;
		delete_registry_key (L"Info");
		delete_registry_key (L"DebugInfo");
		}

	GF1_DBGSTR (L"Status", L"Initializing");

	// Check resources
	if (resources->NumberOfPorts() != 3 ||
		resources->NumberOfInterrupts() < 1 || resources->NumberOfInterrupts() > 2 ||
		resources->NumberOfDmas() < 1 || resources->NumberOfDmas() > 2)
		{
		// Wrong resource count
		GF1_INFSTR (L"ErrorMessage", L"Wrong number of resources");
		return STATUS_INSUFFICIENT_RESOURCES;
		}

	// Check ports (FIXME: translated port == untranslated port?)
	{
	ULONG port0 = resources->FindTranslatedPort(0)->u.Port.Start.LowPart;
	ULONG port1 = resources->FindTranslatedPort(1)->u.Port.Start.LowPart;
	ULONG port2 = resources->FindTranslatedPort(2)->u.Port.Start.LowPart;

	ULONG port0len = resources->FindTranslatedPort(0)->u.Port.Length;
	ULONG port1len = resources->FindTranslatedPort(1)->u.Port.Length;
	ULONG port2len = resources->FindTranslatedPort(2)->u.Port.Length;

	if (port0len == IW_P2XR_IO_SPACE_LENGTH &&
		port1len == IW_P3XR_IO_SPACE_LENGTH &&
		port2len == IW_PCODAR_IO_SPACE_LENGTH)
		{
		// UltraSound (InterWave) PnP card.

		if (port0 & (~IW_P2XR_MASK) ||
			port1 & (~IW_P3XR_MASK) ||
			port2 & (~IW_PCODAR_MASK))
			{
			// Bad ports.
			GF1_INFSTR (L"ErrorMessage", L"Invalid port definition (InterWave PnP)");
			return STATUS_DEVICE_CONFIGURATION_ERROR;
			}

		// Set up Direct register to port address mapping table.
		ULONG i;

		for (i=0; i<IW_P2XR_IO_SPACE_LENGTH; i++)
			dreg_to_port_table[i+0x00] = (PUCHAR) (port0+i);	// P2XR IO SPACE
		for (i=0; i<IW_P3XR_IO_SPACE_LENGTH; i++)
			dreg_to_port_table[i+0x10] = (PUCHAR) (port1+i);	// P3XR IO SPACE
		for (i=0; i<IW_PCODAR_IO_SPACE_LENGTH; i++)
			dreg_to_port_table[i+0x1c] = (PUCHAR) (port2+i);	// CODEC IO SPACE

		// Disable 3x6 port (Max Ctrl), so board_detect() detects this as PnP
		dreg_to_port_table[GF1R_MAX_CTRL] = NULL;
		}
	else
		{
		if (port0 & 0x00f || port0 < 0x210 || port0 > 0x260 ||
			port1 & 0x00f || port1 < 0x310 || port1 > 0x360 ||
			port0 + 0x100 != port1 || port0 + 0x506 != port2 ||
			port0len != 0x10 ||
			port1len != 0x10 ||
			port2len != 1)
			{
			// Bad ports
			GF1_INFSTR (L"ErrorMessage", L"Invalid port definition");
			return STATUS_DEVICE_CONFIGURATION_ERROR;
			}

		// Set up Direct register to port address mapping table.
		ULONG i;

		for (i=0; i<0x10; i++)
			dreg_to_port_table[i+0x00] = (PUCHAR) (port0+i);		// 0x2xx IO SPACE
		for (i=0; i<0x10; i++)
			dreg_to_port_table[i+0x10] = (PUCHAR) (port1+i);		// 0x3xx IO SPACE
		for (i=0; i<1; i++)
			dreg_to_port_table[i+0x20] = (PUCHAR) (port2+i);		// 0x7x6 IO SPACE
		}
	}


	// Get IRQs and DMAs
	gf1_irq = resources->FindUntranslatedInterrupt(0)->u.Interrupt.Level;
	if (resources->NumberOfInterrupts() > 1)
		midi_irq = resources->FindUntranslatedInterrupt(1)->u.Interrupt.Level;
	else
		midi_irq = gf1_irq;

	dram_dma = resources->FindUntranslatedDma(0)->u.Dma.Channel;
	if (resources->NumberOfDmas() > 1)
		rec_dma = resources->FindUntranslatedDma(1)->u.Dma.Channel;
	else
		rec_dma = dram_dma;

	// Make local copy of DMA resources
	status = PcNewResourceSublist (&dma_resources, NULL, NonPagedPool, resources, 2);
	if (!NT_SUCCESS (status))
		{
		GF1_INFSTR (L"ErrorMessage", L"Can't allocate DMA resource list");
		return status;
		}
	dma_resources->AddDmaFromParent (resources, 0);
	dma_resources->AddDmaFromParent (resources, dram_dma == rec_dma ? 0 : 1);


	// Check port settings
	if (!check_port())
		{
		// No GUS there
		GF1_INFSTR (L"ErrorMessage", L"No GUS found at specified ports");
		return STATUS_DEVICE_CONFIGURATION_ERROR;
		}


	// Find board revision
	find_board_revision();

	if (board_revision == GF1REVISION_PNP)
		{
		iw_reset_to_gus_mode();
		iw_configure_mem();
		}

	// Detect DRAM size
	dram_size = detect_dram_size();
	if (!dram_size)
		{
		// Should not happen, but...
		GF1_INFSTR (L"ErrorMessage", L"Memory detection failed");
		return STATUS_DEVICE_CONFIGURATION_ERROR;
		}


	// Install generic ISR
	status = PcNewInterruptSync (&interrupt_sync, NULL, resources, 0, InterruptSyncModeNormal);
	if (!NT_SUCCESS (status))
		{
		GF1_INFSTR (L"ErrorMessage", L"Can't allocate InterruptSync");
		return status;
		}
	status = interrupt_sync->RegisterServiceRoutine (&isr, (PVOID) this, FALSE);
	if (NT_SUCCESS (status))
		{
		status = interrupt_sync->Connect();
		}
	if (!NT_SUCCESS (status))
		{
		interrupt_sync->Release();
		interrupt_sync = NULL;
		GF1_INFSTR (L"ErrorMessage", L"Can't connect InterruptSync");
		return status;
		}

	// Install midi ISR if MIDI IRQ != GF1 IRQ
	if (midi_irq != gf1_irq)
		{
		status = PcNewInterruptSync (&midi_interrupt_sync, NULL, resources, 1, InterruptSyncModeNormal);
		if (!NT_SUCCESS (status))
			{
			GF1_INFSTR (L"ErrorMessage", L"Can't allocate midi InterruptSync");
			return status;
			}
		status = midi_interrupt_sync->RegisterServiceRoutine (midi_isr, (PVOID) this, FALSE);
		if (NT_SUCCESS (status))
			{
			status = midi_interrupt_sync->Connect();
			}
		if (!NT_SUCCESS (status))
			{
			midi_interrupt_sync->Release();
			midi_interrupt_sync = NULL;
			GF1_INFSTR (L"ErrorMessage", L"Can't connect midi InterruptSync");
			return status;
			}
		}


	// Finally reset GUS hw
	/*
	 Note: Because this sometimes does not work, we try several times
	 */
	{
	ULONG i;

	for (i = 0; i < 10; i++)
		{
		ULONGLONG t;
		LARGE_INTEGER d;

		// Initialize DRAM DMA channel
		status = init_dram_dma();
		if (!NT_SUCCESS (status))
			{
			// ErrorMessage written by init_dram_dma()
			return status;
			}
		ddma_enabled = TRUE;

		/* Note:
	 	Other DMA channels will be initialized in their miniports:
	 	- (DMA2) GF1 recording DMA - only if revision < Max
	 	- (DMA2) CS4231 playback DMA - Max or PnP (same channel as GF1 recording)
	 	- (DMA1) CS4231 recording DMA - Max or PnP
		FIXME: ?support GUS daughterboards? (CS4231 with shared playback/record DMA)
	 	 */

		/* Configure codec.
		 Note: Done BEFORE gus reset so it does not have to be synchronized and ... stuff
		 */
		if (board_revision >= GF1REVISION_MAX)
			{
			codec_init();
			}

		// Reset GUS
		if (!reset_gus())
			return STATUS_DEVICE_CONFIGURATION_ERROR;

		// Start checks
		interrupt_sync->CallSynchronizedRoutine (&init_check, this);
		// Start DMA transfer (can't be done in synchronized routine)
		if (!check_dma_irq)
			check_dma_irq = (BYTE *) ExAllocatePool (NonPagedPool, ddma_alignment + 1024);
		if (check_dma_irq)
			{
			BYTE *aligned;

			aligned = (BYTE *) (((ULONG_PTR) check_dma_irq + ddma_alignment) & ~ddma_alignment);
			for (int i = 0; i < 1024; i++)
				aligned[i] = (BYTE) (i & 0xff);
			ddma_download_nonpaged (0, aligned, 1024, GF1DMA_UNSIGNED, NULL, NULL);
			}

		// Wait a bit (100ms should be enough, but...)
		t = PcGetTimeInterval (0);
		d.QuadPart = -50 * 10000;
		while (PcGetTimeInterval (t) < GTI_MILLISECONDS (250))
			KeDelayExecutionThread (KernelMode, FALSE, &d);

		if (dread (GF1R_IRQ_STATUS))
			GF1_DBGINT (L"InterruptPendingInInit", 1);

		// Every test passed?
		if (!check_wave_irq && !check_ramp_irq && !check_midi_irq &&
		    !check_timer_irq && !check_dma_irq)
			{
			// Seems ok, check if dma has been successful
			#if GF1_DBG

			ULONG errors = 0;

			for (ULONG i = 0; i < 256; i++)
				{
				if ((ULONG) (peek (i) ^ 0x80) != i)
					errors++;
				}
			GF1_DBGINT (L"DramDmaCheckErrors", errors);

			#endif // GF1_DBG

			break;
			}

		// One of those checks failed -> try again (hope multiple resets are safe)
		// Clean before reinit
		close_dram_dma();
		}

	debug_to_registry (L"CGF1Common::init");

	if (i == 10)
		{
		GF1_INFSTR (L"ErrorMessage", L"Initial checks failed");

		dwrite (GF1R_VSELECT, 0);

		GF1_DBGINT (L"FailedInitVoiceControl", iread8 (GF1REGV_VOICE_CTRL));
		GF1_DBGINT (L"FailedInitRampControl", iread8 (GF1REGV_RAMP_CTRL));
		GF1_DBGINT (L"FailedInitIrqStatus", dread (GF1R_IRQ_STATUS));
		GF1_DBGINT (L"FailedInitVoiceIrqStatus", iread8 (GF1REGV_VIRQ));
		GF1_DBGINT (L"FailedInitReset", iread8 (GF1REG_RESET));
		GF1_DBGINT (L"FailedInitDmaControl", iread8 (GF1REG_DMA));
		GF1_DBGINT (L"FailedInitDmaOffset", ddma_offset);
		GF1_DBGINT (L"FailedInitDmaSending", ddma_sending);

		iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		GF1_SELFMOD(this)
			iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
		GF1_SELFMOD_END

		return STATUS_DEVICE_CONFIGURATION_ERROR;
		}

	GF1_DBGINT (L"ResetSucceededAfterSeveralTries", i);
	}//eob (reset)

	// Checks passed - we don't need DRAM DMA right now
	ddma_enabled = FALSE;

	GF1_DBGSTR (L"Status", L"GF1 reset done");

	// From now on, every access to GF1 registers must be synchronized with 'interrupt_sync'

	return STATUS_SUCCESS;
	}
