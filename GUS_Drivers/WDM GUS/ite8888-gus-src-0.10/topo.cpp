/*
 * topo.cpp
 *
 * Topology miniport
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


#include "topo.h"
#include "topotab.cpp"


#define CHANNEL_LEFT       0
#define CHANNEL_RIGHT      1
#define CHANNEL_MASTER     (-1)


// Debugging info (FIXME: dump topology state)
#if GF1_DBG

#define DAILY_DEBUG(c,f) \
	{ \
	(c)->gf1_common->debug_to_registry (f); \
	}
#define DBG_WRITE_TO_REGISTRY(c)	(c)->write_to_registry()

#else /* GF1_DBG */

#define DAILY_DEBUG(c) {}
#define DBG_WRITE_TO_REGISTRY(c) {}

#endif /* GF1_DBG */


#pragma code_seg("PAGE")


/* Create new GF1 topology object
 */
NTSTATUS create_gf1_topology
	(
    OUT PUNKNOWN *	unknown,
    IN POOL_TYPE	pool_type
	)

	{
    PAGED_CODE();

    ASSERT (unknown);

    STD_CREATE_BODY (CGF1Topology, unknown, NULL, pool_type);
	}



/**************************************** IUnknown stuff ******************************************/

#pragma code_seg ("PAGE")


CGF1Topology::CGF1Topology
	(
	PUNKNOWN	unknown
	) : CUnknown (unknown)

	{
	PAGED_CODE();

	gf1_common = NULL;
	port = NULL;
	node_info = NULL;
	nodes = 0;
	}


#pragma code_seg ("PAGE")


CGF1Topology::~CGF1Topology()
	{
	#if GF1_DBG
	if (gf1_common)
		{
		GF1_DBGINT_ (gf1_common, L"DestroyingCGF1Topology", 1);
		}
	#endif /* GF1_DBG */

	if (node_info)
		{
		if (gf1_common)
			write_to_registry();
		ExFreePool (node_info);
		node_info = NULL;
		}

	if (port)
		{
		//port->Release();
		port = NULL;
		}

	if (gf1_common)
		{
		gf1_common->Release();
		gf1_common = NULL;
		}
	}


#pragma code_seg ("PAGE")


/* Obtain an interface
 */
