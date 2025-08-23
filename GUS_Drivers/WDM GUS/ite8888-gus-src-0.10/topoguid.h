/*
 * topoguid.h
 *
 * Topology node name guids
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



#ifndef _TOPOGUID_H_
#define _TOPOGUID_H_


// {E8A82259-4483-479b-95E5-879663E89806}
DEFINE_GUID (GUSAUDFNAME_MASTER_MIX,
	0xe8a82259, 0x4483, 0x479b, 0x95, 0xe5, 0x87, 0x96, 0x63, 0xe8, 0x98, 0x6);

// {DADF86BF-1CC5-4472-B325-5011FFD138B3}
DEFINE_GUID (GUSAUDFNAME_MIXER_IN,
	0xdadf86bf, 0x1cc5, 0x4472, 0xb3, 0x25, 0x50, 0x11, 0xff, 0xd1, 0x38, 0xb3);



#endif /* _TOPOGUID_H_ */
