/*
 * dramdma.cpp
 *
 * GUS DRAM DMA management
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


/* BIG NOTE:
 This module simply uses DRAM DMA. It doesn't care about DMA sharing. This must
 be done at higher level, otherwise something will fail.
 */

/* Some DMA notes from DDK so I don't have to read it over and over again:
 - prepare MDL:
   a) common buffer: (required only if autoinit is enabled?)
	  - AllocateCommonBuffer (free by FreeCommonBuffer)
	  - IoAllocateMdl  (free by IoFreeMdl)
	  - MmBuildMdlForNonPagedPool
   b) user memory:
	  - IoAllocateMdl (use user-mode ptrs; free by IoFreeMdl)
	  - MmProbeAndLockPages (in __try / __except block; free by MmUnlockPages)
   c) nonpaged kernel memory (probably):
	  - IoAllocateMdl (free by IoFreeMdl)
	  - MmBuildMdlForNonPagedPool
   (note: I don't really understand if b) and c) work, because these buffers
    probably won't be allocated from "low" memory)
 - KeFlushIoBuffers
 - AllocateAdapterChannel
   - get virtual address for MDL (MmGetMdlVirtualAddress)
   - MapTransfer
   - program device
   - return KeepObject
 ...
 - FlushAdapterBuffers
 - FreeAdapterChannel
 - free MDL

 More notes:
 - Use MmGetSystemAddressForMdl (NOT MmGetSystemAddressForMdlSafe) to access MDL buffer
 - See ddksynth/kernhelp.cpp to see how to do "safe" MmGetSystemAddressForMdl
 */


#pragma code_seg()


/* Initialize DRAM DMA stuff
 */
