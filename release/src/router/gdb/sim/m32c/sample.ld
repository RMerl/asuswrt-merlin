/* sample2.ld --- linker script for sample2.x

Copyright (C) 2005, 2007 Free Software Foundation, Inc.
Contributed by Red Hat, Inc.

This file is part of the GNU simulators.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* See the 'sample2.x' target in Makefile.in.  */

ENTRY(_start)

MEMORY {
	RAM1 (w) : ORIGIN = 0xc800, LENGTH = 0x0200
	RAM2 (w) : ORIGIN = 0xca56, LENGTH = 0x1000
	ROM  (w) : ORIGIN = 0x30000, LENGTH = 0x1000
}

SECTIONS {
	.data : {
		*(.data*)
	} > RAM1
	.text : {
		*(.text*)
	} > RAM2
	.fardata : {
		*(.fardata*)
	} > ROM
}
