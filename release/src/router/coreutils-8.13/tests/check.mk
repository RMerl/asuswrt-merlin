# Include this file at the end of each tests/*/Makefile.am.
# Copyright (C) 2007-2011 Free Software Foundation, Inc.

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

# Ensure that all version-controlled executable files are listed in TESTS.
# Collect test names from the line matching /^TESTS = \\$$/ to the following
# one that does not end in '\'.
_v = TESTS
_w = root_tests
vc_exe_in_TESTS: Makefile
	$(AM_V_GEN)rm -f t1 t2;						\
	if test -d $(top_srcdir)/.git && test $(srcdir) = .; then	\
	  { sed -n '/^$(_v) =[	 ]*\\$$/,/[^\]$$/p'			\
		$(srcdir)/Makefile.am					\
	    | sed 's/^  *//;/^\$$.*/d;/^$(_v) =/d';			\
	    sed -n '/^$(_w) =[	 ]*\\$$/,/[^\]$$/p'			\
		$(srcdir)/Makefile.am					\
	    | sed 's/^  *//;/^\$$.*/d;/^$(_w) =/d'; }			\
	    | tr -s '\012\\' '  ' | fmt -1 | sort -u > t1 &&		\
	  for f in `cd $(top_srcdir) && build-aux/vc-list-files $(subdir)`; do \
	    f=`echo $$f|sed 's!^$(subdir)/!!'`;				\
	    test -f "$$f" && test -x "$$f" && echo "$$f";		\
	  done | sort -u > t2 &&					\
	  diff -u t1 t2 || exit 1;					\
	  rm -f t1 t2;							\
	else :; fi

check: vc_exe_in_TESTS
.PHONY: vc_exe_in_TESTS

CLEANFILES =
CLEANFILES += .built-programs
check-am: .built-programs
.built-programs:
	$(AM_V_GEN)(cd $(top_builddir)/src				\
            && MAKEFLAGS= $(MAKE) -s built_programs.list)		\
          > $@-t && mv $@-t $@

## `$f' is set by the Automake-generated test harness to the path of the
## current test script stripped of VPATH components, and is used by the
## shell-or-perl script to determine the name of the temporary files to be
## used.  Note that $f is a shell variable, not a make macro, so the use of
## `$$f' below is correct, and not a typo.
LOG_COMPILER = \
  $(SHELL) $(srcdir)/shell-or-perl \
  --test-name "$$f" --srcdir '$(srcdir)' \
  --shell '$(SHELL)' --perl '$(PERL)' --

# Note that the first lines are statements.  They ensure that environment
# variables that can perturb tests are unset or set to expected values.
# The rest are envvar settings that propagate build-related Makefile
# variables to test scripts.
TESTS_ENVIRONMENT =				\
  . $(srcdir)/lang-default;			\
  tmp__=$${TMPDIR-/tmp};			\
  test -d "$$tmp__" && test -w "$$tmp__" || tmp__=.;	\
  . $(srcdir)/envvar-check;			\
  TMPDIR=$$tmp__; export TMPDIR;		\
  export					\
  VERSION='$(VERSION)'				\
  LOCALE_FR='$(LOCALE_FR)'			\
  LOCALE_FR_UTF8='$(LOCALE_FR_UTF8)'		\
  abs_top_builddir='$(abs_top_builddir)'	\
  abs_top_srcdir='$(abs_top_srcdir)'		\
  abs_srcdir='$(abs_srcdir)'			\
  built_programs="`cat .built-programs`"	\
  host_os=$(host_os)				\
  host_triplet='$(host_triplet)'		\
  srcdir='$(srcdir)'				\
  top_srcdir='$(top_srcdir)'			\
  CONFIG_HEADER='$(abs_top_builddir)/$(CONFIG_INCLUDE)' \
  CU_TEST_NAME=`basename '$(abs_srcdir)'`,`echo $$tst|sed 's,^\./,,;s,/,-,g'` \
  CC='$(CC)'					\
  AWK='$(AWK)'					\
  EGREP='$(EGREP)'				\
  EXEEXT='$(EXEEXT)'				\
  MAKE=$(MAKE)					\
  PACKAGE_BUGREPORT='$(PACKAGE_BUGREPORT)'	\
  PACKAGE_VERSION=$(PACKAGE_VERSION)		\
  PERL='$(PERL)'				\
  PREFERABLY_POSIX_SHELL='$(PREFERABLY_POSIX_SHELL)' \
  REPLACE_GETCWD=$(REPLACE_GETCWD)		\
  ; test -d /usr/xpg4/bin && PATH='/usr/xpg4/bin$(PATH_SEPARATOR)'"$$PATH"; \
  PATH='$(abs_top_builddir)/src$(PATH_SEPARATOR)'"$$PATH" \
  ; 9>&2

VERBOSE = yes
