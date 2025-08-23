Gravis UltraSound WDM Driver Project - Release Notes
====================================================

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

See file "COPYING" for license details.

If you'd like to contribute to the project, see "future.txt" and "todo.txt".



Table of Contents
=================

1. About BBSGF1 Driver
2. System Requirements
3. Compiling BBSGF1 Driver
4. Installing the Driver
5. Using the Driver
6. Contacts



1. About BBSGF1 Driver
======================

This is WDM driver for Gravis UltraSound sound cards. It works
under Windows 2000 (any i386 version) and Windows 98 Second Edition.
See "features.txt" if want to see what should work in current version.
Don't hesitate to contact me at my gus_wdm@come.to.



2. System Requirements
======================

To use these drivers, you will need:

- Windows 98 SE or Windows 2000 (i386) OS
- Any Gravis UltraSound card



3. Compiling BBSGF1 Driver
==========================

You will have to install Windows 2000 DDK and everything it requires
first. MSVC 6.0 + SP3 was used to compile all versions up to 0.9.
Copy all sources to "<ddk>/src/wdm/audio/gus" and use "build" to
compile everything (see DDK for more info).

Files "bbsgf1.inf" and "bbsgf1.sys" are required to install the driver.



4. Installing the Driver
========================

You should probably already know how to install Windows drivers if you
want to play with this. You will need two files - "bbsgf1.inf" and
"bbsgf1.sys".

You will have to assign resources manually unless you have GUS PnP card.
Make sure that all port ranges are set correctly (for example, if your
"base"  address is 240, set the ranges to 240-24f, 340-34f and 746-746).
It's recommended to use two different 16bit DMA channels (numbers >= 4)
and single (shared) IRQ.

Note1:
Do not try to install this under non-SE Windows 98.

Note2:
Sometimes Windows98 do not ask you to restart your computer. The driver
is started correctly in this case, but application usually can't use
it until computer is restarted.



5. Using the Driver
===================

First check that the driver found your card and is working. You should
hear nasty pops when Windows is starting, otherwise something is wrong.
Also check device manager, sounds & multimedia, volume controls and
other easy ways to see if driver works and if it's seen by applications.

Current release is actually debugging build of the driver, so you
can find various info about driver state in the registry. Use registry
and try to find it. The easiest way is probably to search for string
like "BoardRevision" or "DRAMSize". In W2k it should be located
somewhere under:
 "HKLM/SYSTEM/CurrentControlSet/Enum/xxx"
where xxx is
 "ISAPNP/GRV0001_DEV0000/FFFFFFFF/Device Parameters" (PnP)
or
 "Root/Media/something" (non-PnP)
Make sure it's not backup-like subkey.
You should see up to three subkeys there:
 "Settings" - mixer settings
 "Info" - standard information about board. If you see "ErrorString" value
  there, you have a problem. Search sources to see what happened :-( or
  send me mail (with as much info as possible, and attach
  "Device Parameters" registry export).
 "DebugInfo" - debugging information. This should probably not be included
  in public releases, but it is. I am not going to explain what all those
  values mean, but here is a short list:
  - "ResetSucceededAfterSeveralTries" should be 0
  - If playback is stuck, check "GF1WaveOutState", if recording is stuck,
    check "GF1WaveInState". If it says "... jammed", you have a problem.
  Please contact me if you encounter one of those problems.

If you are lucky, everything should work (at least a bit). There are
still problems with annoying clicks (even on Max/PnP, which is weird,
because our implementation is nearly the same as SB16 example).
You won't see many volume controls if you have one of "classic" boards,
even Max does not have master lineout volume slider. But Q3A and NFS5
work quite well, so what do you want?



6. Contacts
===========

You still don't like something? Then join our ever-growing team of
master programmers and contribute to this great project. Contact
me at

 gus_wdm@come.to

Official homepage at

 come.to/gus_wdm


-- Stoupa
