/*
 * iwinit.cpp
 *
 * InterWave initialization
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

/* Global InterWave reset
 N: Resets InterWave to enhanced mode and enables some features it supports.
    Currently it does:
	- Set IW to enhanced mode (doesn't reset IW to GUS compatible mode)
	- Set all voices to offset modes.
 */

void CGF1Common::iw_global_reset
	(
	void
	)
	{
	BYTE	i;

	iw_set_enhanced_mode();

	for (i=0; i<32; i++)
		{
		select_voice (i);
		iw_voice_set_offset_mode();
		}
	}

#pragma code_seg ("PAGE")

/* Reset InterWave to GUS compatible mode.
   Same as reseting GF1.
 */

void CGF1Common::iw_reset_to_gus_mode
	(
	void
	)
	{

	iw_mode = IW_GUS_COMPATIBLE_MODE;

	// Reset InterWave to GUS compatible mode.
	// Same as reseting GF1.
	iwrite8 (GF1REG_RESET, 0);
	GF1_DELAY (5);
	iwrite8 (GF1REG_RESET, GF1RES_RESET);
	GF1_DELAY (3);
	}

#pragma code_seg ("PAGE")

/* Set Interwave from enhanced mode back to GUS mode.
 */

void CGF1Common::iw_set_gus_mode
	(
	void
	)
	{

	iw_mode = IW_GUS_COMPATIBLE_MODE;

	BYTE reg = iread8 (IWREG_SYNTH_GLOB_MODE);
	reg &= (~IWSM_ENH_MODE);
	iwrite8 (IWREG_SYNTH_GLOB_MODE, reg);
	}

#pragma code_seg ("PAGE")

/* Set InterWave to enhanced mode.
 */
void CGF1Common::iw_set_enhanced_mode
	(
	void
	)
	{

	iw_mode = IW_ENHANCED_MODE;

	// Set InterWave to enhanced mode.
	BYTE reg = iread8 (IWREG_SYNTH_GLOB_MODE);
	iwrite8 (IWREG_SYNTH_GLOB_MODE, (BYTE)(reg | IWSM_ENH_MODE));
	}


#pragma code_seg ("PAGE")


/* Reset InterWave Codec
 */
void CGF1Common::iw_reset_codec
	(
	void
	)

	{
	ciwrite8 (CREG_CONFIG2, CCFG2_CENTER_DAC);
	ciwrite8 (CREG_CONFIG3, CCFG3_SYNTH_TO_AUX1 | CCFG3_PLAYBACK_IRQ | CCFG3_RECORD_IRQ);
	ciwrite8 (CREG_LINEOUT_LEFT, 0);
	ciwrite8 (CREG_LINEOUT_RIGHT, 0);
	}


#pragma code_seg ("PAGE")


/* Detect size of DRAM attached to InterWave.
   Before calling this func you should call iw_configure_mem(),
   otherwise this function will report only 256Kb of DRAM.
   N: If InterWave works in GUS compatible mode, max 1MB will
      be reported although more memory (up to 16MB) can be attached to InterWave.
 */

ULONG CGF1Common::iw_detect_dram_size
	(
	void
	)
	{
	BYTE	r;
	DWORD	local;
	ULONG	i;

	WORD	reg;

	// FIXME: Really needed?
	// Select DRAM as target of IO cycles.
	r = iread8 (IWREG_MEM_CTRL);
	r &= (~IWMEM_ACCESS_ROM);		// Access DRAM.
	iwrite8 (IWREG_MEM_CTRL, r);

	r		= 0x55;
	local	= 0;

	for (i=0; i<(IW_MAX_DRAM_SUPPORTED/IW_MEMORY_STEP); i++)
		{
		poke (local, r);
		poke (local+1, r+1);
		if (peek(local) != r || peek(local+1) != (r+1) || peek(0) != 0x55)
			{
			break;
			}
		local += IW_MEMORY_STEP;
		r++;
		}

	return (ULONG)local;
	}


#pragma code_seg ("PAGE")


