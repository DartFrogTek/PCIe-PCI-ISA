/*
 * common.h
 *
 * Every source should include this
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


#ifndef _COMMON_H_
#define _COMMON_H_

								   /****** configuration ******/

// Enable hacked debugging (registry writes to "DebugInfo", etc.)
#define GF1_DBG				1



							  /****** do not change this..... ******/

#ifndef GF1_DBG
#define GF1_DBG				0
#endif /* GF1_DBG */


#define PC_NEW_NAMES		1
#include "portcls.h"
//#include "DMusicKS.h"
//#include "ksdebug.h"


#endif /* _COMMON_H_ */
