#serial 11

dnl Misc lib-related macros for coreutils.

# Copyright (C) 1993-1997, 2000-2001, 2003-2006, 2008-2011 Free Software
# Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Written by Jim Meyering.

AC_DEFUN([cu_LIB_CHECK],
[

  # Check for libypsec.a on Dolphin M88K machines.
  AC_CHECK_LIB([ypsec], [main])

  # m88k running dgux 5.4 needs this
  AC_CHECK_LIB([ldgc], [main])

  # The -lsun library is required for YP support on Irix-4.0.5 systems.
  # m88k/svr3 DolphinOS systems using YP need -lypsec for id.
  AC_SEARCH_LIBS([yp_match], [sun ypsec])

  # SysV needs -lsec, older versions of Linux need -lshadow for
  # shadow passwords.  UnixWare 7 needs -lgen.
  AC_SEARCH_LIBS([getspnam], [shadow sec gen])

  AC_CHECK_HEADERS([shadow.h])

  # Requirements for su.c.
  shadow_includes="\
$ac_includes_default
#if HAVE_SHADOW_H
# include <shadow.h>
#endif
"
  AC_CHECK_MEMBERS([struct spwd.sp_pwdp],,,[$shadow_includes])
  AC_CHECK_FUNCS([getspnam])

  # SCO-ODT-3.0 is reported to need -lufc for crypt.
  # NetBSD needs -lcrypt for crypt.
  LIB_CRYPT=
  cu_saved_libs="$LIBS"
  AC_SEARCH_LIBS([crypt], [ufc crypt],
                 [test "$ac_cv_search_crypt" = "none required" ||
                  LIB_CRYPT="$ac_cv_search_crypt"])
  LIBS="$cu_saved_libs"
  AC_SUBST([LIB_CRYPT])
])
