/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


/* Instruction field macros:

   The macro's below greatly simplify the process of translating the
   pseudo code found in the PowerPC manual into C.

   In addition to the below, more will be found in the gen program's
   cache table */


/* map some statements and variables directly across */

#define is_64bit_implementation (WITH_TARGET_WORD_BITSIZE == 64)
#define is_64bit_mode           IS_64BIT_MODE(processor)

#define NIA nia
#define CIA cia


/* reservation */

#define RESERVE        cpu_reservation(processor)->valid
#define RESERVE_ADDR   cpu_reservation(processor)->addr
#define RESERVE_DATA   cpu_reservation(processor)->data

#define real_addr(EA, IS_READ)  vm_real_data_addr(cpu_data_map(processor), \
						  EA, \
						  IS_READ, \
						  processor, \
						  cia)


/* depending on mode return a 32 or 64bit number */

#define IEA(X)       (is_64bit_mode \
		      ? (X) \
		      : MASKED((X), 32, 63))

/* Expand argument to current architecture size */

#define EXTS(X)		EXTS_##X


/* Gen translates text of the form A{XX:YY} into A_XX_YY_ the macro's
   below define such translated text into real expressions */

/* the spr field as it normally is used */

#define SPR_5_9_ (SPR & 0x1f)
#define SPR_0_4_ (SPR >> 5)
#define SPR_0_ ((SPR & BIT10(0)) != 0)

#define tbr_5_9_ (tbr & 0x1f)
#define tbr_0_4_ (tbr >> 5)


#define TB cpu_get_time_base(processor)


/* various registers with important masks */

#define LR_0b00		(LR & ~3)
#define CTR_0b00	(CTR & ~3)

#define CR_BI_  ((CR & BIT32_BI) != 0)
#define CR_BA_	((CR & BIT32_BA) != 0)
#define CR_BB_	((CR & BIT32_BB) != 0)


/* extended extracted fields */

#define TO_0_ ((TO & BIT5(0)) != 0)
#define TO_1_ ((TO & BIT5(1)) != 0)
#define TO_2_ ((TO & BIT5(2)) != 0)
#define TO_3_ ((TO & BIT5(3)) != 0)
#define TO_4_ ((TO & BIT5(4)) != 0)

#define BO_0_ ((BO & BIT5(0)) != 0)
#define BO_1_ ((BO & BIT5(1)) != 0)
#define BO_2_ ((BO & BIT5(2)) != 0)
#define BO_3_ ((BO & BIT5(3)) != 0)
#define BO_4_ ((BO & BIT5(4)) != 0)

#define GOTO(dest)   goto XCONCAT4(label__,dest,__,MY_PREFIX)
#define LABEL(dest)  XCONCAT4(label__,dest,__,MY_PREFIX)
