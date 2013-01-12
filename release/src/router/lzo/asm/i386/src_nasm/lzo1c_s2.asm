;  lzo1c_s2.asm -- lzo1c_decompress_asm_safe
;
;  This file is part of the LZO real-time data compression library.
;
;  Copyright (C) 2011 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2010 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2009 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2008 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2007 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2006 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2005 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2004 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2003 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2002 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2001 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 2000 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 1999 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 1998 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 1997 Markus Franz Xaver Johannes Oberhumer
;  Copyright (C) 1996 Markus Franz Xaver Johannes Oberhumer
;  All Rights Reserved.
;
;  The LZO library is free software; you can redistribute it and/or
;  modify it under the terms of the GNU General Public License as
;  published by the Free Software Foundation; either version 2 of
;  the License, or (at your option) any later version.
;
;  The LZO library is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with the LZO library; see the file COPYING.
;  If not, write to the Free Software Foundation, Inc.,
;  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
;
;  Markus F.X.J. Oberhumer
;  <markus@oberhumer.com>
;  http://www.oberhumer.com/opensource/lzo/
;

; /***** DO NOT EDIT - GENERATED AUTOMATICALLY *****/

%include "asminit.def"
%ifdef NAME1
globalf(NAME1(lzo1c_decompress_asm_safe))
%endif
%ifdef NAME2
globalf(NAME2(lzo1c_decompress_asm_safe))
%endif
%ifdef NAME1
NAME1(lzo1c_decompress_asm_safe):
%endif
%ifdef NAME2
NAME2(lzo1c_decompress_asm_safe):
%endif
db 85,87,86,83,81,82,131,236,12,252,139,116,36,40,139,124
db 36,48,189,3,0,0,0,141,70,253,3,68,36,44,137,68
db 36,4,137,248,139,84,36,52,3,2,137,4,36,141,118,0
db 49,192,138,6,70,60,32,115,40,8,192,116,99,137,193,141
db 28,15,57,28,36,15,130,107,1,0,0,141,28,14,57,92
db 36,4,15,130,87,1,0,0,243,164,138,6,70,60,32,114
db 127,60,64,15,130,169,0,0,0,137,193,36,31,141,87,255
db 193,233,5,41,194,138,6,70,193,224,5,41,194,65,135,242
db 59,116,36,48,15,130,51,1,0,0,141,28,15,57,28,36
db 15,130,32,1,0,0,243,164,137,214,235,148,141,116,38,0
db 138,6,70,141,72,32,60,248,114,149,185,24,1,0,0,44
db 248,116,6,145,48,192,211,224,145,141,28,15,57,28,36,15
db 130,241,0,0,0,141,28,14,57,92,36,4,15,130,221,0
db 0,0,243,164,233,87,255,255,255,141,180,38,0,0,0,0
db 141,87,255,41,194,138,6,70,193,224,5,41,194,135,242,59
db 116,36,48,15,130,196,0,0,0,141,95,4,57,28,36,15
db 130,177,0,0,0,164,164,164,137,214,164,49,192,233,72,255
db 255,255,36,31,137,193,117,26,177,31,138,6,70,8,192,117
db 15,129,193,255,0,0,0,235,241,141,180,38,0,0,0,0
db 1,193,138,6,70,137,195,36,63,137,250,41,194,138,6,70
db 193,224,6,41,194,57,250,116,41,135,214,141,73,3,59,116
db 36,48,114,105,141,4,15,57,4,36,114,90,243,164,137,214
db 49,192,193,235,6,137,217,15,133,210,254,255,255,233,190,254
db 255,255,131,249,1,15,149,192,59,60,36,119,57,139,84,36
db 40,3,84,36,44,57,214,119,38,114,29,43,124,36,48,139
db 84,36,52,137,58,247,216,131,196,12,90,89,91,94,95,93
db 195,184,1,0,0,0,235,227,184,8,0,0,0,235,220,184
db 4,0,0,0,235,213,184,5,0,0,0,235,206,184,6,0
db 0,0,235,199,141,182,0,0,0,0,141,191,0,0,0,0
%ifdef NAME1
globalf_end(NAME1(lzo1c_decompress_asm_safe))
%endif
%ifdef NAME2
globalf_end(NAME2(lzo1c_decompress_asm_safe))
%endif
