/*
 * registry.cpp
 *
 * GF1 common class registry stuff
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


#pragma code_seg ("PAGE")


/* Delete registry subkey
 */
NTSTATUS CGF1Common::delete_registry_key
	(
	IN PCWSTR	subkey
	)

	{
	PAGED_CODE();

	NTSTATUS		status;
    PREGISTRYKEY    device_key;

	ASSERT (subkey);

	// Open device key
	status = PcNewRegistryKey (&device_key, NULL, DeviceRegistryKey,
		KEY_ALL_ACCESS, device_object, NULL, NULL, 0, NULL);

	if (NT_SUCCESS (status))
		{
		UNICODE_STRING	uni_name;
		PREGISTRYKEY	sub_key		= NULL;

		// Make subkey name string
		RtlInitUnicodeString (&uni_name, subkey);

		// Open subkey
		status = device_key->NewSubKey (&sub_key, NULL, KEY_ALL_ACCESS, &uni_name,
			REG_OPTION_NON_VOLATILE, NULL);

		if (NT_SUCCESS (status))
			{
			// Delete it
			sub_key->DeleteKey();
			sub_key->Release();
			}

		device_key->Release();
		}

	return status;
	}


#pragma code_seg ("PAGE")


/* Read something from registry
 */
NTSTATUS CGF1Common::read_registry
	(
	OUT PKEY_VALUE_PARTIAL_INFORMATION *	info,
	IN PCWSTR								subkey	OPTIONAL,
	IN PCWSTR								value_name,
	IN ULONG								value_length
	)

	{
	PAGED_CODE();

	NTSTATUS		status;
    PREGISTRYKEY    device_key;

	ASSERT (info);
	ASSERT (value_name);
	*info = NULL;

	// Open device key
	status = PcNewRegistryKey (&device_key, NULL, DeviceRegistryKey,
		KEY_ALL_ACCESS, device_object, NULL, NULL, 0, NULL);

	if (NT_SUCCESS (status))
		{
		UNICODE_STRING	uni_name;
		PREGISTRYKEY	sub_key		= NULL;

		if (subkey)
			{
			// Make subkey name string
			RtlInitUnicodeString (&uni_name, subkey);

			// Open subkey
			status = device_key->NewSubKey (&sub_key, NULL, KEY_ALL_ACCESS, &uni_name,
				REG_OPTION_NON_VOLATILE, NULL);
			}

		// Allocate data for key info
		*info = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePool (PagedPool,
			sizeof (KEY_VALUE_PARTIAL_INFORMATION) + value_length);
		if (!*info)
			status = STATUS_NO_MEMORY; // FIXME: Is this right status?

		if (NT_SUCCESS (status))
			{
			ULONG result_length;

			// Make value name string
			RtlInitUnicodeString (&uni_name, value_name);

			// Read data
			status = (sub_key ? sub_key : device_key)->QueryValueKey (&uni_name,
				KeyValuePartialInformation, *info, sizeof (KEY_VALUE_PARTIAL_INFORMATION) +
				value_length, &result_length);

			if (!NT_SUCCESS (status))
				{
				ExFreePool (*info);
				*info = NULL;
				}

			if (sub_key)
				{
				sub_key->Release();
				}
			}

		device_key->Release();
		}

	return status;
	}


#pragma code_seg ("PAGE")


/* Write something to registry
 */
NTSTATUS CGF1Common::write_registry
	(
	IN PCWSTR	subkey	OPTIONAL,
	IN PCWSTR	value_name,
	IN ULONG	value_type,
	IN PVOID	value_data,
	IN ULONG	value_length,
	IN BOOLEAN	volat
	)

	{
	PAGED_CODE();

	NTSTATUS		status;
    PREGISTRYKEY    device_key;

	ASSERT (value_name);
	ASSERT (value_data);

	// Open device key
	status = PcNewRegistryKey (&device_key, NULL, DeviceRegistryKey,
		KEY_ALL_ACCESS, device_object, NULL, NULL, 0, NULL);

	if (NT_SUCCESS (status))
		{
		UNICODE_STRING	uni_name;
		PREGISTRYKEY	sub_key		= NULL;

		if (subkey)
			{
			// Make subkey name string
			RtlInitUnicodeString (&uni_name, subkey);

			// Open subkey
			status = device_key->NewSubKey (&sub_key, NULL, KEY_ALL_ACCESS, &uni_name,
				volat ? REG_OPTION_VOLATILE : REG_OPTION_NON_VOLATILE, NULL);
			}

		if (NT_SUCCESS (status))
			{
			// Make value name string
			RtlInitUnicodeString (&uni_name, value_name);

			// Write data
			(sub_key ? sub_key : device_key)->SetValueKey (&uni_name, value_type,
				value_data, value_length);

			if (sub_key)
				{
				sub_key->Release();
				}
			}

		device_key->Release();
		}

	return status;
	}