NTSTATUS CGF1Common::init_dram_dma
	(
	void
	)

	{
	KIRQL	old_irql;

	// Get DMA adapter
	{
	DEVICE_DESCRIPTION					ddesc;
	INTERFACE_TYPE						iface;
	ULONG								res_length;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR		dr;

	RtlZeroMemory (&ddesc, sizeof (ddesc));
	if (!NT_SUCCESS (IoGetDeviceProperty (physical_device_object, DevicePropertyLegacyBusType,
	    sizeof (iface), (PVOID) &iface, &res_length)))
		{
		// Probably could be either Isa or PNPISABus, I don't know if it really matters
		iface = Isa;
		}
	dr = dma_resources->FindTranslatedDma(0);
	if (dr->u.Dma.Channel != dram_dma)
		{
		GF1_INFSTR (L"ErrorMessage", L"Invalid DMA resource passed to init_dram_dma()");
		return STATUS_INVALID_PARAMETER;
		}

  	ddesc.Version			= DEVICE_DESCRIPTION_VERSION;
	ddesc.Master			= FALSE;
	ddesc.ScatterGather		= FALSE;
	ddesc.DemandMode		= TRUE;		// FIXME: who REALLY knows what this means?
  	ddesc.AutoInitialize	= FALSE;	// FIXME?
 	ddesc.Dma32BitAddresses	= FALSE;
	ddesc.IgnoreCount		= FALSE;
	ddesc.Dma64BitAddresses	= FALSE;
	//ddesc.BusNumber;
	ddesc.DmaChannel		= dr->u.Dma.Channel;
	ddesc.InterfaceType		= iface;
	ddesc.DmaWidth			= (dr->Flags & CM_RESOURCE_DMA_8) ? Width8Bits : Width16Bits;
	ddesc.DmaSpeed			= (dr->Flags & CM_RESOURCE_DMA_TYPE_A) ? TypeA :
							  (dr->Flags & CM_RESOURCE_DMA_TYPE_B) ? TypeB : Compatible;
	ddesc.MaximumLength		= 0x10000;	// Yep?
	ddesc.DmaPort			= dr->u.Dma.Port;

	ddma_max_map_regs = 0x10000 / PAGE_SIZE + 1;
	ddma_adapter = IoGetDmaAdapter (physical_device_object, &ddesc, &ddma_max_map_regs);
	if (!ddma_adapter)
		{
		GF1_INFSTR (L"ErrorMessage", L"Can't allocate DMA adapter");
		return STATUS_UNSUCCESSFUL;
		}
	ddma_alignment = ddma_adapter->DmaOperations->GetDmaAlignment (ddma_adapter);

	GF1_DBGINT (L"DramDmaMapRegisters", ddma_max_map_regs);
	GF1_DBGINT (L"DramDmaMapAlignment", ddma_alignment);

	#if GF1_DBG
	ddma_transferred = 0;
	#endif /* GF1_DBG */
	}//eob (get DMA adapter)

	// Initialize DMA structures
	ddma_first = -1;
	ddma_last = -1;
	ddma_enabled = FALSE;
	ddma_busy = FALSE;

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Disable DMA IRQs
 synchronized
 */
static NTSTATUS cdd_disable_dma_irq
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	CGF1Common 		* self;

	self = (CGF1Common *) context;
	self->iwrite8 (GF1REG_DMA, self->iread8 (GF1REG_DMA) & ~GF1DMA_IRQ_ENABLE);

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Stop DMA
 synchronized
 */
static NTSTATUS cdd_stop_dma
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	CGF1Common 		* self;

	self = (CGF1Common *) context;
	self->iwrite8 (GF1REG_DMA, 0);

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* Free DRAM DMA structures
 */
void CGF1Common::close_dram_dma
	(
	void
	)

	{
	KIRQL	old_irql;
	ULONG	first, last;

	if (!ddma_adapter)
		return;

	KeAcquireSpinLock (&ddma_lock, &old_irql);

	first = ddma_first;
	last = ddma_last;
	ddma_first = ddma_last = -1;
	ddma_enabled = FALSE;
	ddma_busy = FALSE;

	if (first != -1)
		{
		GF1DDRequest *req;
		ULONG i;

		req = &ddma_request[first];
		i = first;

		// Disable DMA interrupts
		if (interrupt_sync)
			interrupt_sync->CallSynchronizedRoutine (&cdd_disable_dma_irq, this);
		else
			cdd_disable_dma_irq (NULL, this);

		// Free DMA if it's allocated
		if (ddma_reg_base)
			{
			// Flush buffers
			ddma_adapter->DmaOperations->FlushAdapterBuffers (ddma_adapter, req->mdl,
				ddma_reg_base, ddma_currentva, ddma_sending, TRUE);

			// Stop transfer
			if (interrupt_sync)
				interrupt_sync->CallSynchronizedRoutine (&cdd_stop_dma, this);
			else
				cdd_stop_dma (NULL, this);

			// Free adapter channel
			ddma_adapter->DmaOperations->FreeAdapterChannel (ddma_adapter);

			ddma_reg_base = NULL;
			}

		// Free all pending requests
		while (1)
			{
			switch (req->memory_type)
				{
			case GF1DDMA_TYPE_NONPAGED:
				// Decrement counter if ptr is valid
				if (req->counter)
					{
					InterlockedDecrement (req->counter);
					}

				// Signal event if valid
				if (req->event)
					{
					KeSetEvent (req->event, 0, FALSE);
					}

				// Free MDL
				if (req->mdl)
					{
					IoFreeMdl (req->mdl);
					req->mdl = NULL;
					}
				break;
				}

			// Next request
			if (i == last)
				break;
			i = (i + 1) % GF1DDMA_REQUESTS;
			req = &ddma_request[i];
			}
		}

	// Free DMA adapter
	ddma_adapter->DmaOperations->PutDmaAdapter (ddma_adapter);
	ddma_adapter = NULL;

	KeReleaseSpinLock (&ddma_lock, old_irql);
	}


#pragma code_seg()


/* DMA terminal count DPC
 */
void CGF1Common::ddma_tc_dpc
	(
	IN PKDPC,
	IN PVOID	context,
	IN PVOID,
	IN PVOID
	)

	{
	CGF1Common *	self;
	GF1DDRequest *	req;
	BOOLEAN			send;

	self = (CGF1Common *) context;
	if (!self || !self->ddma_adapter || !self->ddma_reg_base)
		return;

	ASSERT (KeGetCurrentIrql() == DISPATCH_LEVEL);
	KeAcquireSpinLockAtDpcLevel (&self->ddma_lock);

	// Get request
	if (self->ddma_first == -1)
		{
		KeReleaseSpinLockFromDpcLevel (&self->ddma_lock);
		return;
		}
	req = &self->ddma_request[self->ddma_first];

	// Finish transfer
	self->ddma_adapter->DmaOperations->FlushAdapterBuffers (self->ddma_adapter,
		req->mdl, self->ddma_reg_base, self->ddma_currentva, self->ddma_sending, TRUE);
	self->ddma_reg_base = NULL;
	self->ddma_adapter->DmaOperations->FreeAdapterChannel (self->ddma_adapter);
	self->ddma_busy = FALSE;

	// Update pointers
	self->ddma_offset += self->ddma_sending;
	self->ddma_currentva = (BYTE *) self->ddma_currentva + self->ddma_sending;
	#if GF1_DBG
	self->ddma_transferred += self->ddma_sending;
	#endif /* GF1_DBG */

	if (self->ddma_offset >= req->length)
		{
		// Transfer done

		// Decrement counter if ptr is valid
		if (req->counter)
			{
			InterlockedDecrement (req->counter);
			}

		// Signal event if valid
		if (req->event)
			{
			KeSetEvent (req->event, 0, FALSE);
			}

		// (initial dma check thingy)
		if (self->check_dma_irq)
			{
			// Check OK
			ExFreePool (self->check_dma_irq);
			self->check_dma_irq = NULL;
			}

		// Free stuff
		IoFreeMdl (req->mdl);
		req->mdl = NULL;
		req->address = NULL;
		req->length = 0;

		if (self->ddma_first == self->ddma_last)
			{
			// No more requests
			self->ddma_first = self->ddma_last = -1;
			send = FALSE;
			}
		else
			{
			// Next request
			self->ddma_first = (self->ddma_first + 1) % GF1DDMA_REQUESTS;
			send = TRUE;
			}
		}
	else
		{
		// Another block of current request
		send = TRUE;
		}

	if (!self->ddma_enabled)
		send = FALSE;

	KeReleaseSpinLockFromDpcLevel (&self->ddma_lock);

	if (send)
		self->ddma_send();
	}


#pragma code_seg()


struct DDmaAdapterCtrlContext
	{
	CGF1Common *	self;
	BYTE			dma_ctrl;
	WORD			dest;
	};


/* Start DMA transfer
 synchronized
 */
static NTSTATUS ac_start_gf1_dma
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	DDmaAdapterCtrlContext	*ctx;

	ctx = (DDmaAdapterCtrlContext *) context;

	ctx->self->iwrite8 (GF1REG_DMA, 0);
	ctx->self->iwrite8 (GF1REG_DMA, ctx->dma_ctrl & ~GF1DMA_ENABLE);
	ctx->self->iwrite16 (GF1REG_DMA_ADDR, ctx->dest);
	ctx->self->iwrite8 (GF1REG_DMA, ctx->dma_ctrl);

	return STATUS_SUCCESS;
	};


