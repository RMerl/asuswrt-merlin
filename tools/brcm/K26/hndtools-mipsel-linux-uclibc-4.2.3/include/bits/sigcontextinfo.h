/* Copyright (C) 2000, 2001, 2003, 2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Andreas Jaeger <aj@suse.de>, 2000.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */


#include <sgidefs.h>

#if _MIPS_SIM == _ABIO32

#define SIGCONTEXT unsigned long _code, struct sigcontext *
#define SIGCONTEXT_EXTRA_ARGS _code,
#define GET_PC(ctx)	((void *) ctx->sc_pc)
#define GET_FRAME(ctx)	((void *) ctx->sc_regs[30])
#define GET_STACK(ctx)	((void *) ctx->sc_regs[29])
#define CALL_SIGHANDLER(handler, signo, ctx) \
  (handler)((signo), SIGCONTEXT_EXTRA_ARGS (ctx))

#else

#define SIGCONTEXT unsigned long _code, ucontext_t *
#define SIGCONTEXT_EXTRA_ARGS _code,
#define GET_PC(ctx)	((void *) ctx->uc_mcontext.pc)
#define GET_FRAME(ctx)	((void *) ctx->uc_mcontext.gregs[30])
#define GET_STACK(ctx)	((void *) ctx->uc_mcontext.gregs[29])
#define CALL_SIGHANDLER(handler, signo, ctx) \
  (handler)((signo), SIGCONTEXT_EXTRA_ARGS (ctx))

#endif