#pragma code_seg ("PAGE")


/* Compare string in registry
 */
LONG CGF1Common::registry_stricmp
	(
	IN PCWSTR		subkey	OPTIONAL,
	IN PCWSTR		valkey,
	IN PCWSTR		string2 OPTIONAL
	)

	{
	PAGED_CODE();

	NTSTATUS						status;
	LONG							result;
	UNICODE_STRING					unistr1;
	UNICODE_STRING					unistr2;
	PKEY_VALUE_PARTIAL_INFORMATION	info;

	ASSERT (valkey);

	// Get info
	status = read_registry (&info, subkey, valkey, 256);
	if (!NT_SUCCESS (status))
		{
		// Can't read value -> it's NULL -> str1 is less
		ASSERT (!info);
		return -1;
		}
	if (info->Type != REG_SZ)
		{
		// Not a string -> str1 is less
		ExFreePool (info);
		return -1;
		}

	if (!string2)
		{
		// No string to compare to -> str1 is greater
		ExFreePool (info);
		return 1;
		}

	// Compare strings
	RtlInitUnicodeString (&unistr1, (PCWSTR) info->Data);
	RtlInitUnicodeString (&unistr2, string2);
	result = RtlCompareUnicodeString (&unistr1, &unistr2, FALSE);
	ExFreePool (info);

	return result;
	}


#pragma code_seg ("PAGE")


/* Read integer from registry
 */
NTSTATUS CGF1Common::read_registry_int
	(
	IN PCWSTR	subkey	OPTIONAL,
	IN PCWSTR	valkey,
	IN BOOLEAN	volat,
	OUT PULONG	value
	)

	{
	PAGED_CODE();

	NTSTATUS						status;
	PKEY_VALUE_PARTIAL_INFORMATION	info;

	ASSERT (valkey);
	ASSERT (value);

	// Get info
	status = read_registry (&info, subkey, valkey, sizeof (ULONG));
	if (NT_SUCCESS (status))
		{
		if (info->Type != REG_DWORD)
			{
			status = STATUS_NOT_FOUND;
			}
		else
			{
			*value = *((PLONG) info->Data);
			}
		ExFreePool (info);
		}

	return status;
	}


#pragma code_seg ("PAGE")


/* Write string to registry
 */
NTSTATUS CGF1Common::write_registry_str
	(
	IN PCWSTR		subkey	OPTIONAL,
	IN PCWSTR		valkey,
	IN PCWSTR		value,
	IN BOOLEAN      volat
	)

	{
	PAGED_CODE();

	UNICODE_STRING uni_val;

	ASSERT (valkey);
	ASSERT (value);

	// Compute string length
	RtlInitUnicodeString (&uni_val, value);
	return write_registry (subkey, valkey, REG_SZ, (PVOID) value,
		uni_val.Length + sizeof (WCHAR), volat);
	}


#pragma code_seg ("PAGE")


/* Write integer to registry
 */
NTSTATUS CGF1Common::write_registry_int
	(
	IN PCWSTR		subkey	OPTIONAL,
	IN PCWSTR		valkey,
	IN ULONG		value,
	IN BOOLEAN      volat
	)

	{
	PAGED_CODE();

	ASSERT (valkey);
	ASSERT (value);

	// Compute string length
	return write_registry (subkey, valkey, REG_DWORD, (PVOID) &value, sizeof (ULONG), volat);
	}


#pragma code_seg ("PAGE")


/* Write debugging info to registry
 */
void CGF1Common::reg_putstr
	(
	IN ULONG	type,
	IN PCWSTR	name,
	IN PCWSTR	value
	)

	{
	PAGED_CODE();

	if (info_enabled)
		{
		if (type == GF1REGPUT_INFO)
			{
			write_registry_str (L"Info", name, value, FALSE);
			}
		else if (type == GF1REGPUT_DBG)
			{
			write_registry_str (L"DebugInfo", name, value, FALSE);
			}
		}
	if (type == GF1REGPUT_CONFIG)
		{
		write_registry_str (L"Settings", name, value, FALSE);
		}
	}


