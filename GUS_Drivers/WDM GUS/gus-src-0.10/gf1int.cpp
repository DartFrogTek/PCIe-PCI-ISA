/*
 * gf1int.cpp
 *
 * Interrupt stuff
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


#pragma code_seg()


/* Generic interrupt service routine
 Note: We must do a lot of work to acknowledge all interrupts. Even indirect registers are
 accessed, which means that _EVERY_ access to GF1 registers must be synchronized with this ISR.
 */
NTSTATUS CGF1Common::isr
	(
	IN PINTERRUPTSYNC	isync,
	IN PVOID			context
	)

	{
	CGF1Common *	self;
	BYTE			irq_status;
	BYTE			irq_status_mask;
	ULONG			wave_mask			= 0;
	ULONG			vol_mask			= 0;
	ULONG			wave_fired			= 0;
	ULONG			vol_fired			= 0;
	BOOLEAN			timer_fired			= FALSE;

	ASSERT (isync);
	ASSERT (context);

	self = (CGF1Common *) context;
	if (!self || !(self->dreg_to_port_table[0]))
		return STATUS_UNSUCCESSFUL;

	irq_status_mask = GF1IRQS_TIMER_MASK | GF1IRQS_DMA | GF1IRQS_WAVE | GF1IRQS_VOLUME;
	if (self->gf1_irq == self->midi_irq)
		irq_status_mask |= GF1IRQS_MIDI_XMIT | GF1IRQS_MIDI_RECV;

	irq_status = self->dread (GF1R_IRQ_STATUS);

	while (irq_status & irq_status_mask)
		{
		if ((irq_status & irq_status_mask) & GF1IRQS_MIDI_XMIT)
			{
			// MIDI transmit - ready to send
			if (self->check_midi_irq)
				{
				self->check_midi_irq = FALSE;
				self->dwrite (GF1R_MIDI_CTRL, 0);
				}
			else
				{
				// FIXME: inform MIDI UART miniport
				self->dwrite (GF1R_MIDI_CTRL, 0);
				}
			#if GF1_DBG
			self->xmit_irqs_handled++;
			#endif
			}

		if ((irq_status & irq_status_mask) & GF1IRQS_MIDI_RECV)
			{
			// MIDI receive - something to read
			// FIXME: do not discard this
			self->dread (GF1R_MIDI_DATA);
			#if GF1_DBG
			self->recv_irqs_handled++;
			#endif
			}

		if (irq_status & GF1IRQS_TIMER_MASK)
			{
			// FIXME: Both timers can't be used at the same time right now
			// Timer interrupt
			if (irq_status & GF1IRQS_TIMER1)
				{
				BYTE tctl = self->iread8 (GF1REG_TIMER);
				self->iwrite8 (GF1REG_TIMER, tctl & ~GF1TIMER_IRQ1);
				self->iwrite8 (GF1REG_TIMER, tctl);
				}

			if (irq_status & GF1IRQS_TIMER2)
				{
				BYTE tctl = self->iread8 (GF1REG_TIMER);
				self->iwrite8 (GF1REG_TIMER, tctl & ~GF1TIMER_IRQ2);
				self->iwrite8 (GF1REG_TIMER, tctl);
				}

			if (self->gf1timer_ack_handler)
				{
				// We have handler - call it
				self->gf1timer_ack_handler (self->gf1timer_context);
				}
			else
				{
				// Otherwise stop timer
				self->iwrite8 (GF1REG_TIMER, 0);
				self->dwrite (GF1R_TIMER_CTRL, GF1TCTL_WRITETHIS);
				self->dwrite (GF1R_TIMER_DATA, GF1TDATA_CLEAR_IRQ);
				self->dwrite (GF1R_TIMER_DATA, GF1TDATA_T1MASK | GF1TDATA_T2MASK);
				self->iwrite8 (GF1REG_COUNT1, 0);
				self->iwrite8 (GF1REG_COUNT2, 0);
				}
			timer_fired = TRUE;

			if (self->check_timer_irq)
				self->check_timer_irq = FALSE;

			#if GF1_DBG
			self->timer_irqs_handled++;
			#endif
			}

		if (irq_status & GF1IRQS_DMA)
			{
			if (self->iread8 (GF1REG_DMA) & GF1DMA_IRQ_PENDING)
				{
				// DRAM DMA TC
				self->iwrite8 (GF1REG_DMA, 0);

				// Schedule DMA DPC
				KeInsertQueueDpc (&self->ddma_dpc, NULL, NULL);

				#if GF1_DBG
				self->ddma_irqs_handled++;
				#endif
				}

			if (self->iread8 (GF1REG_SMP_CTRL) & GF1SMP_IRQ_PENDING)
				{
				// Recording DMA TC
				// FIXME
				self->iwrite8 (GF1REG_SMP_CTRL, 0);

				#if GF1_DBG
				self->rdma_irqs_handled++;
				#endif
				}
			}

		if (irq_status & (GF1IRQS_WAVE | GF1IRQS_VOLUME))
			{
			// Voice IRQ
			BYTE	virq_status;
			BYTE	voice;

			while (1)
				{
				virq_status = self->iread8 (GF1REGV_VIRQ);
				if ((virq_status & GF1VIRQ_NOT_PENDING) == GF1VIRQ_NOT_PENDING)
					{
					// No more voices to service
					break;
					}
				voice = (BYTE) (virq_status & GF1VIRQ_VOICE_MASK);

				if (!(virq_status & GF1VIRQ_NOT_WAVE))
					{
					// Voice position IRQ
					if (wave_mask & (1 << voice))
						{
						wave_mask &= ~(1 << voice);
						}
					else
						{
						wave_mask |= 1 << voice;
						wave_fired |= 1 << voice;

						if (self->wavetable_ack_handler[voice])
							{
							// We have handler
							self->wavetable_ack_handler[voice] (voice, self->wavetable_context[voice]);
							}
						else
							{
							// Stop voice
							self->dwrite (GF1R_VSELECT, voice);
							self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
							GF1_SELFMOD(self)
								self->iwrite8 (GF1REGV_VOICE_CTRL, GF1VC_STOPPED | GF1VC_STOP);
							GF1_SELFMOD_END
							}

						if (voice == 0 && self->check_wave_irq)
							self->check_wave_irq = FALSE;

						#if GF1_DBG
						self->wave_irqs_handled++;
						#endif
						}
					}

				if (!(virq_status & GF1VIRQ_NOT_VOLUME))
					{
					// Volume ramp IRQ
					if (vol_mask & (1 << voice))
						{
						vol_mask &= ~(1 << voice);
						}
					else
						{
						vol_mask |= 1 << voice;
						vol_fired |= 1 << voice;

						if (self->ramp_ack_handler[voice])
							{
							// We have handler
							self->ramp_ack_handler[voice] (voice, self->ramp_context[voice]);
							}
						else
							{
							// Stop ramp
							self->dwrite (GF1R_VSELECT, voice);

							self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
							GF1_SELFMOD(self)
								self->iwrite8 (GF1REGV_RAMP_CTRL, GF1RC_STOPPED | GF1RC_STOP);
							GF1_SELFMOD_END
							}

						if (voice == 0 && self->check_ramp_irq)
							self->check_ramp_irq = FALSE;

						#if GF1_DBG
						self->ramp_irqs_handled++;
						#endif
						}
					}
				}
			}

		irq_status = self->dread (GF1R_IRQ_STATUS);
		}

	// Check if Codec interrupted
	if (self->board_revision >= GF1REVISION_MAX)
		{
		// NOTE: Reading CR_STATUS will clear overrun and underrun bits of the Codec Status Register 2
		// (CSR2I[7:6]) and Codec Status Register 3 (CSR3I[3:0]), if any are set.
		if (self->dread (CR_STATUS) & CSTAT_IRQ)
			{
			// Get IRQ status
			BYTE	status = self->ciread8 (CREG_IRQ_STATUS);

			if (status & CIRQSTAT_PLAYBACK_IRQ)
				{
				// Playback IRQ
				self->ciwrite8 (CREG_IRQ_STATUS, ~CIRQSTAT_PLAYBACK_IRQ);
				if (self->playback_handler)
					self->playback_handler (self->playback_context);
				#if GF1_DBG
				self->cplay_irqs_handled++;
				#endif
				}

			if (status & CIRQSTAT_RECORD_IRQ)
				{
				// Record IRQ
				self->ciwrite8 (CREG_IRQ_STATUS, ~CIRQSTAT_RECORD_IRQ);
				if (self->record_handler)
					self->record_handler (self->record_context);
				#if GF1_DBG
				self->crec_irqs_handled++;
				#endif
				}

			if (status & CIRQSTAT_TIMER_IRQ)
				{
				// Timer IRQ
				self->ciwrite8 (CREG_IRQ_STATUS, ~CIRQSTAT_TIMER_IRQ);
				if (self->cstimer_handler)
					self->cstimer_handler (self->cstimer_context);
				#if GF1_DBG
				self->ctimer_irqs_handled++;
				#endif
				}
			}
		}

	// Call handlers
	{
	ULONG v;

	for (v = 0 ; v < 32; v++)
		{
		if ((wave_fired & (1 << v)) && self->wavetable_action_handler[v])
			self->wavetable_action_handler[v] (v, self->wavetable_context[v]);
		if ((vol_fired & (1 << v)) && self->ramp_action_handler[v])
			self->ramp_action_handler[v] (v, self->ramp_context[v]);
		}

	if (timer_fired && self->gf1timer_action_handler)
		self->gf1timer_action_handler (self->gf1timer_context);
	}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* MIDI interrupt service routine
 */
NTSTATUS CGF1Common::midi_isr
	(
	IN PINTERRUPTSYNC	isync,
	IN PVOID			context
	)

	{
	CGF1Common *self;
	BYTE irq_status;

	ASSERT (isync);
	ASSERT (context);

	self = (CGF1Common *) context;
	if (!self || !self->dreg_to_port_table[0])
		return STATUS_UNSUCCESSFUL;

	irq_status = self->dread (GF1R_IRQ_STATUS);

	while (irq_status & (GF1IRQS_MIDI_XMIT | GF1IRQS_MIDI_RECV))
		{
		if (irq_status & GF1IRQS_MIDI_XMIT)
			{
			// MIDI transmit - ready to send
			if (self->check_midi_irq)
				{
				self->check_midi_irq = FALSE;
				self->dwrite (GF1R_MIDI_CTRL, 0);
				}
			else
				{
				// FIXME: inform MIDI UART miniport
				self->dwrite (GF1R_MIDI_CTRL, 0);
				}

			#if GF1_DBG
			self->xmit_irqs_handled++;
			#endif
			}

		if (irq_status & GF1IRQS_MIDI_RECV)
			{
			// MIDI receive - something to read
			// FIXME: do not discard this
			self->dread (GF1R_MIDI_DATA);

			#if GF1_DBG
			self->recv_irqs_handled++;
			#endif
			}

		irq_status = self->dread (GF1R_IRQ_STATUS);
		}

	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct SHContext
	{
	CGF1Common *		self;
	ULONG				voice;
	GF1VoiceIrqFn *		ack_handler;
	GF1VoiceIrqFn *		action_handler;
	PVOID				context;
	BOOLEAN				result;
	};


/* set_wavetable_handler() synchronized stuff
 */
NTSTATUS CGF1Common::swh_synchronized
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	SHContext *		ctx = (SHContext *) context;

	if (ctx->voice > 32 || (ctx->ack_handler && ctx->self->wavetable_ack_handler[ctx->voice]) ||
		(ctx->action_handler && ctx->self->wavetable_action_handler[ctx->voice]))
		{
		ctx->result = FALSE;
		return STATUS_SUCCESS;
		}

	ctx->self->wavetable_ack_handler[ctx->voice] = ctx->ack_handler;
	ctx->self->wavetable_action_handler[ctx->voice] = ctx->action_handler;
	ctx->self->wavetable_context[ctx->voice] = ctx->context;

	ctx->result = TRUE;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


/* set_ramp_handler() synchronized stuff
 */
NTSTATUS CGF1Common::srh_synchronized
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	SHContext *		ctx = (SHContext *) context;

	if (ctx->voice > 32 || (ctx->ack_handler && ctx->self->ramp_ack_handler[ctx->voice]) ||
		(ctx->action_handler && ctx->self->ramp_action_handler[ctx->voice]))
		{
		ctx->result = FALSE;
		return STATUS_SUCCESS;
		}

	ctx->self->ramp_ack_handler[ctx->voice] = ctx->ack_handler;
	ctx->self->ramp_action_handler[ctx->voice] = ctx->action_handler;
	ctx->self->ramp_context[ctx->voice] = ctx->context;

	ctx->result = TRUE;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct STHContext
	{
	CGF1Common *		self;
	GF1TimerIrqFn *		ack_handler;
	GF1TimerIrqFn *		action_handler;
	PVOID				context;
	BOOLEAN				result;
	};


/* set_gf1timer_handler() synchronized stuff
 */
NTSTATUS CGF1Common::sth_synchronized
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	STHContext *	ctx = (STHContext *) context;

	if ((ctx->ack_handler && ctx->self->gf1timer_ack_handler) ||
		(ctx->action_handler && ctx->self->gf1timer_action_handler))
		{
		ctx->result = FALSE;
		return STATUS_SUCCESS;
		}

	ctx->self->gf1timer_ack_handler = ctx->ack_handler;
	ctx->self->gf1timer_action_handler = ctx->action_handler;
	ctx->self->gf1timer_context = ctx->context;

	ctx->result = TRUE;
	return STATUS_SUCCESS;
	}


#pragma code_seg()


struct SCHContext
	{
	CGF1Common *		self;
	GF1CodecIrqFn *		handler;
	PVOID				context;
	ULONG				index;	// 0 = playback, 1 = record, 2 = timer
	BOOLEAN				result;
	};


/* Set codec IRQ handler synchronized stuff
 */
NTSTATUS CGF1Common::sch_synchronized
	(
	IN PINTERRUPTSYNC,
	IN PVOID			context
	)

	{
	SCHContext *		ctx = (SCHContext *) context;
	GF1CodecIrqFn **	th;
	PVOID *				tc;

	th = (ctx->index == 0) ? &ctx->self->playback_handler : (ctx->index == 1) ?
		&ctx->self->record_handler : &ctx->self->cstimer_handler;
	tc = (ctx->index == 0) ? &ctx->self->playback_context : (ctx->index == 1) ?
		&ctx->self->record_context : &ctx->self->cstimer_context;

	if (ctx->handler && *th)
		{
		ctx->result = FALSE;
		return STATUS_SUCCESS;
		}

	*th = ctx->handler;
	*tc = ctx->context;

	ctx->result = TRUE;
	return STATUS_SUCCESS;
	}


#pragma code_seg ()


/* Call routine synchronized with interrupts
 */
void CGF1Common::call_synchronized
	(
	IN PINTERRUPTSYNCROUTINE	routine,
	IN PVOID					context
	)

	{
	ASSERT (routine);

	if (interrupt_sync)
		interrupt_sync->CallSynchronizedRoutine (routine, context);
	else
		routine (NULL, context);
	}


#pragma code_seg ("PAGE")


/* Install wavetable IRQ handler
 */
BOOLEAN CGF1Common::set_wavetable_handler
	(
	IN ULONG			voice,
	IN GF1VoiceIrqFn *	ack_handler,
	IN GF1VoiceIrqFn *	action_handler,
	IN PVOID			context
	)

	{
	SHContext ctx;

	ctx.self = this;
	ctx.voice = voice;
	ctx.ack_handler = ack_handler;
	ctx.action_handler = action_handler;
	ctx.context = context;
	interrupt_sync->CallSynchronizedRoutine (&swh_synchronized, &ctx);

	return ctx.result;
	}


#pragma code_seg ("PAGE")


/* Install volume ramp IRQ handler
 */
BOOLEAN CGF1Common::set_ramp_handler
	(
	IN ULONG			voice,
	IN GF1VoiceIrqFn *	ack_handler,
	IN GF1VoiceIrqFn *	action_handler,
	IN PVOID			context
	)

	{
	SHContext ctx;

	ctx.self = this;
	ctx.voice = voice;
	ctx.ack_handler = ack_handler;
	ctx.action_handler = action_handler;
	ctx.context = context;
	interrupt_sync->CallSynchronizedRoutine (&srh_synchronized, &ctx);

	return ctx.result;
	}


#pragma code_seg ("PAGE")


/* Install AdLib/GF1 timer IRQ handler
 */
BOOLEAN CGF1Common::set_gf1timer_handler
	(
	IN GF1TimerIrqFn *	ack_handler,
	IN GF1TimerIrqFn *	action_handler,
	IN PVOID			context
	)

	{
	STHContext ctx;

	ctx.self = this;
	ctx.ack_handler = ack_handler;
	ctx.action_handler = action_handler;
	ctx.context = context;
	interrupt_sync->CallSynchronizedRoutine (&srh_synchronized, &ctx);

	return ctx.result;
	}


#pragma code_seg ("PAGE")


/* Install CS4231 playback IRQ handler
 */
BOOLEAN CGF1Common::set_playback_handler
	(
	IN GF1CodecIrqFn *	handler,
	IN PVOID			context
	)

	{
	SCHContext ctx;

	ctx.self = this;
	ctx.handler = handler;
	ctx.context = context;
	ctx.index = 0;
	interrupt_sync->CallSynchronizedRoutine (&sch_synchronized, &ctx);

	return ctx.result;
	}


#pragma code_seg ("PAGE")


/* Install CS4231 record IRQ handler
 */
BOOLEAN CGF1Common::set_record_handler
	(
	IN GF1CodecIrqFn *	handler,
	IN PVOID			context
	)

	{
	SCHContext ctx;

	ctx.self = this;
	ctx.handler = handler;
	ctx.context = context;
	ctx.index = 1;
	interrupt_sync->CallSynchronizedRoutine (&sch_synchronized, &ctx);

	return ctx.result;
	}


#pragma code_seg ("PAGE")


/* Install CS4231 timer IRQ handler
 */
BOOLEAN CGF1Common::set_cstimer_handler
	(
	IN GF1CodecIrqFn *	handler,
	IN PVOID			context
	)

	{
	SCHContext ctx;

	ctx.self = this;
	ctx.handler = handler;
	ctx.context = context;
	ctx.index = 2;
	interrupt_sync->CallSynchronizedRoutine (&sch_synchronized, &ctx);

	return ctx.result;
	}
