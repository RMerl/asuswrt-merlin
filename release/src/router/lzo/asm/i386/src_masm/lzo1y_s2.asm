;  lzo1y_s2.asm -- lzo1y_decompress_asm_safe
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

include asminit.def
public _lzo1y_decompress_asm_safe
_lzo1y_decompress_asm_safe:
db 85,87,86,83,81,82,131,236,12,252,139,116,36,40,139,124
db 36,48,189,3,0,0,0,141,70,253,3,68,36,44,137,68
db 36,4,137,248,139,84,36,52,3,2,137,4,36,49,192,49
db 219,172,60,17,118,87,44,17,60,4,115,92,141,20,7,57
db 20,36,15,130,130,2,0,0,141,20,6,57,84,36,4,15
db 130,110,2,0,0,137,193,235,110,5,255,0,0,0,141,84
db 6,18,57,84,36,4,15,130,87,2,0,0,138,30,70,8
db 219,116,230,141,68,24,18,235,31,141,180,38,0,0,0,0
db 57,116,36,4,15,130,57,2,0,0,138,6,70,60,16,115
db 127,8,192,116,215,131,192,3,141,84,7,0,57,20,36,15
db 130,37,2,0,0,141,84,6,0,57,84,36,4,15,130,16
db 2,0,0,137,193,193,232,2,33,233,139,22,131,198,4,137
db 23,131,199,4,72,117,243,243,164,138,6,70,60,16,115,64
db 141,87,3,57,20,36,15,130,238,1,0,0,193,232,2,138
db 30,141,151,255,251,255,255,141,4,152,70,41,194,59,84,36
db 48,15,130,218,1,0,0,138,2,136,7,138,66,1,136,71
db 1,138,66,2,136,71,2,1,239,233,163,0,0,0,137,246
db 60,64,114,68,137,193,193,232,2,141,87,255,33,232,138,30
db 193,233,4,141,4,152,70,41,194,73,57,232,115,76,233,181
db 0,0,0,5,255,0,0,0,141,86,3,57,84,36,4,15
db 130,126,1,0,0,138,30,70,8,219,116,231,141,76,24,33
db 49,192,235,20,141,116,38,0,60,32,15,130,200,0,0,0
db 131,224,31,116,224,141,72,2,102,139,6,141,87,255,193,232
db 2,131,198,2,41,194,57,232,114,110,59,84,36,48,15,130
db 77,1,0,0,141,4,15,57,4,36,15,130,58,1,0,0
db 137,203,193,235,2,116,17,139,2,131,194,4,137,7,131,199
db 4,75,117,243,33,233,116,9,138,2,66,136,7,71,73,117
db 247,138,70,254,33,232,15,132,196,254,255,255,141,20,7,57
db 20,36,15,130,2,1,0,0,141,20,6,57,84,36,4,15
db 130,238,0,0,0,138,14,70,136,15,71,72,117,247,138,6
db 70,233,42,255,255,255,137,246,59,84,36,48,15,130,223,0
db 0,0,141,68,15,0,57,4,36,15,130,203,0,0,0,135
db 214,243,164,137,214,235,170,129,193,255,0,0,0,141,86,3
db 57,84,36,4,15,130,169,0,0,0,138,30,70,8,219,116
db 230,141,76,11,9,235,21,144,60,16,114,44,137,193,131,224
db 8,193,224,13,131,225,7,116,225,131,193,2,102,139,6,131
db 198,2,141,151,0,192,255,255,193,232,2,116,57,41,194,233
db 38,255,255,255,141,116,38,0,141,87,2,57,20,36,114,106
db 193,232,2,138,30,141,87,255,141,4,152,70,41,194,59,84
db 36,48,114,93,138,2,136,7,138,90,1,136,95,1,131,199
db 2,233,43,255,255,255,131,249,3,15,149,192,59,60,36,119
db 57,139,84,36,40,3,84,36,44,57,214,119,38,114,29,43
db 124,36,48,139,84,36,52,137,58,247,216,131,196,12,90,89
db 91,94,95,93,195,184,1,0,0,0,235,227,184,8,0,0
db 0,235,220,184,4,0,0,0,235,213,184,5,0,0,0,235
db 206,184,6,0,0,0,235,199,144,141,180,38,0,0,0,0
end
