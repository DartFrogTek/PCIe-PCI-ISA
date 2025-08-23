/*
 * bbsgf1.cpp
 *
 * Main adapter driver stuff
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


#define PUT_GUIDS_HERE

#include "gf1cmn.h"
#include "topo.h"
#include "gf1wave.h"
#include "cswave.h"
#include "topoguid.h"


// Maximum number of miniports registered via PcRegisterSubdevice
#define MAX_MINIPORTS			3


// Globals (FIXME: remove these somehow)
static PDEVICE_OBJECT g_physical_device = NULL;


// Forwards...
static NTSTATUS add_device (IN PDRIVER_OBJECT, IN PDEVICE_OBJECT);
static NTSTATUS start_device (IN PDEVICE_OBJECT, IN PIRP, IN PRESOURCELIST);


#pragma code_seg("INIT")


/* Driver entry
 I: driver_object - driver object
 I: registry_path - path to CurrentControlSet/Services/xxx
 */
extern "C" NTSTATUS DriverEntry
	(
	IN PDRIVER_OBJECT	driver_object,
	IN PUNICODE_STRING	registry_path
	)

	{
	PAGED_CODE();

	// Call portcls to initialize this driver
	return PcInitializeAdapterDriver (driver_object, registry_path, &add_device);
	}


#pragma code_seg("PAGE")


/* Device is added
 I: driver_object
 I: physical_device - pd object
 */
NTSTATUS add_device
	(
	IN PDRIVER_OBJECT	driver_object,
	IN PDEVICE_OBJECT   physical_device
	)

	{
	PAGED_CODE();

	NTSTATUS status;

	// Call portcls to add this device
	status = PcAddAdapterDevice (driver_object, physical_device, &start_device, MAX_MINIPORTS, 0);

	if (NT_SUCCESS (status))
		{
		/*
		 Portcls created functional device for physical device.

		 It probably stored 'physical_device' somewhere in device extension.
		 We will need this pointer to use dma directly (portcls dma wrappers suck),
		 but portcls device extension is top secret and we can't access our extension
		 yet (ptr to device object will be passed to 'start_device').

		 We store it in global variable, but that won't work with multiple devices.
		 */
		if (!g_physical_device)
			g_physical_device = physical_device;
		}

	return status;
	}


static const PCWSTR errmsg = L"ErrorMessage";


#define INFERR(s) if (!NT_SUCCESS (status)) { GF1_INFSTR_ (gf1_common, errmsg, s); }


/* Start all required ports/miniports
 I: device_object - created by PcAddAdapterDevice
 I: irp - IO request packet (used to initialize ports)
 I: resources - list of hardware resources to use
 */