#pragma code_seg ("PAGE")


void CGF1Common::reg_putint
	(
	IN ULONG	type,
	IN PCWSTR	name,
	IN ULONG	value
	)

	{
	PAGED_CODE();

	if (info_enabled)
		{
		if (type == GF1REGPUT_INFO)
			{
			write_registry_int (L"Info", name, value, FALSE);
			}
		else if (type == GF1REGPUT_DBG)
			{
			write_registry_int (L"DebugInfo", name, value, FALSE);
			}
		}
	if (type == GF1REGPUT_CONFIG)
		{
		write_registry_int (L"Settings", name, value, FALSE);
		}
	}


#pragma code_seg ("PAGE")


/* Write integer to registry
 */
NTSTATUS CGF1Common::reg_getint
	(
	IN ULONG	type,
	IN PCWSTR	name,
	OUT PULONG	value
	)

	{
	PAGED_CODE();

	NTSTATUS	status = STATUS_INVALID_PARAMETER;

	if (info_enabled)
		{
		if (type == GF1REGGET_INFO)
			{
			status = read_registry_int (L"Info", name, FALSE, value);
			}
		else if (type == GF1REGGET_DBG)
			{
			status = read_registry_int (L"DebugInfo", name, FALSE, value);
			}
		}
	if (type == GF1REGGET_CONFIG)
		{
		status = read_registry_int (L"Settings", name, FALSE, value);
		}

	return status;
	}


#pragma code_seg()


#if GF1_DBG

struct DVContext
	{
	IGF1Common *	self;
	BYTE			vctrl[32];
	BYTE			rctrl[32];
	BYTE			panning[32];
	WORD			volume[32];
	ULONG			position[32];
	};


static NTSTATUS dbg_voices
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	DVContext *		ctx;
	ULONG			v;

	ctx = (DVContext *) context;

	for (v = 0; v < 32; v++)
		{
		ctx->self->dwrite (GF1R_VSELECT, (BYTE) v);
		ctx->vctrl[v] = ctx->self->iread8 (GF1REGV_VOICE_CTRL);
		ctx->rctrl[v] = ctx->self->iread8 (GF1REGV_RAMP_CTRL);
		ctx->panning[v] = ctx->self->iread8 (GF1REGV_PANNING) & 0xf;
		ctx->volume[v] = ctx->self->iread16 (GF1REGV_VOLUME);
		ctx->position[v] = (ctx->self->iread16 (GF1REGV_POSL) >> 9) & 0x7f;
		ctx->position[v] |= (ctx->self->iread16 (GF1REGV_POSH) & 0x1fff) << 7;
		}

	return STATUS_SUCCESS;
	}

struct DRContext
	{
	IGF1Common *	self;
	BYTE			irq_status;
	BYTE			dma_ctrl;
	BYTE			smp_ctrl;
	BYTE			reset;
	};


static NTSTATUS dbg_registers
	(
	PINTERRUPTSYNC,
	PVOID			context
	)

	{
	DRContext *		ctx;

	ctx = (DRContext *) context;

	ctx->irq_status = ctx->self->dread (GF1R_IRQ_STATUS);
	ctx->dma_ctrl = ctx->self->iread8 (GF1REG_DMA);
	ctx->smp_ctrl = ctx->self->iread8 (GF1REG_SMP_CTRL);
	ctx->reset = ctx->self->iread8 (GF1REG_RESET);

	return STATUS_SUCCESS;
	}

#endif /* GF1_DBG */


#pragma code_seg ("PAGE")


	/* Write debugging info to registry
	 */
