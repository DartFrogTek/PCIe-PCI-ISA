/*
 * dramdma.h
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

#ifndef _DRAMDMA_H_
#define _DRAMDMA_H_


// Maximum number of pending requests
#define GF1DDMA_REQUESTS		64


/* DRAM DMA download request list item
 */
struct GF1DDRequest
	{
	// DRAM destination address (dram-aligned)
	ULONG				dram_address;

	// Source virtual system address
	PVOID				address;

	// MDL describing source (may be NULL)
	PMDL				mdl;

	// Number of bytes to transfer (dram-aligned)
	ULONG				length;

	// Decrement this when transfer is done (may be NULL)
	PLONG				counter;

	// Signal this when transfer is done (may be NULL)
	PRKEVENT			event;

	// GF1 DMA flags (GF1DMA_DATA16, GF1DMA_UNSIGNED)
	BYTE				dma_flags;

	// Source memory type
	BYTE				memory_type;

	// Nonpaged kernel memory, MDL managed by driver
	#define GF1DDMA_TYPE_NONPAGED		0x01

	// Note: in future user paged memory may be used for DMusicDls
	};


#endif /* _DRAMDMA_H_ */