#pragma code_seg()


/* Adapter control callback - start DMA transfer
 */
IO_ALLOCATION_ACTION CGF1Common::ddma_adapter_control
	(
	IN PDEVICE_OBJECT,
	IN PIRP,
	IN PVOID            register_base,
	IN PVOID			context
	)

	{
	/* Note:
	 I assume that if adapter object is deallocated, this routing won't be called.
	 */
	CGF1Common *			self;
	KIRQL					old_irql;
	GF1DDRequest *			req;
	DDmaAdapterCtrlContext	ctx;

	self = (CGF1Common *) context;
	self->ddma_reg_base = register_base;

	// Acquire queue lock (FIXME: is this routine called at DISPATCH_LEVEL?)
	KeAcquireSpinLock (&self->ddma_lock, &old_irql);

	// Queue still valid?
	if (self->ddma_first == -1)
		{
		// No - bail out
		KeReleaseSpinLock (&self->ddma_lock, old_irql);
		// FIXME: shedule dummy "DMA transfer completed" call
		return DeallocateObject;
		}

	// Map transfer
	req = &self->ddma_request[self->ddma_first];
	self->ddma_adapter->DmaOperations->MapTransfer (self->ddma_adapter, req->mdl,
		register_base, self->ddma_currentva, &self->ddma_sending, TRUE);

	// Start transfer (FIXME: another addressing on interwave)
	ctx.self = self;
	ctx.dma_ctrl = req->dma_flags | GF1DMA_ENABLE | GF1DMA_IRQ_ENABLE | GF1DMA_DEFAULT_RATE;
	if (self->dram_dma < 4)	// FIXME: can't use 'dma_resources' because we're not at PASSIVE_LEVEL
		{
		ctx.dest = (WORD) (GF1_8BIT_ADDR (req->dram_address + self->ddma_offset) >> 4);
		}
	else
		{
		ctx.dma_ctrl |= GF1DMA_DMA16;
		ctx.dest = (WORD) (GF1_16BIT_ADDR (req->dram_address + self->ddma_offset) >> 4);
		}

	self->interrupt_sync->CallSynchronizedRoutine (&ac_start_gf1_dma, &ctx);

	KeReleaseSpinLock (&self->ddma_lock, old_irql);

	return KeepObject;
	}