STDMETHODIMP CGF1Topology::NonDelegatingQueryInterface
	(
	REFIID		iface,
    PVOID *		object
	)

	{
	PAGED_CODE();

	ASSERT (object);

	if (IsEqualGUIDAligned (iface, IID_IUnknown))
		{
		*object = (PVOID) ((PUNKNOWN) this);
		}
	else if (IsEqualGUIDAligned (iface, IID_IMiniport))
		{
		*object = (PVOID) ((PMINIPORT) this);
		}
	else if (IsEqualGUIDAligned (iface, IID_IMiniportTopology))
		{
		*object = (PVOID) ((PMINIPORTTOPOLOGY) this);
		}
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



/**************************************** IMiniport stuff *****************************************/

#pragma code_seg ("PAGE")


/* Get topology description
 */
NTSTATUS CGF1Topology::GetDescription
	(
	OUT PPCFILTER_DESCRIPTOR *		descriptor
	)

	{
	PAGED_CODE();

	ASSERT (descriptor);

	if (!gf1_common)
		return STATUS_UNSUCCESSFUL;

	switch (gf1_common->get_revision())
		{
	case GF1REVISION_MAX:
	case GF1REVISION_PNP:
		if (gf1_common->get_codec_mode() == CODEC_MODE3)
			*descriptor = &iw_descriptor;
		else
			*descriptor = &cs_descriptor;
		break;

	case GF1REVISION_PRE34:
	case GF1REVISION_PRE37:
	case GF1REVISION_ICSFLIPPED:
	case GF1REVISION_ICS:
	default:
		*descriptor = &gf1_descriptor;
		break;
		}

	return STATUS_SUCCESS;
	}



/************************************ IMiniportTopology stuff *************************************/

#pragma code_seg ("PAGE")


/* Initialize miniport
 */
NTSTATUS CGF1Topology::Init
	(
	IN PUNKNOWN			common_unknown,
	IN PRESOURCELIST,
	IN PPORTTOPOLOGY	_port
	)

	{
	PAGED_CODE();

	NTSTATUS status;

	ASSERT (common_unknown);
	ASSERT (_port);

	// Keep port
	port = _port;

	/* Note:
	 Do not addref it, because ports addrefs us - and we don't want cycles. Port is
	 responsible for destroying miniport, so this pointer should be always valid.
	 */
	//port->AddRef();

	// Keep IGF1Common
	status = common_unknown->QueryInterface (IID_IGF1Common, (PVOID *) &gf1_common);

	// Allocate node info (paged)
	if (gf1_common->get_revision() >= GF1REVISION_MAX)
		{
		if (gf1_common->get_codec_mode() == CODEC_MODE3)
			nodes = IWNODE_MAX;
		else
			nodes = CSNODE_MAX;
		}
	else
		{
		nodes = GF1NODE_MAX;
		}
	node_info = (GF1NodeInfo *) ExAllocatePool (PagedPool, sizeof (GF1NodeInfo) * nodes);
	if (!node_info)
		status = STATUS_INSUFFICIENT_RESOURCES;

	if (NT_SUCCESS (status))
		{
		ULONG i;

		// Invalidate 'node_info' array
		for (i = 0; i < nodes; i++)
			{
			node_info[i].left_valid = FALSE;
			node_info[i].right_valid = FALSE;
			}

		read_from_registry();
		}

	return status;
	}



/*************************************** Property handlers ****************************************/

#pragma code_seg ("PAGE")


/* On/Off controls (mute)
 */
NTSTATUS CGF1Topology::prophandler_onoff
	(
	IN PPCPROPERTY_REQUEST		request
	)

	{
	PAGED_CODE();

	NTSTATUS		status = STATUS_INVALID_PARAMETER;
	CGF1Topology *	self;

	ASSERT (request);

	self = (CGF1Topology *) request->MajorTarget;
	ASSERT (self);

	if (request->Node >= 0 && request->Node < self->nodes)
		{
		// Node OK

		if (request->Verb & KSPROPERTY_TYPE_GET)
			{

			// Get property
			if (request->InstanceSize >= sizeof (LONG) &&
			    request->ValueSize >= sizeof (BOOL))
				{
				LONG channel = *((PLONG) request->Instance);
				PBOOL b = (PBOOL) request->Value;

				// Try to retrieve info from "cache"
				if ((channel == CHANNEL_LEFT && self->node_info[request->Node].left_valid) ||
					(channel == CHANNEL_RIGHT && self->node_info[request->Node].right_valid))
					{
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
						{
						if (channel == CHANNEL_LEFT)
							*b = self->node_info[request->Node].mute.lmute;
						else
							*b = self->node_info[request->Node].mute.rmute;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					}

				// Must read registers
				else if (self->gf1_common->get_revision() >= GF1REVISION_MAX) switch (request->Node)

					// CS4231 nodes

					{
				case CSNODE_WAVEOUT_MUTE:
					// WaveOut mute (stereo)
					if ((request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) &&
						(channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT))
						{
						*b = (self->gf1_common->get_codec_reg
							((channel == CHANNEL_LEFT) ? CREG_DAC_LEFT : CREG_DAC_RIGHT)
							& CDAC_MUTE) ? TRUE : FALSE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;

				case CSNODE_SYNTH_MUTE:
					// Synth (aux1) mute (stereo)
					if ((request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) &&
						(channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT))
						{
						*b = (self->gf1_common->get_codec_reg
							((channel == CHANNEL_LEFT) ? CREG_SYNTH_LEFT : CREG_SYNTH_RIGHT)
							& CGAIN_MUTE) ? TRUE : FALSE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;

				case CSNODE_CDIN_MUTE:
					// CDIn mute (stereo)
					if ((request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) &&
						(channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT))
						{
						*b = (self->gf1_common->get_codec_reg
							((channel == CHANNEL_LEFT) ? CREG_CDIN_LEFT : CREG_CDIN_RIGHT)
							& CGAIN_MUTE) ? TRUE : FALSE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;

				case CSNODE_LINEIN_MUTE:
					// LineIn mute (stereo)
					if ((request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) &&
						(channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT))
						{
						*b = (self->gf1_common->get_codec_reg
							((channel == CHANNEL_LEFT) ? CREG_LINEIN_LEFT : CREG_LINEIN_RIGHT)
							& CGAIN_MUTE) ? TRUE : FALSE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;

				case CSNODE_MICIN_MUTE:
					if (self->gf1_common->get_codec_mode() == CODEC_MODE3)
						{
						// Interwave MicIn->Sum stereo mute
						if ((request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) &&
							(channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT))
							{
							*b = (self->gf1_common->get_codec_reg
								((channel == CHANNEL_LEFT) ? CREG_MICIN_LEFT : CREG_MICIN_RIGHT)
								& CGAIN_MUTE) ? TRUE : FALSE;
							request->ValueSize = sizeof (BOOL);
							status = STATUS_SUCCESS;
							}
						}
					else
						{
						// MicIn master mute (old-style, mono)
						if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE && channel == CHANNEL_LEFT)
							{
							*b = (self->gf1_common->get_mixer() & GF1MIX_MICIN) ? FALSE : TRUE;
							request->ValueSize = sizeof (BOOL);
							status = STATUS_SUCCESS;
							}
						}
					break;

				case CSNODE_LINEOUT_MUTE:
					if (self->gf1_common->get_codec_mode() == CODEC_MODE3)
						{
						// Interwave Master stereo mute
						if ((request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) &&
							(channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT))
							{
							*b = (self->gf1_common->get_codec_reg
								((channel == CHANNEL_LEFT) ? CREG_LINEOUT_LEFT : CREG_LINEOUT_RIGHT)
								& COUT_MUTE) ? TRUE : FALSE;
							request->ValueSize = sizeof (BOOL);
							status = STATUS_SUCCESS;
							}
						}
					else
						{
						// LineOut mute (old-style, mono)
						if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE && channel == 0)
							{
							*b = (self->gf1_common->get_mixer() & GF1MIX_NOLINEOUT) ? TRUE : FALSE;
							request->ValueSize = sizeof (BOOL);
							status = STATUS_SUCCESS;
							}
						}
					break;
					}

				else switch (request->Node)

					// Classic nodes

					{
				case GF1NODE_LINEIN_MUTE:
					// LineIn mute (mono)
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE && channel == 0)
						{
						*b = (self->gf1_common->get_mixer() & GF1MIX_NOLINEIN) ? TRUE : FALSE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;

				case GF1NODE_LINEOUT_MUTE:
					// LineOut mute (mono)
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE && channel == 0)
						{
						*b = (self->gf1_common->get_mixer() & GF1MIX_NOLINEOUT) ? TRUE : FALSE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;

				case GF1NODE_MICIN_MUTE:
					// MicIn mute (mono)
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE && channel == 0)
						{
						*b = (self->gf1_common->get_mixer() & GF1MIX_MICIN) ? FALSE : TRUE;
						request->ValueSize = sizeof (BOOL);
						status = STATUS_SUCCESS;
						}
					break;
					}

				if (NT_SUCCESS (status))
					{
					// Update cache
					if (channel == CHANNEL_LEFT || channel == CHANNEL_MASTER)
						{
						self->node_info[request->Node].left_valid = TRUE;
						self->node_info[request->Node].mute.lmute = (BOOLEAN) *b;
						}
					if (channel == CHANNEL_RIGHT || channel == CHANNEL_MASTER)
						{
						self->node_info[request->Node].right_valid = TRUE;
						self->node_info[request->Node].mute.rmute = (BOOLEAN) *b;
						}
					}
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}

			}

		else if (request->Verb & KSPROPERTY_TYPE_SET)
			{

			// Set property
			if (request->InstanceSize >= sizeof (LONG) &&
			    request->ValueSize >= sizeof (BOOL))
				{
				LONG channel = *((PLONG) request->Instance);
				BOOL b = * ((PBOOL) request->Value);

				if (self->gf1_common->get_revision() >= GF1REVISION_MAX) switch (request->Node)

					// CS4231 nodes

					{
				case CSNODE_WAVEOUT_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
						{
						// WaveOut mute (stereo)
						BYTE value = (b) ? CDAC_MUTE : 0;
						if (channel == CHANNEL_MASTER)
							{
							self->gf1_common->set_codec_reg (CREG_DAC_LEFT,  ~CDAC_MUTE, value);
							self->gf1_common->set_codec_reg (CREG_DAC_RIGHT, ~CDAC_MUTE, value);
							status = STATUS_SUCCESS;
							}
						else if (channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT)
							{
							BYTE reg = (channel == CHANNEL_LEFT) ? CREG_DAC_LEFT : CREG_DAC_RIGHT;
							self->gf1_common->set_codec_reg (reg, ~CDAC_MUTE, value);
							status = STATUS_SUCCESS;
							}
						}
					break;

				case CSNODE_SYNTH_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
						{
						// Synth (aux1) mute (stereo)
						BYTE value = (b) ? CGAIN_MUTE : 0;
						if (channel == CHANNEL_MASTER)
							{
							self->gf1_common->set_codec_reg (CREG_SYNTH_LEFT,  ~CGAIN_MUTE, value);
							self->gf1_common->set_codec_reg (CREG_SYNTH_RIGHT, ~CGAIN_MUTE, value);
							status = STATUS_SUCCESS;
							}
						else if (channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT)
							{
							BYTE reg = (channel == CHANNEL_LEFT) ?
								CREG_SYNTH_LEFT : CREG_SYNTH_RIGHT;
							self->gf1_common->set_codec_reg (reg, ~CGAIN_MUTE, value);
							status = STATUS_SUCCESS;
							}
						}
					break;

				case CSNODE_CDIN_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
						{
						// CDIn mute (stereo)
						BYTE value = (b) ? CGAIN_MUTE : 0;
						if (channel == CHANNEL_MASTER)
							{
							self->gf1_common->set_codec_reg (CREG_CDIN_LEFT,  ~CGAIN_MUTE, value);
							self->gf1_common->set_codec_reg (CREG_CDIN_RIGHT, ~CGAIN_MUTE, value);
							status = STATUS_SUCCESS;
							}
						else if (channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT)
							{
							BYTE reg = (channel == CHANNEL_LEFT) ? CREG_CDIN_LEFT : CREG_CDIN_RIGHT;
							self->gf1_common->set_codec_reg (reg, ~CGAIN_MUTE, value);
							status = STATUS_SUCCESS;
							}
						}
					break;

				case CSNODE_LINEIN_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
						{
						// LineIn mute (stereo)
						BYTE value = (b) ? CGAIN_MUTE : 0;
						if (channel == CHANNEL_MASTER)
							{
							self->gf1_common->set_codec_reg (CREG_LINEIN_LEFT,  ~CGAIN_MUTE, value);
							self->gf1_common->set_codec_reg (CREG_LINEIN_RIGHT, ~CGAIN_MUTE, value);
							status = STATUS_SUCCESS;
							}
						else if (channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT)
							{
							BYTE reg = (channel == CHANNEL_LEFT) ?
								CREG_LINEIN_LEFT : CREG_LINEIN_RIGHT;
							self->gf1_common->set_codec_reg (reg, ~CGAIN_MUTE, value);
							status = STATUS_SUCCESS;
							}
						}
					break;

				case CSNODE_MICIN_MUTE:
					if (self->gf1_common->get_codec_mode() == CODEC_MODE3)
						{
						if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
							{
							// Interwave MicIn mute (stereo)
							BYTE value = (b) ? CGAIN_MUTE : 0;
							if (channel == CHANNEL_MASTER)
								{
								self->gf1_common->set_codec_reg (CREG_MICIN_LEFT,
									~CGAIN_MUTE, value);
								self->gf1_common->set_codec_reg (CREG_MICIN_RIGHT,
									~CGAIN_MUTE, value);
								status = STATUS_SUCCESS;
								}
							else if (channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT)
								{
								BYTE reg = (channel == CHANNEL_LEFT) ?
									CREG_MICIN_LEFT : CREG_MICIN_RIGHT;
								self->gf1_common->set_codec_reg (reg, ~CGAIN_MUTE, value);
								status = STATUS_SUCCESS;
								}
							}
						}
					else
						{
						if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE &&
							channel == CHANNEL_LEFT)
							{
							// MicIn mute (mono)
							BYTE mix = self->gf1_common->get_mixer();
							if (b)
								mix &= ~GF1MIX_MICIN;
							else
								mix |= GF1MIX_MICIN;
							self->gf1_common->set_mixer (mix);
							status = STATUS_SUCCESS;
							}
						}
					break;

				case CSNODE_LINEOUT_MUTE:
					if (self->gf1_common->get_codec_mode() == CODEC_MODE3)
						{
						if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
							{
							// Interwave Master mute (stereo)
							BYTE value = (b) ? COUT_MUTE : 0;
							if (channel == CHANNEL_MASTER)
								{
								self->gf1_common->set_codec_reg (CREG_LINEOUT_LEFT,
									~COUT_MUTE, value);
								self->gf1_common->set_codec_reg (CREG_LINEOUT_RIGHT,
									~COUT_MUTE, value);
								status = STATUS_SUCCESS;
								}
							else if (channel == CHANNEL_LEFT || channel == CHANNEL_RIGHT)
								{
								BYTE reg = (channel == CHANNEL_LEFT) ?
									CREG_LINEOUT_LEFT : CREG_LINEOUT_RIGHT;
								self->gf1_common->set_codec_reg (reg, ~COUT_MUTE, value);
								status = STATUS_SUCCESS;
								}
							}
						}
					else
						{
						if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE &&
							channel == CHANNEL_LEFT)
							{
							// LineOut mute (mono)
							BYTE mix = self->gf1_common->get_mixer();
							if (b)
								mix |= GF1MIX_NOLINEOUT;
							else
								mix &= ~GF1MIX_NOLINEOUT;
							self->gf1_common->set_mixer (mix);
							status = STATUS_SUCCESS;
							}
						}
					break;
					}

				else switch (request->Node)

					// Classic nodes

					{
				case GF1NODE_LINEIN_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE &&
						channel == CHANNEL_LEFT)
						{
						// LineIn mute (mono)
						BYTE mix = self->gf1_common->get_mixer();
						if (b)
							mix |= GF1MIX_NOLINEIN;
						else
							mix &= ~GF1MIX_NOLINEIN;
						self->gf1_common->set_mixer (mix);
						status = STATUS_SUCCESS;
						}
					break;

				case GF1NODE_LINEOUT_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE &&
						channel == CHANNEL_LEFT)
						{
						// LineOut mute (mono)
						BYTE mix = self->gf1_common->get_mixer();
						if (b)
							mix |= GF1MIX_NOLINEOUT;
						else
							mix &= ~GF1MIX_NOLINEOUT;
						self->gf1_common->set_mixer (mix);
						status = STATUS_SUCCESS;
						}
					break;

				case GF1NODE_MICIN_MUTE:
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE &&
						channel == CHANNEL_LEFT)
						{
						// MicIn mute (mono)
						BYTE mix = self->gf1_common->get_mixer();
						if (b)
							mix &= ~GF1MIX_MICIN;
						else
							mix |= GF1MIX_MICIN;
						self->gf1_common->set_mixer (mix);
						status = STATUS_SUCCESS;
						}
					break;
					}

				if (NT_SUCCESS (status))
					{
					// Update cache
					if (channel == CHANNEL_LEFT || channel == CHANNEL_MASTER)
						{
						self->node_info[request->Node].left_valid = TRUE;
						self->node_info[request->Node].mute.lmute = (BOOLEAN) b;
						}
					if (channel == CHANNEL_RIGHT || channel == CHANNEL_MASTER)
						{
						self->node_info[request->Node].right_valid = TRUE;
						self->node_info[request->Node].mute.rmute = (BOOLEAN) b;
						}
					}
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}

			}

		else if (request->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
			{
			// Request property support
			if (
				(self->gf1_common->get_revision() >= GF1REVISION_MAX &&
				 (
				  (request->Node == CSNODE_WAVEOUT_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
				  (request->Node == CSNODE_SYNTH_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
				  (request->Node == CSNODE_CDIN_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
				  (request->Node == CSNODE_LINEIN_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
				  (request->Node == CSNODE_MICIN_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
				  (request->Node == CSNODE_LINEOUT_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
				 )
				) ||
				(self->gf1_common->get_revision() < GF1REVISION_MAX &&
				 (
				  (request->Node == GF1NODE_LINEIN_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
			      (request->Node == GF1NODE_LINEOUT_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ||
			      (request->Node == GF1NODE_MICIN_MUTE &&
				   request->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE)
				 )
				)
			   )
				{
				if (request->ValueSize >= sizeof (KSPROPERTY_DESCRIPTION))
					{
					// "Complete" description
					PKSPROPERTY_DESCRIPTION pd = (PKSPROPERTY_DESCRIPTION) request->Value;

					pd->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
						KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
					pd->DescriptionSize = sizeof (KSPROPERTY_DESCRIPTION);
					pd->PropTypeSet.Set = KSPROPTYPESETID_General;
					pd->PropTypeSet.Id = VT_BOOL;
					pd->PropTypeSet.Flags = 0;
					pd->MembersListCount = 0;
					pd->Reserved = 0;

					request->ValueSize = sizeof (KSPROPERTY_DESCRIPTION);
					status = STATUS_SUCCESS;
					}
				else if (request->ValueSize >= sizeof (ULONG))
					{
					// Access flags
					PULONG af = (PULONG) request->Value;
					*af = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
					request->ValueSize = sizeof (ULONG);
					status = STATUS_SUCCESS;
					}
				else
					{
					status = STATUS_BUFFER_TOO_SMALL;
					}
				}
			}
		}

	DAILY_DEBUG (self, L"CGF1Topology::prophandler_onoff");
	DBG_WRITE_TO_REGISTRY (self);

	return status;
	}


#pragma code_seg ("PAGE")


/* CPU resources
 */
NTSTATUS CGF1Topology::prophandler_cpu
	(
	IN PPCPROPERTY_REQUEST		request
	)

	{
	PAGED_CODE();

	NTSTATUS		status = STATUS_INVALID_PARAMETER;
	CGF1Topology *	self;

	ASSERT (request);

	self = (CGF1Topology *) request->MajorTarget;
	ASSERT (self);

	if (request->Node != (ULONG) -1)
		{
		// Node OK

		if (request->Verb & KSPROPERTY_TYPE_GET)
			{
			// Get property
			if (request->ValueSize >= sizeof (LONG))
				{
				// Any node is in Hw
				*((PLONG) (request->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
				request->ValueSize = sizeof (LONG);
				status = STATUS_SUCCESS;
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}

		else if (request->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
			{
			// Request property support
			if (request->ValueSize >= sizeof (KSPROPERTY_DESCRIPTION))
				{
				// "Complete" description
				PKSPROPERTY_DESCRIPTION pd = (PKSPROPERTY_DESCRIPTION) request->Value;

				pd->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET;
				pd->DescriptionSize = sizeof (KSPROPERTY_DESCRIPTION);
				pd->PropTypeSet.Set = KSPROPTYPESETID_General;
				pd->PropTypeSet.Id = VT_I4;
				pd->PropTypeSet.Flags = 0;
				pd->MembersListCount = 0;
				pd->Reserved = 0;

				request->ValueSize = sizeof (KSPROPERTY_DESCRIPTION);
				status = STATUS_SUCCESS;
				}
			else if (request->ValueSize >= sizeof (ULONG))
				{
				// Access flags
				PULONG af = (PULONG) request->Value;
				*af = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET;
				request->ValueSize = sizeof (ULONG);
				status = STATUS_SUCCESS;
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}
		}

	DAILY_DEBUG (self, L"CGF1Topology::prophandler_cpu");

	return status;
	}


#pragma code_seg ("PAGE")


/* Get volume node/register info
 I: node - node id
 O: min_db - volume in db (fixed point 16.16) if reg. value is 0
 O: max_db - volume in db (fixed point 16.16) if reg. value is 'max_val'
 O: max_val - maximum volume value and mask (always n lowest bits)
 O: lreg - left volume register index, (BYTE) -1 if left volume is not supported
 O: rreg - right volume register index, (BYTE) -1 if left volume is not supported
 N: min_db may be > max_db
 */
static NTSTATUS volume_info
	(
	IN ULONG	node,
	OUT LONG *	min_db,
	OUT LONG *	max_db,
	OUT BYTE *	max_val,
	OUT BYTE *	lreg,
	OUT BYTE *	rreg
	)

	{
	NTSTATUS status = STATUS_SUCCESS;

	switch (node)
		{
	case CSNODE_WAVEOUT_VOLUME:
		// DAC attenuation
		*min_db = 0;										// 0 dB
		*max_db = -CDAC_ATTN_MASK * 3 << 15;				// -94.5 dB (1.5 dB step)
		*max_val = CDAC_ATTN_MASK;
		*lreg = CREG_DAC_LEFT;
		*rreg = CREG_DAC_RIGHT;
		break;

	//case CSNODE_WAVEIN_VOLUME:
	case CSNODE_SYNTH_MUX_VOLUME:
	case CSNODE_LINEIN_MUX_VOLUME:
	case CSNODE_MICIN_MUX_VOLUME:
	case CSNODE_MIXER_MUX_VOLUME:
		// ADC gain
		/* Note:
		 I don't know why, but obviously positive values are not welcome here. So instead
		 of 0 dB to 22.5 dB range we claim it's -22.5 dB to 0 dB.
		 FIXME: Is that bug in driver or in portcls or what?
		 *min_db = 0;
		 *max_db = -CADC_GAIN_MASK * 3 << 15;
		 */
		*min_db = -CADC_GAIN_MASK * 3 << 15;				// 0 dB - 22.5
		*max_db = 0;										// 22.5 dB - 22.5 (1.5 dB step)
		*max_val = CADC_GAIN_MASK;
		*lreg = CREG_ADC_LEFT;
		*rreg = CREG_ADC_RIGHT;
		break;

	case CSNODE_CDIN_VOLUME:
	case CSNODE_SYNTH_VOLUME:
	case CSNODE_LINEIN_VOLUME:
	case IWNODE_MICIN_VOLUME:
		// Std. attenuation/gain
		/* Note:
		 The same applies here - windoze don't like positive values
		 *min_db = 12 << 16;
		 *max_db = (-CGAIN_GAIN_MASK * 3 << 15) + *min_db;
		 */
		*min_db = 0;										// 12 dB - 12
		*max_db = -CGAIN_GAIN_MASK * 3 << 15;				// -34.5 dB - 12 (1.5 dB step)
		*max_val = CGAIN_GAIN_MASK;
		if (node == CSNODE_CDIN_VOLUME)
			{
			*lreg = CREG_CDIN_LEFT;
			*rreg = CREG_CDIN_RIGHT;
			}
		else if (node == CSNODE_SYNTH_VOLUME)
			{
			*lreg = CREG_SYNTH_LEFT;
			*rreg = CREG_SYNTH_RIGHT;
			}
		else if (node == CSNODE_LINEIN_VOLUME)
			{
			*lreg = CREG_LINEIN_LEFT;
			*rreg = CREG_LINEIN_RIGHT;
			}
		else
			{
			// The one who wrote this is stupid
			status = STATUS_INVALID_PARAMETER;
			}
		break;

	case IWNODE_LINEOUT_VOLUME:
		// Master attenuation
		*min_db = 0;										// 0 dB
		*max_db = -COUT_ATTN_MASK * 3 << 15;				// -46.5 dB (1.5 dB step)
		*max_val = COUT_ATTN_MASK;
		*lreg = CREG_LINEOUT_LEFT;
		*rreg = CREG_LINEOUT_RIGHT;
		break;

	default:
		status = STATUS_INVALID_PARAMETER;
		break;
		}

	return status;
	}


/* Volume control
 */
NTSTATUS CGF1Topology::prophandler_volume
	(
	IN PPCPROPERTY_REQUEST		request
	)

	{
	PAGED_CODE();

	NTSTATUS		status = STATUS_INVALID_PARAMETER;
	CGF1Topology *	self;

	ASSERT (request);

	self = (CGF1Topology *) request->MajorTarget;
	ASSERT (self);

	if (request->Node >= 0 && request->Node < self->nodes)
		{
		// Node OK

		if (request->Verb & KSPROPERTY_TYPE_GET)
			{
			// Get property
			if (request->PropertyItem->Id != KSPROPERTY_AUDIO_VOLUMELEVEL)
				return STATUS_INVALID_PARAMETER;
			if (request->InstanceSize >= sizeof (LONG) &&
				request->ValueSize >= sizeof (LONG))
				{
				LONG	channel		= *((PLONG) request->Instance);
				PLONG	db			= (PLONG) request->Value;
				LONG	min_db;
				LONG	max_db;
				BYTE	max_val;
				BYTE	lr;
				BYTE	rr;

				// Get node info
				status = volume_info (request->Node, &min_db, &max_db, &max_val, &lr, &rr);
				if (NT_SUCCESS (status))
					{
					// Get volumes
					BYTE val;

					if (channel == CHANNEL_LEFT && lr != (BYTE) -1)
						{
						if (self->node_info[request->Node].left_valid)
							{
							// Read from cache
							*db = self->node_info[request->Node].volume.lvol;
							}
						else
							{
							// Read from regs
							val = self->gf1_common->get_codec_reg (lr) & max_val;
							*db = ((LONG) val) * (max_db - min_db) / max_val + min_db;

							// Update cache
							self->node_info[request->Node].left_valid = TRUE;
							self->node_info[request->Node].volume.lvol = *db;
							}
						}
					else if (channel == CHANNEL_RIGHT && rr != (BYTE) -1)
						{
						if (self->node_info[request->Node].right_valid)
							{
							// Read from cache
							*db = self->node_info[request->Node].volume.rvol;
							}
						else
							{
							// Read from regs
							val = self->gf1_common->get_codec_reg (rr) & max_val;
							*db = ((LONG) val) * (max_db - min_db) / max_val + min_db;

							// Update cache
							self->node_info[request->Node].right_valid = TRUE;
							self->node_info[request->Node].volume.rvol = *db;
							}
						}
					else
						{
						return STATUS_INVALID_PARAMETER;
						}
					status = STATUS_SUCCESS;
					}
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}

		else if (request->Verb & KSPROPERTY_TYPE_SET)
			{
			// Set property
			if (request->PropertyItem->Id != KSPROPERTY_AUDIO_VOLUMELEVEL)
				return STATUS_INVALID_PARAMETER;
			if (request->InstanceSize >= sizeof (LONG) &&
				request->ValueSize >= sizeof (LONG))
				{
				LONG	channel		= *((PLONG) request->Instance);
				LONG	db			= *((PLONG) request->Value);
				LONG	min_db;
				LONG	max_db;
				BYTE	max_val;
				BYTE	lr;
				BYTE	rr;

				// Get node info
				status = volume_info (request->Node, &min_db, &max_db, &max_val, &lr, &rr);
				if (NT_SUCCESS (status))
					{
					// Write value
					LONG		val;
					BOOLEAN		apply;
					CSMuxIn		src;

					apply = TRUE;
					if (request->Node == CSNODE_SYNTH_MUX_VOLUME)
						src = CSMUXIN_SYNTH;
					else if (request->Node == CSNODE_LINEIN_MUX_VOLUME)
						src = CSMUXIN_LINE;
					else if (request->Node == CSNODE_MICIN_MUX_VOLUME)
						src = CSMUXIN_MIC;
					else if (request->Node == CSNODE_MIXER_MUX_VOLUME)
						src = CSMUXIN_MIXER;
					else
						src = (CSMuxIn) 0;

					if (src != (CSMuxIn) 0)
						{
						// Fake pre-adc volume - is this active?
						if (!self->node_info[(ULONG) CSNODE_WAVEIN_MUX].left_valid ||
							self->node_info[(ULONG) CSNODE_WAVEIN_MUX].mux.source != src)
							{
							// Nope
							apply = FALSE;
							}
						}

					if (db < min (min_db, max_db))
						db = min (min_db, max_db);
					else if (db > max (min_db, max_db))
						db = max (min_db, max_db);
					val = (db - min_db) * max_val / (max_db - min_db);
					if (val < 0)
						val = 0;
					else if (val > max_val)
						val = max_val;
					status = STATUS_INVALID_PARAMETER;
					if ((channel == CHANNEL_LEFT || channel == CHANNEL_MASTER) && lr != (BYTE) -1)
						{
						status = STATUS_SUCCESS;
						if (apply)
							self->gf1_common->set_codec_reg (lr, ~max_val, (BYTE) val);
						self->node_info[request->Node].left_valid = TRUE;
						self->node_info[request->Node].volume.lvol = db;
						}
					if ((channel == CHANNEL_RIGHT || channel == CHANNEL_MASTER) && rr != (BYTE) -1)
						{
						status = STATUS_SUCCESS;
						if (apply)
							self->gf1_common->set_codec_reg (rr, ~max_val, (BYTE) val);
						self->node_info[request->Node].right_valid = TRUE;
						self->node_info[request->Node].volume.rvol = db;
						}
					}
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}

		else if (request->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
			{
			if (request->ValueSize >= sizeof (KSPROPERTY_DESCRIPTION))
				{
				// Basic description
				PKSPROPERTY_DESCRIPTION pd = (PKSPROPERTY_DESCRIPTION) request->Value;

				pd->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
					KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
				pd->DescriptionSize = sizeof (KSPROPERTY_DESCRIPTION) +
					sizeof (KSPROPERTY_MEMBERSHEADER) + sizeof (KSPROPERTY_STEPPING_LONG);
				pd->PropTypeSet.Set = KSPROPTYPESETID_General;
				pd->PropTypeSet.Id = VT_I4;
				pd->PropTypeSet.Flags = 0;
				pd->MembersListCount = 0;
				pd->Reserved = 0;

				if (request->ValueSize >= sizeof (KSPROPERTY_DESCRIPTION) +
					sizeof (KSPROPERTY_MEMBERSHEADER) + sizeof (KSPROPERTY_STEPPING_LONG))
					{
					// Extended info
					PKSPROPERTY_MEMBERSHEADER members = (PKSPROPERTY_MEMBERSHEADER) (pd + 1);
					PKSPROPERTY_STEPPING_LONG range = (PKSPROPERTY_STEPPING_LONG) (members + 1);
					LONG	min_db;
					LONG	max_db;
					BYTE	max_val;
					BYTE	lr;
					BYTE	rr;

					members->MembersFlags = KSPROPERTY_MEMBER_STEPPEDRANGES;
					members->MembersSize = sizeof (KSPROPERTY_STEPPING_LONG);
					members->MembersCount = 1;
					members->Flags = 0;

					status = volume_info (request->Node, &min_db, &max_db, &max_val, &lr, &rr);
					if (min_db < max_db)
						{
						range->Bounds.SignedMinimum = min_db;
						range->Bounds.SignedMaximum = max_db;
						range->SteppingDelta = (max_db - min_db) / max_val;
						}
					else
						{
						range->Bounds.SignedMinimum = max_db;
						range->Bounds.SignedMaximum = min_db;
						range->SteppingDelta = (min_db - max_db) / max_val;
						}
					range->Reserved = 0;

					request->ValueSize = sizeof (KSPROPERTY_DESCRIPTION) +
						sizeof (KSPROPERTY_MEMBERSHEADER) + sizeof (KSPROPERTY_STEPPING_LONG);
					}
				else
					{
					request->ValueSize = sizeof (KSPROPERTY_DESCRIPTION);
					}

				status = STATUS_SUCCESS;
				}
			else if (request->ValueSize >= sizeof (ULONG))
				{
				// Access flags
				PULONG af = (PULONG) request->Value;
				*af = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
				request->ValueSize = sizeof (LONG);
				status = STATUS_SUCCESS;
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}
		}

	DAILY_DEBUG (self, L"CGF1Topology::prophandler_volume");

	return status;
	}


#pragma code_seg ("PAGE")


/* CS4231 record multiplexer - select recording source
 N: 'Instance' (channel) is NOT used - all channels always go to the same destination.
	(although it's not that clear from docs; actually, there is nothing about this stuff
	in ddk docs)
 */
NTSTATUS CGF1Topology::prophandler_mux_source
	(
	IN PPCPROPERTY_REQUEST		request
	)

	{
	PAGED_CODE();

	NTSTATUS		status = STATUS_INVALID_PARAMETER;
	CGF1Topology *	self;

	ASSERT (request);

	self = (CGF1Topology *) request->MajorTarget;
	ASSERT (self);

	if (request->Node >= 0 && request->Node < self->nodes)
		{
		// Node OK

		if (request->Verb & KSPROPERTY_TYPE_GET)
			{
			// Get property
			if (request->ValueSize >= sizeof (ULONG))
				{
				if (request->Node == CSNODE_WAVEIN_MUX)
					{
					// WaveIn muxer - read just left channel, settings for right should be the same
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUX_SOURCE)
						{
						if (!self->node_info[request->Node].left_valid)
							{
							BYTE reg = self->gf1_common->get_codec_reg (CREG_ADC_LEFT);
							CSMuxIn pin;

							switch (reg & CADC_SRC_MASK)
								{
							case CADC_SRC_SYNTH:
								pin = CSMUXIN_SYNTH;
								break;

							case CADC_SRC_LINE:
								pin = CSMUXIN_LINE;
								break;

							case CADC_SRC_MIC:
								pin = CSMUXIN_MIC;
								break;

							case CADC_SRC_MIXER:
							default:
								pin = CSMUXIN_MIXER;
								break;
								}

							self->node_info[request->Node].left_valid = TRUE;
							self->node_info[request->Node].mux.source = pin;
							}

						status = STATUS_SUCCESS;
						*((PULONG) (request->Value)) = (ULONG)
							self->node_info[request->Node].mux.source;
						request->ValueSize = sizeof (ULONG);
						}
					}
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}

		else if (request->Verb & KSPROPERTY_TYPE_SET)
			{
			// Set property
			if (request->ValueSize >= sizeof (ULONG))
				{
				CSMuxIn		pin = (CSMuxIn) *((PULONG) request->Value);

				if (request->Node == CSNODE_WAVEIN_MUX)
					{
					// WaveIn muxer
					if (request->PropertyItem->Id == KSPROPERTY_AUDIO_MUX_SOURCE)
						{
						BYTE	reg;
						ULONG	src_vnode;

						// Choose source
						switch (pin)
							{
						case CSMUXIN_SYNTH:
							reg = CADC_SRC_SYNTH;
							src_vnode = CSNODE_SYNTH_MUX_VOLUME;
							break;

						case CSMUXIN_LINE:
							reg = CADC_SRC_LINE;
							src_vnode = CSNODE_LINEIN_MUX_VOLUME;
							break;

						case CSMUXIN_MIC:
							reg = CADC_SRC_MIC;
							src_vnode = CSNODE_MICIN_MUX_VOLUME;
							break;

						case CSMUXIN_MIXER:
							reg = CADC_SRC_MIXER;
							src_vnode = CSNODE_MIXER_MUX_VOLUME;
							break;

						default:
							reg = (BYTE)-1;
							break;
							}

						if (reg != (BYTE)-1)
							{
							LONG	min_db;
							LONG	max_db;
							BYTE	max_val;
							BYTE	lr;
							BYTE	rr;

							// Change source
							self->gf1_common->set_codec_reg (CREG_ADC_LEFT, ~CADC_SRC_MASK, reg);
							self->gf1_common->set_codec_reg (CREG_ADC_RIGHT, ~CADC_SRC_MASK, reg);

							// Update cache
							self->node_info[request->Node].left_valid = TRUE;
							self->node_info[request->Node].mux.source = pin;

							// Change volume
							status = volume_info (src_vnode, &min_db, &max_db, &max_val, &lr, &rr);
							if (NT_SUCCESS (status))
								{
								LONG	db;
								LONG	val;

								// Left
								if (self->node_info[(ULONG) src_vnode].left_valid)
									db = self->node_info[src_vnode].volume.lvol;
								else
									db = 0;
								val = (db - min_db) * max_val / (max_db - min_db);
								if (val < 0)
									val = 0;
								else if (val > max_val)
									val = max_val;
								self->gf1_common->set_codec_reg (lr, ~max_val, (BYTE) val);

								// Right
								if (self->node_info[src_vnode].right_valid)
									db = self->node_info[src_vnode].volume.rvol;
								else
									db = 0;
								val = (db - min_db) * max_val / (max_db - min_db);
								if (val < 0)
									val = 0;
								else if (val > max_val)
									val = max_val;
								self->gf1_common->set_codec_reg (rr, ~max_val, (BYTE) val);
								}
							}
						}
					}
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}

		else if (request->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
			{
			// Request property support
			if (request->ValueSize >= sizeof (KSPROPERTY_DESCRIPTION))
				{
				// "Complete" description
				PKSPROPERTY_DESCRIPTION pd = (PKSPROPERTY_DESCRIPTION) request->Value;

				pd->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
					KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
				pd->DescriptionSize = sizeof (KSPROPERTY_DESCRIPTION);
				pd->PropTypeSet.Set = KSPROPTYPESETID_General;
				pd->PropTypeSet.Id = VT_UI4;
				pd->PropTypeSet.Flags = 0;
				pd->MembersListCount = 0;
				pd->Reserved = 0;

				request->ValueSize = sizeof (KSPROPERTY_DESCRIPTION);
				status = STATUS_SUCCESS;
				}
			else if (request->ValueSize >= sizeof (ULONG))
				{
				// Access flags
				PULONG af = (PULONG) request->Value;
				*af = KSPROPERTY_TYPE_BASICSUPPORT | KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET;
				request->ValueSize = sizeof (ULONG);
				status = STATUS_SUCCESS;
				}
			else
				{
				status = STATUS_BUFFER_TOO_SMALL;
				}
			}
		}

	DAILY_DEBUG (self, L"CGF1Topology::prophandler_mux_source");

	return status;
	}


#pragma code_seg ("PAGE")


/* Read mixer settings from registry
 */
void CGF1Topology::read_from_registry
	(
	void
	)

	{
	PCPROPERTY_REQUEST	req;
	ULONG				channel;

	ULONG				i;
	NTSTATUS			status;
	ULONG				val;

	req.MajorTarget		= (PUNKNOWN) this;
	req.MinorTarget		= NULL;
	req.Verb			= KSPROPERTY_TYPE_SET;
	req.InstanceSize	= sizeof (LONG);
	req.Instance		= &channel;
	req.Irp				= NULL;

	if (gf1_common->get_revision() >= GF1REVISION_MAX)
		{
		// CS4231 nodes
		for (i = 0; i < nodes; i++)
			{
			status = STATUS_UNSUCCESSFUL;
			req.Node = i;

			switch (i)
				{
			case CSNODE_WAVEOUT_MUTE:
			case CSNODE_SYNTH_MUTE:
			case CSNODE_CDIN_MUTE:
			case CSNODE_LINEIN_MUTE:
			case CSNODE_MICIN_MUTE:
			case CSNODE_LINEOUT_MUTE:
				{
				// Mute nodes
				BOOL	b;

				req.PropertyItem	= &prop_mute[0];
				req.ValueSize		= sizeof (BOOL);
				req.Value			= &b;

				if (cs_node_name[i].left)
					{
					status = gf1_common->reg_getint (GF1REGGET_CONFIG,
						cs_node_name[i].left, &val);

					// Apply left / mono mute
					if (!NT_SUCCESS (status))
						b = FALSE;
					else
						b = (val ? TRUE : FALSE);
					channel = CHANNEL_LEFT;
					prophandler_onoff (&req);
					}

				if (cs_node_name[i].right)
					{
					/* Note:
					 If we are in mode 2 and we try to access control which behaves as mono
					 in mode 2 and as stereo in mode 3, changes to right channel will be
					 rejected by property handler, which is what we want.
					 */
					status = gf1_common->reg_getint (GF1REGGET_CONFIG,
						cs_node_name[i].right, &val);

					// Apply right mute
					if (!NT_SUCCESS (status))
						b = FALSE;
					else
						b = (val ? TRUE : FALSE);
					channel = CHANNEL_RIGHT;
					prophandler_onoff (&req);
					}
				}//eob

				break;

			case CSNODE_WAVEOUT_VOLUME:
			case CSNODE_SYNTH_VOLUME:
			case CSNODE_CDIN_VOLUME:
			case CSNODE_LINEIN_VOLUME:
			case CSNODE_SYNTH_MUX_VOLUME:
			case CSNODE_LINEIN_MUX_VOLUME:
			case CSNODE_MICIN_MUX_VOLUME:
			case CSNODE_MIXER_MUX_VOLUME:
			case IWNODE_MICIN_VOLUME:
			case IWNODE_LINEOUT_VOLUME:
				{
				// Volume nodes
				LONG	db;

				req.PropertyItem	= &prop_volume[0];
				req.ValueSize		= sizeof (LONG);
				req.Value			= &db;

				if (cs_node_name[i].left)
					{
					status = gf1_common->reg_getint (GF1REGGET_CONFIG,
						cs_node_name[i].left, &val);

					// Apply left / mono volume
					if (!NT_SUCCESS (status))
						{
						// FIXME: other defaults than 0 dB?
						db = 0;
						}
					else
						{
						db = (LONG) val;
						}
					channel = CHANNEL_LEFT;
					prophandler_volume (&req);
					}

				if (cs_node_name[i].right)
					{
					status = gf1_common->reg_getint (GF1REGGET_CONFIG,
						cs_node_name[i].right, &val);

					// Apply right volume
					if (!NT_SUCCESS (status))
						{
						db = 0;
						}
					else
						{
						db = (LONG) val;
						}
					channel = CHANNEL_RIGHT;
					prophandler_volume (&req);
					}
				}//eob

				break;

			case CSNODE_WAVEIN_MUX:
				{
				// Mux nodes
				ULONG	src;

				req.PropertyItem	= &prop_mux[0];
				req.ValueSize		= sizeof (ULONG);
				req.Value			= &src;

				if (cs_node_name[i].left)
					{
					status = gf1_common->reg_getint (GF1REGGET_CONFIG,
						cs_node_name[i].left, &val);

					// Apply mux
					if (!NT_SUCCESS (status))
						src = (ULONG) CSMUXIN_MIXER;
					else
						src = (ULONG) val;

					channel = CHANNEL_LEFT;
					prophandler_mux_source (&req);
					}

				// No right mux
				}//eob

				break;

				}
			}
		}
	else
		{
		// GF1 nodes
		for (i = 0; i < nodes; i++)
			{
			status = STATUS_UNSUCCESSFUL;
			req.Node = i;

			switch (i)
				{
			case GF1NODE_LINEIN_MUTE:
			case GF1NODE_MICIN_MUTE:
			case GF1NODE_LINEOUT_MUTE:
				{
				// Mute nodes
				BOOL	b;

				req.PropertyItem	= &prop_mute[0];
				req.ValueSize		= sizeof (BOOL);
				req.Value			= &b;

				if (gf1_node_name[i].left)
					{
					status = gf1_common->reg_getint (GF1REGGET_CONFIG,
						gf1_node_name[i].left, &val);

					// Apply mute
					if (!NT_SUCCESS (status))
						b = FALSE;
					else
						b = (val ? TRUE : FALSE);
					channel = CHANNEL_LEFT;
					prophandler_onoff (&req);
					}

				// All mutes are mono on GF1
				}//eob

				break;

				}
			}
		}
	}


#pragma code_seg ("PAGE")


/* Write mixer config to registry
 */
void CGF1Topology::write_to_registry
	(
	void
	)

	{
	ULONG i;

	if (gf1_common->get_revision() >= GF1REVISION_MAX)
		{
		// CS4231 nodes
		for (i = 0; i < nodes; i++)
			{
			switch (i)
				{
			case CSNODE_WAVEOUT_MUTE:
			case CSNODE_SYNTH_MUTE:
			case CSNODE_CDIN_MUTE:
			case CSNODE_LINEIN_MUTE:
			case CSNODE_MICIN_MUTE:
			case CSNODE_LINEOUT_MUTE:
				// Mute nodes
				if (cs_node_name[i].left && node_info[i].left_valid)
					{
					gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].left,
						node_info[i].mute.lmute ? 1 : 0);
					}

				/* Take special care of right channels. If we're in mode 2, write some
				 mono mutes as both left and right (those which are stereo in mode 3)
				 */
				if (cs_node_name[i].right && (node_info[i].right_valid ||
					(node_info[i].left_valid &&
					 (i == CSNODE_MICIN_MUTE || i == CSNODE_LINEOUT_MUTE))))
					{
					if (node_info[i].right_valid)
						{
						gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].right,
							node_info[i].mute.rmute ? 1 : 0);
						}
					else
						{
						gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].right,
							node_info[i].mute.lmute ? 1 : 0);
						}
					}
				break;

			case CSNODE_WAVEOUT_VOLUME:
			case CSNODE_SYNTH_VOLUME:
			case CSNODE_CDIN_VOLUME:
			case CSNODE_LINEIN_VOLUME:
			case CSNODE_SYNTH_MUX_VOLUME:
			case CSNODE_LINEIN_MUX_VOLUME:
			case CSNODE_MICIN_MUX_VOLUME:
			case CSNODE_MIXER_MUX_VOLUME:
			case IWNODE_MICIN_VOLUME:
			case IWNODE_LINEOUT_VOLUME:
				// Volume nodes
				if (cs_node_name[i].left && node_info[i].left_valid)
					{
					gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].left,
						node_info[i].volume.lvol);
					}
				if (cs_node_name[i].right && node_info[i].right_valid)
					{
					gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].right,
						node_info[i].volume.rvol);
					}
				break;

			case CSNODE_WAVEIN_MUX:
				// Mux nodes
				if (cs_node_name[i].left && node_info[i].left_valid)
					{
					gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].left,
						node_info[i].mux.source);
					}
				break;
				}
			}
		}
	else
		{
		// GF1 nodes
		for (i = 0; i < nodes; i++)
			{
			switch (i)
				{
			case GF1NODE_LINEIN_MUTE:
			case GF1NODE_MICIN_MUTE:
			case GF1NODE_LINEOUT_MUTE:
				// Mute nodes
				if (cs_node_name[i].left && node_info[i].left_valid)
					{
					gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].left,
						node_info[i].mute.lmute ? 1 : 0);
					}
				if (cs_node_name[i].right && node_info[i].right_valid)
					{
					gf1_common->reg_putint (GF1REGPUT_CONFIG, cs_node_name[i].right,
						node_info[i].mute.rmute ? 1 : 0);
					}
				break;
				}
			}
		}
	}
