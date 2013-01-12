;  lzo1x_f2.asm -- lzo1x_decompress_asm_fast_safe
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
globalf(NAME1(lzo1x_decompress_asm_fast_safe))
%endif
%ifdef NAME2
globalf(NAME2(lzo1x_decompress_asm_fast_safe))
%endif
%ifdef NAME1
NAME1(lzo1x_decompress_asm_fast_safe):
%endif
%ifdef NAME2
NAME2(lzo1x_decompress_asm_fast_safe):
%endif
db 85,87,86,83,81,82,131,236,12,252,139,116,36,40,139,124
db 36,48,189,3,0,0,0,141,70,253,3,68,36,44,137,68
db 36,4,137,248,139,84,36,52,3,2,137,4,36,49,192,49
db 219,172,60,17,118,55,44,14,235,62,5,255,0,0,0,141
db 84,6,18,57,84,36,4,15,130,78,2,0,0,138,30,70
db 8,219,116,230,141,68,24,21,235,30,141,182,0,0,0,0
db 57,116,36,4,15,130,49,2,0,0,138,6,70,60,16,115
db 119,8,192,116,216,131,192,6,141,84,7,253,57,20,36,15
db 130,29,2,0,0,141,84,6,253,57,84,36,4,15,130,8
db 2,0,0,137,193,49,232,193,233,2,33,232,139,22,131,198
db 4,137,23,131,199,4,73,117,243,41,198,41,199,138,6,70
db 60,16,115,52,141,87,3,57,20,36,15,130,226,1,0,0
db 193,232,2,138,30,141,151,255,247,255,255,141,4,152,70,41
db 194,59,84,36,48,15,130,206,1,0,0,139,10,137,15,1
db 239,233,151,0,0,0,137,246,60,64,114,68,137,193,193,232
db 2,141,87,255,131,224,7,138,30,193,233,5,141,4,216,70
db 41,194,131,193,4,57,232,115,73,233,170,0,0,0,5,255
db 0,0,0,141,86,3,57,84,36,4,15,130,123,1,0,0
db 138,30,70,8,219,116,231,141,76,24,36,49,192,235,17,144
db 60,32,15,130,200,0,0,0,131,224,31,116,227,141,72,5
db 102,139,6,141,87,255,193,232,2,131,198,2,41,194,57,232
db 114,102,59,84,36,48,15,130,77,1,0,0,141,68,15,253
db 193,233,2,57,4,36,15,130,54,1,0,0,139,26,131,194
db 4,137,31,131,199,4,73,117,243,137,199,49,219,138,70,254
db 33,232,15,132,216,254,255,255,141,20,7,57,20,36,15,130
db 14,1,0,0,141,20,6,57,84,36,4,15,130,250,0,0
db 0,139,22,1,198,137,23,1,199,138,6,70,233,55,255,255
db 255,141,180,38,0,0,0,0,59,84,36,48,15,130,231,0
db 0,0,141,68,15,253,57,4,36,15,130,211,0,0,0,135
db 214,41,233,243,164,137,214,235,164,129,193,255,0,0,0,141
db 86,3,57,84,36,4,15,130,175,0,0,0,138,30,70,8
db 219,116,230,141,76,11,12,235,27,141,180,38,0,0,0,0
db 60,16,114,44,137,193,131,224,8,193,224,13,131,225,7,116
db 219,131,193,5,102,139,6,131,198,2,141,151,0,192,255,255
db 193,232,2,116,57,41,194,233,38,255,255,255,141,116,38,0
db 141,87,2,57,20,36,114,106,193,232,2,138,30,141,87,255
db 141,4,152,70,41,194,59,84,36,48,114,93,138,2,136,7
db 138,90,1,136,95,1,131,199,2,233,31,255,255,255,131,249
db 6,15,149,192,59,60,36,119,57,139,84,36,40,3,84,36
db 44,57,214,119,38,114,29,43,124,36,48,139,84,36,52,137
db 58,247,216,131,196,12,90,89,91,94,95,93,195,184,1,0
db 0,0,235,227,184,8,0,0,0,235,220,184,4,0,0,0
db 235,213,184,5,0,0,0,235,206,184,6,0,0,0,235,199
%ifdef NAME1
globalf_end(NAME1(lzo1x_decompress_asm_fast_safe))
%endif
%ifdef NAME2
globalf_end(NAME2(lzo1x_decompress_asm_fast_safe))
%endif
