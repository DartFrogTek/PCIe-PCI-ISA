/*
 * gf1tabs.cpp
 *
 * Various GUS tables
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


// Number of channels -> mixing rate
const ULONG gf1_voices2frequency[32-14+1] =
	{
	44100, 41160, 38587, 36317, 34300, 32494, 30870, 29400, 28063, 26843,
	25725, 24696, 23746, 22866, 22050, 21289, 20580, 19916, 19293
	};


// Recording frequencies (just few of them)
const ULONG gf1_recfreq[24] =
	{
	  4410,	  8232,	 11025,	 12348,
	 12600,	 13720,	 14700,	 15435,
	 17150,	 17640,	 20580,	 22050,
	 24696,	 25725,	 29400,	 30870,
	 34300,	 41160,	 44100,	 51450,
	 61740,	 68600,	 77175,	 88200
	};


// Frequencies supported by codec playback and record
const ULONG codec_freq[CODEC_FREQ_COUNT] =
	{
	 5512,  6620,  8000,  9600, 11025, 16000, 18900,
	22050, 27420, 32000, 33075, 37800, 44100, 48000
	};


const BYTE codec_freq_format[CODEC_FREQ_COUNT] =
	{
	0x00 | CDF_XTAL2,
	0x0e | CDF_XTAL2,
	0x00 | CDF_XTAL1,
	0x0e | CDF_XTAL1,
	0x02 | CDF_XTAL2,
	0x02 | CDF_XTAL1,
	0x04 | CDF_XTAL2,
	0x06 | CDF_XTAL2,
	0x04 | CDF_XTAL1,
	0x06 | CDF_XTAL1,
	0x0c | CDF_XTAL2,
	0x08 | CDF_XTAL2,
	0x0a | CDF_XTAL2,
	0x0c | CDF_XTAL1
	};