/* Set up InterWave's DRAM bank configuration.
   Interwave can't detect size of DRAM attached to it by itself,
   so we must do it.
 */
void CGF1Common::iw_configure_mem
	(
	void
	)
	{
	ULONG	old_mode = iw_mode;			// Old InterWave mode.
	DWORD	bank[4] = {0, 0, 0, 0};		// Bank sizes in kbytes.
	WORD	reg;
	DWORD	base;
	DWORD	cnt;
	ULONG	i;

	// Set to enhacement mode to be able to access more then 1MB DRAM.
	iw_set_enhanced_mode();

	// Select max address span (16MB).
	reg = iread16 (IWREG_MEM_CONFIG);
	reg &= (~IWMEMCFG_DRAMCFG);									// Clear DRAM Configuration bits.
	iwrite16 (IWREG_MEM_CONFIG, reg | IWMEMCFG_DRAMCFG_4444);	// Max address span.

	// FIXME: I'm not sure with this NOTE, but it looks it is right.
	// NOTE; We have configured InterWave so it thinks that it has four 4MB banks.
	//		 So first bank is mapped at address 0, second is mapped at address 4<<20 (4MB),
	//		 third at 8<<20 (8MB) and fourth at 12<<20 (12 MB).
	//		 So if the first bank is 256 kbytes long, accessing address 256<<10 (256kb) will overlap and
	//		 returns value from address 0.

	// Clear every IW_MEMORY_STEPth location
	base = 0;
	while (base < IW_MAX_DRAM_SUPPORTED)
		{
		poke (base, 0x00);
		base += IW_MEMORY_STEP;
		}

	// Determine amount of RAM in each bank
	base = 0;
	for (i=0; i<4; i++)
	{
		poke (base, 0xaa);				// Start of bank.
		poke (base+1, 0x55);

		if (peek (base) == 0xaa && peek(base+1) == 0x55)
			{
			// Bank present.
			cnt  = 0;
			while (cnt < IW_MAX_DRAM_IN_BANK)
				{
				bank[i] += IW_MEMORY_STEP;
				cnt		+= IW_MEMORY_STEP;
				if (peek (base + cnt) == 0xaa)	// Bank overlap.
					break;
				}
			}
		bank[i] = bank[i] >> 10;
		base += IW_MAX_DRAM_IN_BANK;
	}

	// Say the right bank configuration to InterWave.
#define CHECK_BANKS(b3,b2,b1,b0)	\
	(bank[3] == (b3) && bank[2] == (b2) && bank[1] == (b1) && bank[0] == (b0))

	if (CHECK_BANKS (   0,    0,    0,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0002); else
	if (CHECK_BANKS (   0,    0,  256,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0022); else
	if (CHECK_BANKS ( 256,  256,  256,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_2222); else
	if (CHECK_BANKS (   0,    0, 1024,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0012); else
	if (CHECK_BANKS (1024, 1024, 1024,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_1112); else
	if (CHECK_BANKS (   0, 1024,  256,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0122); else
	if (CHECK_BANKS (1024, 1024,  256,  256)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_1122); else
	if (CHECK_BANKS (   0,    0,    0, 1024)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0001); else
	if (CHECK_BANKS (   0,    0, 1024, 1024)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0011); else
	if (CHECK_BANKS (1024, 1024, 1024, 1024)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_1111); else
	if (CHECK_BANKS (   0,    0,    0, 4096)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0004); else
	if (CHECK_BANKS (   0,    0, 4096, 4096)) iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_0044); else
											  iwrite16 (IWREG_MEM_CONFIG, IWMEMCFG_DRAMCFG_4444);

#undef CHECK_BANKS

	// Reset InterWave back to old mode.
	if (old_mode == IW_GUS_COMPATIBLE_MODE)
		{
		iw_set_gus_mode();
		}

	// Debug info.
	GF1_DBGINT (L"Bank0(kb)", bank[0]);
	GF1_DBGINT (L"Bank1(kb)", bank[1]);
	GF1_DBGINT (L"Bank2(kb)", bank[2]);
	GF1_DBGINT (L"Bank3(kb)", bank[3]);
	}
