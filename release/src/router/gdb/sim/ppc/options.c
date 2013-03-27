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

#ifndef _OPTIONS_C_
#define _OPTIONS_C_

#include "cpu.h"
#include "options.h"

STATIC_INLINE_OPTIONS\
(const char *)
options_byte_order (int order)
{
  switch (order) {
  case 0:		return "0";
  case BIG_ENDIAN:	return "BIG_ENDIAN";
  case LITTLE_ENDIAN:	return "LITTLE_ENDIAN";
  }

  return "UNKNOWN";
}

STATIC_INLINE_OPTIONS\
(const char *)
options_env (int env)
{
  switch (env) {
  case OPERATING_ENVIRONMENT:	return "OPERATING";
  case VIRTUAL_ENVIRONMENT:	return "VIRTUAL";
  case USER_ENVIRONMENT:	return "USER";
  case 0:			return "0";
  }

  return "UNKNOWN";
}

STATIC_INLINE_OPTIONS\
(const char *)
options_align (int align)
{
  switch (align) {
  case NONSTRICT_ALIGNMENT:	return "NONSTRICT";
  case STRICT_ALIGNMENT:	return "STRICT";
  case 0:			return "0";
  }

  return "UNKNOWN";
}

STATIC_INLINE_OPTIONS\
(const char *)
options_float (int float_type)
{
  switch (float_type) {
  case SOFT_FLOATING_POINT:	return "SOFTWARE";
  case HARD_FLOATING_POINT:	return "HARDWARE";
  }

  return "UNKNOWN";
}

STATIC_INLINE_OPTIONS\
(const char *)
options_mon (int mon)
{
  switch (mon) {
  case MONITOR_INSTRUCTION_ISSUE|MONITOR_LOAD_STORE_UNIT:	return "ALL";
  case MONITOR_INSTRUCTION_ISSUE:				return "INSTRUCTION";
  case MONITOR_LOAD_STORE_UNIT:					return "MEMORY";
  case 0:							return "0";
  }

  return "UNKNOWN";
}

STATIC_INLINE_OPTIONS\
(const char *)
options_inline (int in)
{
  switch (in) {
  case /*0*/ 0:					return "0";
  case /*1*/ REVEAL_MODULE:			return "REVEAL_MODULE";
  case /*2*/ INLINE_MODULE:			return "INLINE_MODULE";
  case /*3*/ REVEAL_MODULE|INLINE_MODULE:	return "REVEAL_MODULE|INLINE_MODULE";
  case /*4*/ PSIM_INLINE_LOCALS:		return "PSIM_LOCALS_INLINE";
  case /*5*/ PSIM_INLINE_LOCALS|REVEAL_MODULE:	return "PSIM_INLINE_LOCALS|REVEAL_MODULE";
  case /*6*/ PSIM_INLINE_LOCALS|INLINE_MODULE:	return "PSIM_INLINE_LOCALS|INLINE_MODULE";
  case /*7*/ ALL_INLINE:			return "ALL_INLINE";
  }
  return "0";
}


