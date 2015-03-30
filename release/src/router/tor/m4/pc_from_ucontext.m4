# This file is from Google Performance Tools, svn revision r226.
#
# The Google Performance Tools license is:
########
# Copyright (c) 2005, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
########
# Original file follows below.

# We want to access the "PC" (Program Counter) register from a struct
# ucontext.  Every system has its own way of doing that.  We try all the
# possibilities we know about.  Note REG_PC should come first (REG_RIP
# is also defined on solaris, but does the wrong thing).

# OpenBSD doesn't have ucontext.h, but we can get PC from ucontext_t
# by using signal.h.

# The first argument of AC_PC_FROM_UCONTEXT will be invoked when we
# cannot find a way to obtain PC from ucontext.

AC_DEFUN([AC_PC_FROM_UCONTEXT],
  [AC_CHECK_HEADERS(ucontext.h)
   # Redhat 7 has <sys/ucontext.h>, but it barfs if we #include it directly
   # (this was fixed in later redhats).  <ucontext.h> works fine, so use that.
   if grep "Red Hat Linux release 7" /etc/redhat-release >/dev/null 2>&1; then
     AC_DEFINE(HAVE_SYS_UCONTEXT_H, 0, [<sys/ucontext.h> is broken on redhat 7])
     ac_cv_header_sys_ucontext_h=no
   else
     AC_CHECK_HEADERS(sys/ucontext.h)       # ucontext on OS X 10.6 (at least)
   fi
   AC_CHECK_HEADERS(cygwin/signal.h)        # ucontext on cywgin
   AC_MSG_CHECKING([how to access the program counter from a struct ucontext])
   pc_fields="           uc_mcontext.gregs[[REG_PC]]"  # Solaris x86 (32 + 64 bit)
   pc_fields="$pc_fields uc_mcontext.gregs[[REG_EIP]]" # Linux (i386)
   pc_fields="$pc_fields uc_mcontext.gregs[[REG_RIP]]" # Linux (x86_64)
   pc_fields="$pc_fields uc_mcontext.sc_ip"            # Linux (ia64)
   pc_fields="$pc_fields uc_mcontext.uc_regs->gregs[[PT_NIP]]" # Linux (ppc)
   pc_fields="$pc_fields uc_mcontext.gregs[[R15]]"     # Linux (arm old [untested])
   pc_fields="$pc_fields uc_mcontext.arm_pc"           # Linux (arm arch 5)
   pc_fields="$pc_fields uc_mcontext.gp_regs[[PT_NIP]]"  # Suse SLES 11 (ppc64)
   pc_fields="$pc_fields uc_mcontext.mc_eip"           # FreeBSD (i386)
   pc_fields="$pc_fields uc_mcontext.mc_rip"           # FreeBSD (x86_64 [untested])
   pc_fields="$pc_fields uc_mcontext.__gregs[[_REG_EIP]]"  # NetBSD (i386)
   pc_fields="$pc_fields uc_mcontext.__gregs[[_REG_RIP]]"  # NetBSD (x86_64)
   pc_fields="$pc_fields uc_mcontext->ss.eip"          # OS X (i386, <=10.4)
   pc_fields="$pc_fields uc_mcontext->__ss.__eip"      # OS X (i386, >=10.5)
   pc_fields="$pc_fields uc_mcontext->ss.rip"          # OS X (x86_64)
   pc_fields="$pc_fields uc_mcontext->__ss.__rip"      # OS X (>=10.5 [untested])
   pc_fields="$pc_fields uc_mcontext->ss.srr0"         # OS X (ppc, ppc64 [untested])
   pc_fields="$pc_fields uc_mcontext->__ss.__srr0"     # OS X (>=10.5 [untested])
   pc_field_found=false
   for pc_field in $pc_fields; do
     if ! $pc_field_found; then
       # Prefer sys/ucontext.h to ucontext.h, for OS X's sake.
       if test "x$ac_cv_header_cygwin_signal_h" = xyes; then
         AC_TRY_COMPILE([#define _GNU_SOURCE 1
                         #include <cygwin/signal.h>],
                        [ucontext_t u; return u.$pc_field == 0;],
                        AC_DEFINE_UNQUOTED(PC_FROM_UCONTEXT, $pc_field,
                                           How to access the PC from a struct ucontext)
                        AC_MSG_RESULT([$pc_field])
                        pc_field_found=true)
       elif test "x$ac_cv_header_sys_ucontext_h" = xyes; then
         AC_TRY_COMPILE([#define _GNU_SOURCE 1
                         #include <sys/ucontext.h>],
                        [ucontext_t u; return u.$pc_field == 0;],
                        AC_DEFINE_UNQUOTED(PC_FROM_UCONTEXT, $pc_field,
                                           How to access the PC from a struct ucontext)
                        AC_MSG_RESULT([$pc_field])
                        pc_field_found=true)
       elif test "x$ac_cv_header_ucontext_h" = xyes; then
         AC_TRY_COMPILE([#define _GNU_SOURCE 1
                         #include <ucontext.h>],
                        [ucontext_t u; return u.$pc_field == 0;],
                        AC_DEFINE_UNQUOTED(PC_FROM_UCONTEXT, $pc_field,
                                           How to access the PC from a struct ucontext)
                        AC_MSG_RESULT([$pc_field])
                        pc_field_found=true)
       else     # hope some standard header gives it to us
         AC_TRY_COMPILE([],
                        [ucontext_t u; return u.$pc_field == 0;],
                        AC_DEFINE_UNQUOTED(PC_FROM_UCONTEXT, $pc_field,
                                           How to access the PC from a struct ucontext)
                        AC_MSG_RESULT([$pc_field])
                        pc_field_found=true)
       fi
     fi
   done
   if ! $pc_field_found; then
     pc_fields="           sc_eip"  # OpenBSD (i386)
     pc_fields="$pc_fields sc_rip"  # OpenBSD (x86_64)
     for pc_field in $pc_fields; do
       if ! $pc_field_found; then
         AC_TRY_COMPILE([#include <signal.h>],
                        [ucontext_t u; return u.$pc_field == 0;],
                        AC_DEFINE_UNQUOTED(PC_FROM_UCONTEXT, $pc_field,
                                           How to access the PC from a struct ucontext)
                        AC_MSG_RESULT([$pc_field])
                        pc_field_found=true)
       fi
     done
   fi
   if ! $pc_field_found; then
     [$1]
   fi])