#pragma code_seg()


/* Send next part of current request
 N: first request must be valid, DRAM DMA must be stopped
 */
void CGF1Common::ddma_send
	(
	void
	)

	{
	KIRQL			old_irql;
	GF1DDRequest *	req;
	ULONG			regs;

	KeAcquireSpinLock (&ddma_lock, &old_irql);

	if (ddma_busy)
		{
		KeReleaseSpinLock (&ddma_lock, old_irql);
		if (old_irql == PASSIVE_LEVEL)
			GF1_INFSTR (L"DDmaWarning", L"DRAM DMA is busy in ddma_send()");
		return;
		}

	if (ddma_first == -1)
		{
		KeReleaseSpinLock (&ddma_lock, old_irql);
		if (old_irql == PASSIVE_LEVEL)
			GF1_INFSTR (L"DDmaWarning", L"No requests in ddma_send()");
		return;
		}

	while (1)
		{
		// Get request
		req = &ddma_request[ddma_first];

		if (req->mdl)
			{
			// Valid MDL - use it
			break;
			}

		// New request - allocate MDL and stuff
		req->mdl = IoAllocateMdl (req->address, req->length, FALSE, FALSE, NULL);
		if (req->mdl)
			{
			// Prepare MDL and use this buffer
			MmBuildMdlForNonPagedPool (req->mdl);
			ddma_offset = 0;
			ddma_currentva = MmGetMdlVirtualAddress (req->mdl);
			break;
			}

		// Can't allocate MDL - usually should not happen - just forget this request
		if (ddma_first == ddma_last)
			{
			// All requests failed
			ddma_first = ddma_last = -1;
			KeReleaseSpinLock (&ddma_lock, old_irql);
			if (old_irql == PASSIVE_LEVEL)
				GF1_INFSTR (L"DDmaWarning", L"Can't allocate MDL");
			return;
			}

		// Try next request
		ddma_first = (ddma_first + 1) % GF1DDMA_REQUESTS;
		}

	ddma_busy = TRUE;

	// How much bytes can we transfer?
	ddma_sending = req->length - ddma_offset;

	regs = ADDRESS_AND_SIZE_TO_SPAN_PAGES (ddma_currentva, ddma_sending);
	if (regs > ddma_max_map_regs)
		{
		// Too many map registers used
		ddma_sending = (ddma_max_map_regs - 1) * PAGE_SIZE;
		}

	{
	ULONG spage, epage;

	spage = (req->dram_address + ddma_offset) >> GF1MEM_PAGE_SHIFT;
	epage = (req->dram_address + ddma_offset + ddma_sending - 1) >> GF1MEM_PAGE_SHIFT;
	if (spage != epage)
		{
		// Request crosses DRAM page boundaries
		ddma_sending = ((spage + 1) << GF1MEM_PAGE_SHIFT) - (req->dram_address + ddma_offset);
		}
	}//eob (page boundary)

	// Align it
	ddma_sending &= ~GF1MEM_ALIGNMENT;

	// Recompute number of required map registers
	regs = ADDRESS_AND_SIZE_TO_SPAN_PAGES (ddma_currentva, ddma_sending);

	// Flush caches
	KeFlushIoBuffers (req->mdl, FALSE, TRUE);

	/*KeReleaseSpinLock (&ddma_lock, old_irql);

	// Allocate channel
	if (old_irql < DISPATCH_LEVEL)
		KeRaiseIrql (DISPATCH_LEVEL, &old_irql);
	if (!NT_SUCCESS (ddma_adapter->DmaOperations->AllocateAdapterChannel (ddma_adapter,
	    device_object, regs, &ddma_adapter_control, this)))
		{
		// This really should not happen - it's bug in driver
		if (old_irql < DISPATCH_LEVEL)
			{
			KeLowerIrql (old_irql);
			// (can't access registry at >passive_level)
			if (old_irql == PASSIVE_LEVEL)
				{
				GF1_INFSTR (L"DDmaWarning", L"AllocateAdapterChannel failed");
				}
			}

		KeAcquireSpinLock (&ddma_lock, &old_irql);
		IoFreeMdl (req->mdl);
		ddma_first = ddma_last = -1;	// FIXME: could work now - all mdls should be NULL
		KeReleaseSpinLock (&ddma_lock, old_irql);
		}

	if (old_irql < DISPATCH_LEVEL)
		KeLowerIrql (old_irql);*/

	// Allocate channel
	if (!NT_SUCCESS (ddma_adapter->DmaOperations->AllocateAdapterChannel (ddma_adapter,
	    device_object, regs, &ddma_adapter_control, this)))
		{
		// This really should not happen - it's bug in driver
		if (old_irql == PASSIVE_LEVEL)
			{
			GF1_INFSTR (L"DDmaWarning", L"AllocateAdapterChannel failed");
			}

		IoFreeMdl (req->mdl);
		ddma_first = ddma_last = -1;	// FIXME: could work now - all mdls should be NULL
		ddma_busy = FALSE;
		}

	KeReleaseSpinLock (&ddma_lock, old_irql);
	}


