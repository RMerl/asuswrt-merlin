#ifndef GCC_TM_H
#define GCC_TM_H
#define TARGET_CPU_DEFAULT (TARGET_CPU_generic)
#ifndef LIBC_GLIBC
# define LIBC_GLIBC 1
#endif
#ifndef LIBC_UCLIBC
# define LIBC_UCLIBC 2
#endif
#ifndef LIBC_BIONIC
# define LIBC_BIONIC 3
#endif
#ifdef IN_GCC
# include "options.h"
# include "insn-constants.h"
# include "config/dbxelf.h"
# include "config/elfos.h"
# include "config/arm/unknown-elf.h"
# include "config/arm/elf.h"
# include "config/arm/bpabi.h"
# include "config/../../libgcc/config/arm/bpabi-lib.h"
# include "config/newlib-stdint.h"
# include "config/arm/nocrt0.h"
# include "config/arm/aout.h"
# include "config/arm/arm.h"
#endif
#if defined IN_GCC && !defined GENERATOR_FILE && !defined USED_FOR_TARGET
# include "insn-flags.h"
#endif
# include "defaults.h"
#endif /* GCC_TM_H */