NTSTATUS start_device
	(
	IN PDEVICE_OBJECT	device_object,
	IN PIRP				irp,
	IN PRESOURCELIST	resources
	)

	{
	PAGED_CODE();

	NTSTATUS 		status;
	PUNKNOWN		unknown_common	= NULL;
	IGF1Common *	gf1_common		= NULL;

	ASSERT (device_object);
	ASSERT (irp);
	ASSERT (resources);

	// Create adapter object
	status = create_gf1_common (&unknown_common, NonPagedPool);

	if (NT_SUCCESS (status))
		{
		ASSERT (unknown_common);

		// Get GF1 interface
		status = unknown_common->QueryInterface (IID_IGF1Common, (PVOID *) &gf1_common);

		if (NT_SUCCESS (status))
			{
			ASSERT (gf1_common);

			// Initialize GUS
			status = gf1_common->init (resources, g_physical_device, device_object);

			if (g_physical_device)
				{
				// Hey, we don't even know if it belonged to this device
				g_physical_device = NULL;
				}

			if (NT_SUCCESS (status))
				{
				// FIXME: Register power management!!!
				}
			}
		}

	if (!NT_SUCCESS (status))
		{
		if (gf1_common)
			gf1_common->Release();
		if (unknown_common)
			unknown_common->Release();
		return status;
		}

	// GF1Common is valid

	// Start ports/miniports
	{
	PPORT		port		= NULL;
	PUNKNOWN	topology	= NULL;
	PUNKNOWN	wave		= NULL;
	PUNKNOWN	miniport	= NULL;

	// Create topology port
	if (NT_SUCCESS (status))
		{
		status = PcNewPort (&port, CLSID_PortTopology);
		INFERR (L"Can't create topology port");
		}
	if (NT_SUCCESS (status))
		{
		port->QueryInterface (IID_IUnknown, (PVOID *) &topology);
		}

	if (NT_SUCCESS (status))
		{
		// Create miniport
		status = create_gf1_topology (&miniport, NonPagedPool);
		INFERR (L"Can't create topology miniport");

		if (NT_SUCCESS (status))
			{
			// Initialize port/miniport
			status = port->Init (device_object, irp, miniport, unknown_common, resources);
			INFERR (L"Can't initialize topology port");

			if (NT_SUCCESS (status))
				{
				// Register port to portcls
				status = PcRegisterSubdevice (device_object, L"Topology", port);
				INFERR (L"Can't register topology port");
				}

			miniport->Release();
			}

		// Release port (portcls has it or something failed)
		port->Release();
		}

	// Create wave port
	if (NT_SUCCESS (status))
		{
		status = PcNewPort (&port, CLSID_PortWaveCyclic);
		INFERR (L"Can't create wave port");
		}
	if (NT_SUCCESS (status))
		{
		port->QueryInterface (IID_IUnknown, (PVOID *) &wave);
		}

	if (NT_SUCCESS (status))
		{
		// Create miniport
		if (gf1_common->get_revision() < GF1REVISION_MAX)
			status = create_gf1_wave (&miniport, NonPagedPool);
		else
			status = create_codec_wave (&miniport, NonPagedPool);
		INFERR (L"Can't create wave miniport");

		if (NT_SUCCESS (status))
			{
			// Initialize port/miniport
			status = port->Init (device_object, irp, miniport, unknown_common, resources);
			INFERR (L"Can't initialize wave port");

			if (NT_SUCCESS (status))
				{
				// Register port to portcls
				status = PcRegisterSubdevice (device_object, L"Wave", port);
				INFERR (L"Can't register wave port");
				}

			miniport->Release();
			}

		// Release port (portcls has it or something failed)
		port->Release();
		}

	// FIXME: create dmusic port

	// Register connections
	if (topology)
		{
		if (wave)
			{
			if (gf1_common->get_revision() < GF1REVISION_MAX)
				{
				// Connect GF1 stuff
				status = PcRegisterPhysicalConnection (device_object, wave,
					GF1WPIN_WAVEOUT_BRIDGE, topology, GF1TPIN_WAVEOUT);
				if (NT_SUCCESS (status))
					{
					PcRegisterPhysicalConnection (device_object, topology,
						GF1TPIN_WAVEIN, wave, GF1WPIN_WAVEIN_BRIDGE);
					}
				INFERR (L"Can't register wave<->topology connection");
				}
			else
		   		{
				// Connect CS4231 stuff
				status = PcRegisterPhysicalConnection (device_object, wave,
					CODECWPIN_WAVEOUT_BRIDGE, topology, GF1TPIN_WAVEOUT);
				if (NT_SUCCESS (status))
					{
					PcRegisterPhysicalConnection (device_object, topology,
						GF1TPIN_WAVEIN, wave, CODECWPIN_WAVEIN_BRIDGE);
					}
				INFERR (L"Can't register wave<->topology connection");
				}
			}

		// FIXME: connect synth
		}

	// Release local port ptrs
	if (wave)
		wave->Release();
	if (topology)
		topology->Release();
	}//eob (port/miniport)

	// Cleanup
	if (unknown_common)
		{
		unknown_common->Release();
		}
	if (gf1_common)
		{
		// Ports have their references
		gf1_common->Release();
		}

	return status;
	}