#pragma code_seg()


/* Schedule DRAM download
 */
BOOLEAN CGF1Common::ddma_download
	(
	IN ULONG		dram_address,
	IN PVOID		src_address,
	IN ULONG		length,
	IN ULONG		flags,
	IN PLONG		counter			OPTIONAL,
	IN PRKEVENT		event			OPTIONAL,
	IN ULONG		type
	)

	{
	KIRQL			old_irql;
	GF1DDRequest *	req;
	BOOLEAN			send;

	KeAcquireSpinLock (&ddma_lock, &old_irql);

	if (!ddma_enabled)
		{
		KeReleaseSpinLock (&ddma_lock, old_irql);

		if (KeGetCurrentIrql() == PASSIVE_LEVEL)
			{
			GF1_INFSTR (L"DDmaWarning", L"DRAM DMA not enabled");
			}

		return FALSE;
		}

	if (ddma_first == -1)
		{
		ddma_first = ddma_last = 0;
		}
	else
		{
		if ((ddma_last + 1) % GF1DDMA_REQUESTS == ddma_first)
			{
			// Too many requests (not "ErrorMessage" - actually this is kind of warning)
			KeReleaseSpinLock (&ddma_lock, old_irql);

			if (KeGetCurrentIrql() == PASSIVE_LEVEL)
				{
				GF1_INFSTR (L"DDmaWarning", L"DRAM DMA request overflowed");
				}

			return FALSE;
			}
		ddma_last = (ddma_last + 1) % GF1DDMA_REQUESTS;
		}

	// Store request
	req = &ddma_request[ddma_last];
	req->dram_address				= dram_address;
	req->address		  			= src_address;
	req->mdl						= NULL;
	req->length						= length;
	req->counter					= counter;
	req->event						= event;
	req->dma_flags					= (BYTE) (flags & (GF1DMA_DATA16 | GF1DMA_UNSIGNED));
	req->memory_type				= (BYTE) type;

	// If this is the only request, send it
	if (ddma_first == ddma_last)
		send = TRUE;
	else
		send = FALSE;

	KeReleaseSpinLock (&ddma_lock, old_irql);

	if (send)
		{
		// We have the first buffer and there's no DMA transfer, so no one else can call this
		ddma_send();
		}

	return TRUE;
	}


#pragma code_seg()


/* Get DRAM DMA alignment
 */
ULONG CGF1Common::ddma_get_alignment
	(
	void
	)

	{
	return ddma_alignment;
	}


#pragma code_seg()


/* Schedule DRAM download from nonpaged memory
 */
BOOLEAN CGF1Common::ddma_download_nonpaged
	(
	IN ULONG		dram_address,
	IN PVOID		src_address,
	IN ULONG		length,
	IN ULONG		flags,
	IN PLONG		counter			OPTIONAL,
	IN PRKEVENT		event			OPTIONAL
	)

	{
	ASSERT (src_address);
	if (dram_address & GF1MEM_ALIGNMENT || length & GF1MEM_ALIGNMENT ||
	    ((ULONG_PTR) src_address) & ddma_alignment)
		{
		GF1_INFSTR (L"DDmaWarning", L"NonPaged download isn't aligned");
		return FALSE;
		}

	return ddma_download (dram_address, src_address, length, flags, counter,
		event, GF1DDMA_TYPE_NONPAGED);
	}