void CGF1Common::debug_to_registry
	(
	IN PCWSTR	from
	)

	{
	PAGED_CODE();

	GF1_DBGSTR (L"DbgSource", from);
	GF1_DBGINT (L"ActiveVoices", gf1_voices);
	GF1_DBGINT (L"CheckWaveIrqOk", (ULONG) !check_wave_irq);
	GF1_DBGINT (L"CheckRampIrqOk", (ULONG) !check_ramp_irq);
	GF1_DBGINT (L"CheckMidiIrqOk", (ULONG) !check_midi_irq);
	GF1_DBGINT (L"CheckTimerIrqOk", (ULONG) !check_timer_irq);
	GF1_DBGINT (L"CheckDmaIrqOk", (ULONG) check_dma_irq ? 0 : 1);

	#if GF1_DBG

	{
	DVContext ctx;

	ctx.self = (IGF1Common *) this;
	call_synchronized (&dbg_voices, &ctx);
	GF1_DBGINT (L"Voice0VCtrl", ctx.vctrl[0]);
	GF1_DBGINT (L"Voice0RCtrl", ctx.rctrl[0]);
	GF1_DBGINT (L"Voice0Pan", ctx.panning[0]);
	GF1_DBGINT (L"Voice0Vol", ctx.volume[0]);
	GF1_DBGINT (L"Voice0Pos", ctx.position[0]);
	GF1_DBGINT (L"Voice1VCtrl", ctx.vctrl[1]);
	GF1_DBGINT (L"Voice1RCtrl", ctx.rctrl[1]);
	GF1_DBGINT (L"Voice1Pan", ctx.panning[1]);
	GF1_DBGINT (L"Voice1Vol", ctx.volume[1]);
	GF1_DBGINT (L"Voice1Pos", ctx.position[1]);
	GF1_DBGINT (L"Voice2VCtrl", ctx.vctrl[2]);
	GF1_DBGINT (L"Voice2RCtrl", ctx.rctrl[2]);
	GF1_DBGINT (L"Voice2Pan", ctx.panning[2]);
	GF1_DBGINT (L"Voice2Vol", ctx.volume[2]);
	GF1_DBGINT (L"Voice2Pos", ctx.position[2]);
	GF1_DBGINT (L"Voice3VCtrl", ctx.vctrl[3]);
	GF1_DBGINT (L"Voice3RCtrl", ctx.rctrl[3]);
	GF1_DBGINT (L"Voice3Pan", ctx.panning[3]);
	GF1_DBGINT (L"Voice3Vol", ctx.volume[3]);
	GF1_DBGINT (L"Voice3Pos", ctx.position[3]);
	}// eob

	{
	DRContext ctx;

	ctx.self = (IGF1Common *) this;
	call_synchronized (&dbg_registers, &ctx);
	GF1_DBGINT (L"RegMixCtrl", mixer_settings);
	GF1_DBGINT (L"RegIrqStatus", ctx.irq_status);
	GF1_DBGINT (L"RegDmaCtrl", ctx.dma_ctrl);
	GF1_DBGINT (L"RegSmpCtrl", ctx.smp_ctrl);
	GF1_DBGINT (L"RegReset", ctx.reset);
	}

	GF1_DBGINT (L"IntsHandledWavetable", wave_irqs_handled);
	GF1_DBGINT (L"IntsHandledVolumeRamp", ramp_irqs_handled);
	GF1_DBGINT (L"IntsHandledTimer", timer_irqs_handled);
	GF1_DBGINT (L"IntsHandledDramDma", ddma_irqs_handled);
	GF1_DBGINT (L"IntsHandledRecDma", rdma_irqs_handled);
	GF1_DBGINT (L"IntsHandledMidiXmit", xmit_irqs_handled);
	GF1_DBGINT (L"IntsHandledMidiRecv", recv_irqs_handled);
	GF1_DBGINT (L"IntsHandledCodecPlayback", cplay_irqs_handled);
	GF1_DBGINT (L"IntsHandledCodecRecord", crec_irqs_handled);
	GF1_DBGINT (L"IntsHandledCodecTimer", ctimer_irqs_handled);

	GF1_DBGINT (L"DramDmaRequests", ddma_first == -1 ? 0 :
		ddma_last >= ddma_first ? ddma_last - ddma_first + 1 :
		GF1DDMA_REQUESTS - ddma_first + ddma_last + 1);
	GF1_DBGINT (L"DramDmaBytesSending", ddma_sending);
	GF1_DBGINT (L"DramDmaBytesSent", ddma_transferred);
	GF1_DBGINT (L"DramDmaEnabled", ddma_enabled ? 1 : 0);
	GF1_DBGINT (L"DramDmaBusy", ddma_busy ? 1 : 0);

	GF1_DBGINT (L"DmaDramInits", ddma_inits);
	GF1_DBGINT (L"DmaRecAllocated", dma_rec_allocated);
	GF1_DBGINT (L"DmaCodecPlayAllocated", dma_cplay_allocated);
	GF1_DBGINT (L"DmaCodecRecAllocated", dma_crec_allocated);

	#endif /* GF1_DBG */
	}