INLINE_OPTIONS\
(void)
print_options (void)
{
#if defined(_GNUC_) && defined(__VERSION__)
  printf_filtered ("Compiled by GCC %s on %s %s\n", __VERSION__, __DATE__, __TIME__);
#else
  printf_filtered ("Compiled on %s %s\n", __DATE__, __TIME__);
#endif

  printf_filtered ("WITH_HOST_BYTE_ORDER     = %s\n", options_byte_order (WITH_HOST_BYTE_ORDER));
  printf_filtered ("WITH_TARGET_BYTE_ORDER   = %s\n", options_byte_order (WITH_TARGET_BYTE_ORDER));
  printf_filtered ("WITH_XOR_ENDIAN          = %d\n", WITH_XOR_ENDIAN);
  printf_filtered ("WITH_BSWAP               = %d\n", WITH_BSWAP);
  printf_filtered ("WITH_SMP                 = %d\n", WITH_SMP);
  printf_filtered ("WITH_HOST_WORD_BITSIZE   = %d\n", WITH_HOST_WORD_BITSIZE);
  printf_filtered ("WITH_TARGET_WORD_BITSIZE = %d\n", WITH_TARGET_WORD_BITSIZE);
  printf_filtered ("WITH_ENVIRONMENT         = %s\n", options_env(WITH_ENVIRONMENT));
  printf_filtered ("WITH_EVENTS              = %d\n", WITH_EVENTS);
  printf_filtered ("WITH_TIME_BASE           = %d\n", WITH_TIME_BASE);
  printf_filtered ("WITH_CALLBACK_MEMORY     = %d\n", WITH_CALLBACK_MEMORY);
  printf_filtered ("WITH_ALIGNMENT           = %s\n", options_align (WITH_ALIGNMENT));
  printf_filtered ("WITH_FLOATING_POINT      = %s\n", options_float (WITH_FLOATING_POINT));
  printf_filtered ("WITH_TRACE               = %d\n", WITH_TRACE);
  printf_filtered ("WITH_ASSERT              = %d\n", WITH_ASSERT);
  printf_filtered ("WITH_MON                 = %s\n", options_mon (WITH_MON));
  printf_filtered ("WITH_DEFAULT_MODEL       = %s\n", model_name[WITH_DEFAULT_MODEL]);
  printf_filtered ("WITH_MODEL               = %s\n", model_name[WITH_MODEL]);
  printf_filtered ("WITH_MODEL_ISSUE         = %d\n", WITH_MODEL_ISSUE);
  printf_filtered ("WITH_RESERVED_BITS       = %d\n", WITH_RESERVED_BITS);
  printf_filtered ("WITH_STDIO               = %d\n", WITH_STDIO);
  printf_filtered ("WITH_REGPARM             = %d\n", WITH_REGPARM);
  printf_filtered ("WITH_STDCALL             = %d\n", WITH_STDCALL);
  printf_filtered ("DEFAULT_INLINE           = %s\n", options_inline (DEFAULT_INLINE));
  printf_filtered ("SIM_ENDIAN_INLINE        = %s\n", options_inline (SIM_ENDIAN_INLINE));
  printf_filtered ("BITS_INLINE              = %s\n", options_inline (BITS_INLINE));
  printf_filtered ("CPU_INLINE               = %s\n", options_inline (CPU_INLINE));
  printf_filtered ("VM_INLINE                = %s\n", options_inline (VM_INLINE));
  printf_filtered ("CORE_INLINE              = %s\n", options_inline (CORE_INLINE));
  printf_filtered ("EVENTS_INLINE            = %s\n", options_inline (EVENTS_INLINE));
  printf_filtered ("MON_INLINE               = %s\n", options_inline (MON_INLINE));
  printf_filtered ("INTERRUPTS_INLINE        = %s\n", options_inline (INTERRUPTS_INLINE));
  printf_filtered ("REGISTERS_INLINE         = %s\n", options_inline (REGISTERS_INLINE));
  printf_filtered ("DEVICE_INLINE            = %s\n", options_inline (DEVICE_INLINE));
  printf_filtered ("SPREG_INLINE             = %s\n", options_inline (SPREG_INLINE));
  printf_filtered ("SEMANTICS_INLINE         = %s\n", options_inline (SEMANTICS_INLINE));
  printf_filtered ("IDECODE_INLINE           = %s\n", options_inline (IDECODE_INLINE));
  printf_filtered ("OPTIONS_INLINE           = %s\n", options_inline (OPTIONS_INLINE));
  printf_filtered ("OS_EMUL_INLINE           = %s\n", options_inline (OS_EMUL_INLINE));
  printf_filtered ("SUPPORT_INLINE           = %s\n", options_inline (SUPPORT_INLINE));

#ifdef OPCODE_RULES
  printf_filtered ("OPCODE rules             = %s\n", OPCODE_RULES);
#endif

#ifdef IGEN_FLAGS
  printf_filtered ("IGEN_FLAGS               = %s\n", IGEN_FLAGS);
#endif

#ifdef DGEN_FLAGS
  printf_filtered ("DGEN_FLAGS               = %s\n", DGEN_FLAGS);
#endif

  {
    static const char *const defines[] = {
#ifdef __GNUC__
      "__GNUC__",
#endif

#ifdef __STRICT_ANSI__
      "__STRICT_ANSI__",
#endif

#ifdef __CHAR_UNSIGNED__
      "__CHAR_UNSIGNED__",
#endif

#ifdef __OPTIMIZE__
      "__OPTIMIZE__",
#endif

#ifdef STDC_HEADERS
      "STDC_HEADERS",
#endif

#include "defines.h"

#ifdef HAVE_TERMIOS_CLINE
      "HAVE_TERMIOS_CLINE",
#endif

#ifdef HAVE_TERMIOS_STRUCTURE
      "HAVE_TERMIOS_STRUCTURE",
#endif

#ifdef HAVE_TERMIO_CLINE
      "HAVE_TERMIO_CLINE",
#endif

#ifdef HAVE_TERMIO_STRUCTURE
      "HAVE_TERMIO_STRUCTURE",
#endif

#ifdef HAVE_DEVZERO
      "HAVE_DEVZERO",
#endif
    };

    int i;
    int max_len = 0;
    int cols;

    for (i = 0; i < sizeof (defines) / sizeof (defines[0]); i++) {
      int len = strlen (defines[i]);
      if (len > max_len)
	max_len = len;
    }

    cols = 78 / (max_len + 2);
    if (cols < 0)
      cols = 1;

    printf_filtered ("\n#defines:");
    for (i = 0; i < sizeof (defines) / sizeof (defines[0]); i++) {
      const char *const prefix = ((i % cols) == 0) ? "\n" : "";
      printf_filtered ("%s  %s%*s", prefix, defines[i],
		       (((i == (sizeof (defines) / sizeof (defines[0])) - 1)
			 || (((i + 1) % cols) == 0))
			? 0
			: max_len + 4 - strlen (defines[i])),
		       "");
    }
    printf_filtered ("\n");
  }
}

#endif /* _OPTIONS_C_ */
