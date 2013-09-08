# Customize maint.mk                           -*- makefile -*-
# Copyright (C) 2003-2011 Free Software Foundation, Inc.

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

# Used in maint.mk's web-manual rule
manual_title = Core GNU utilities

# Tests not to run as part of "make distcheck".
local-checks-to-skip = \
  sc_texinfo_acronym

# Tools used to bootstrap this package, used for "announcement".
bootstrap-tools = autoconf,automake,gnulib,bison

# Now that we have better tests, make this the default.
export VERBOSE = yes

old_NEWS_hash = d491296a7e0e2269b2b96dc4bd5f77a8

# Add an exemption for sc_makefile_at_at_check.
_makefile_at_at_check_exceptions = ' && !/^cu_install_program =/'

# Our help-version script is in a slightly different location.
_hv_file ?= $(srcdir)/tests/misc/help-version

# Ensure that the list of O_ symbols used to compute O_FULLBLOCK is complete.
dd = $(srcdir)/src/dd.c
sc_dd_O_FLAGS:
	@rm -f $@.1 $@.2
	@{ echo O_FULLBLOCK; echo O_NOCACHE;				\
	  perl -nle '/^ +\| (O_\w*)$$/ and print $$1' $(dd); } | sort > $@.1
	@{ echo O_NOFOLLOW; perl -nle '/{"[a-z]+",\s*(O_\w+)},/ and print $$1' \
	  $(dd); } | sort > $@.2
	@diff -u $@.1 $@.2 || diff=1 || diff=;				\
	rm -f $@.1 $@.2;						\
	test "$$diff"							\
	  && { echo '$(ME): $(dd) has inconsistent O_ flag lists'>&2;	\
	       exit 1; } || :

# Ensure that dd's definition of LONGEST_SYMBOL stays in sync
# with the strings from the two affected variables.
dd_c = $(srcdir)/src/dd.c
sc_dd_max_sym_length:
ifneq ($(wildcard $(dd_c)),)
	@len=$$( (sed -n '/conversions\[\] =$$/,/^};/p' $(dd_c);\
		 sed -n '/flags\[\] =$$/,/^};/p' $(dd_c) )	\
		|sed -n '/"/s/^[^"]*"\([^"]*\)".*/\1/p'		\
	      | wc --max-line-length);				\
	max=$$(sed -n '/^#define LONGEST_SYMBOL /s///p' $(dd_c)	\
	      |tr -d '"' | wc --max-line-length);		\
	if test "$$len" = "$$max"; then :; else			\
	  echo 'dd.c: LONGEST_SYMBOL is not longest' 1>&2;	\
	  exit 1;						\
	fi
endif

# Many m4 macros names once began with `jm_'.
# On 2004-04-13, they were all changed to start with gl_ instead.
# Make sure that none are inadvertently reintroduced.
sc_prohibit_jm_in_m4:
	@grep -nE 'jm_[A-Z]'						\
		$$($(VC_LIST) m4 |grep '\.m4$$'; echo /dev/null) &&	\
	    { echo '$(ME): do not use jm_ in m4 macro names'		\
	      1>&2; exit 1; } || :

# Ensure that each root-requiring test is run via the "check-root" rule.
sc_root_tests:
	@if test -d tests \
	      && grep check-root tests/Makefile.am>/dev/null 2>&1; then \
	t1=sc-root.expected; t2=sc-root.actual;				\
	grep -nl '^ *require_root_$$'					\
	  $$($(VC_LIST) tests) |sed s,tests/,, |sort > $$t1;		\
	sed -n '/^root_tests =[	 ]*\\$$/,/[^\]$$/p'			\
	  $(srcdir)/tests/Makefile.am					\
	    | sed 's/^  *//;/^root_tests =/d'				\
	    | tr -s '\012\\' '  ' | fmt -1 | sort > $$t2;		\
	diff -u $$t1 $$t2 || diff=1 || diff=;				\
	rm -f $$t1 $$t2;						\
	test "$$diff"							\
	  && { echo 'tests/Makefile.am: missing check-root action'>&2;	\
	       exit 1; } || :;						\
	fi

# Create a list of regular expressions matching the names
# of files included from system.h.  Exclude a couple.
.re-list:
	@sed -n '/^# *include /s///p' $(srcdir)/src/system.h \
	  | grep -Ev 'sys/(param|file)\.h' \
	  | sed 's/ .*//;;s/^["<]/^# *include [<"]/;s/\.h[">]$$/\\.h[">]/' \
	  > $@-t
	@mv $@-t $@

define gl_trap_
  Exit () { set +e; (exit $$1); exit $$1; };				\
  for sig in 1 2 3 13 15; do						\
    eval "trap 'Exit $$(expr $$sig + 128)' $$sig";			\
  done
endef

# Files in src/ should not include directly any of
# the headers already included via system.h.
sc_system_h_headers: .re-list
	@if test -f $(srcdir)/src/system.h; then			\
	  trap 'rc=$$?; rm -f .re-list; exit $$rc' 0;			\
	  $(gl_trap_);							\
	  grep -nE -f .re-list						\
	      $$($(VC_LIST_EXCEPT) | grep '^\($(srcdir)/\)\?src/')	\
	    && { echo '$(ME): the above are already included via system.h'\
		  1>&2;  exit 1; } || :;				\
	fi

sc_sun_os_names:
	@grep -nEi \
	    'solaris[^[:alnum:]]*2\.(7|8|9|[1-9][0-9])|sunos[^[:alnum:]][6-9]' \
	    $$($(VC_LIST_EXCEPT)) &&					\
	  { echo '$(ME): found misuse of Sun OS version numbers' 1>&2;	\
	    exit 1; } || :

ALL_RECURSIVE_TARGETS += sc_check-AUTHORS
sc_check-AUTHORS:
	@$(MAKE) -s -C src _sc_check-AUTHORS

# Look for lines longer than 80 characters, except omit:
# - program-generated long lines in diff headers,
# - tests involving long checksum lines, and
# - the 'pr' test cases.
LINE_LEN_MAX = 80
FILTER_LONG_LINES =						\
  /^[^:]*\.diff:[^:]*:@@ / d;					\
  \|^[^:]*tests/misc/sha[0-9]*sum[-:]| d;			\
  \|^[^:]*tests/pr/|{ \|^[^:]*tests/pr/pr-tests:| !d; };
sc_long_lines:
	@files=$$($(VC_LIST_EXCEPT))					\
	halt='line(s) with more than $(LINE_LEN_MAX) characters; reindent'; \
	for file in $$files; do						\
	  expand $$file | grep -nE '^.{$(LINE_LEN_MAX)}.' |		\
	  sed -e "s|^|$$file:|" -e '$(FILTER_LONG_LINES)';		\
	done | grep . && { msg="$$halt" $(_sc_say_and_exit) } || :

# Option descriptions should not start with a capital letter
# One could grep source directly as follows:
# grep -E " {2,6}-.*[^.]  [A-Z][a-z]" $$($(VC_LIST_EXCEPT) | grep '\.c$$')
# but that would miss descriptions not on the same line as the -option.
ALL_RECURSIVE_TARGETS += sc_option_desc_uppercase
sc_option_desc_uppercase:
	@$(MAKE) -s -C src all_programs
	@$(MAKE) -s -C man $@

# Ensure all man/*.[1x] files are present
ALL_RECURSIVE_TARGETS += sc_man_file_correlation
sc_man_file_correlation:
	@$(MAKE) -s -C src all_programs
	@$(MAKE) -s -C man $@

# Ensure that the end of each release's section is marked by two empty lines.
sc_NEWS_two_empty_lines:
	@sed -n 4,/Noteworthy/p $(srcdir)/NEWS				\
	    | perl -n0e '/(^|\n)\n\n\* Noteworthy/ or exit 1'		\
	  || { echo '$(ME): use two empty lines to separate NEWS sections' \
		 1>&2; exit 1; } || :

# Perl-based tests used to exec perl from a #!/bin/sh script.
# Now they all start with #!/usr/bin/perl and the portability
# infrastructure is in tests/Makefile.am.  Make sure no old-style
# script sneaks back in.
sc_no_exec_perl_coreutils:
	@if test -f $(srcdir)/tests/Coreutils.pm; then			\
	  grep '^exec  *\$$PERL.*MCoreutils' $$($(VC_LIST) tests) &&	\
	    { echo 1>&2 '$(ME): found anachronistic Perl-based tests';	\
	      exit 1; } || :;						\
	fi

# Don't use "readlink" or "readlinkat" directly
sc_prohibit_readlink:
	@prohibit='\<readlink(at)? \('					\
	halt='do not use readlink(at); use via xreadlink or areadlink*'	\
	  $(_sc_search_regexp)

# Don't use address of "stat" or "lstat" functions
sc_prohibit_stat_macro_address:
	@prohibit='\<l?stat '':|&l?stat\>'				\
	halt='stat() and lstat() may be function-like macros'		\
	  $(_sc_search_regexp)

# Ensure that date's --help output stays in sync with the info
# documentation for GNU strftime.  The only exception is %N,
# which date accepts but GNU strftime does not.
extract_char = sed 's/^[^%][^%]*%\(.\).*/\1/'
sc_strftime_check:
	@if test -f $(srcdir)/src/date.c; then				\
	  grep '^  %.  ' $(srcdir)/src/date.c | sort			\
	    | $(extract_char) > $@-src;					\
	  { echo N;							\
	    info libc date calendar format 2>/dev/null|grep '^    `%.'\'\
	      | $(extract_char); } | sort > $@-info;			\
	  if test $$(stat --format %s $@-info) != 2; then		\
	    diff -u $@-src $@-info || exit 1;				\
	  else								\
	    echo '$(ME): skipping $@: libc info not installed' 1>&2;	\
	  fi;								\
	  rm -f $@-src $@-info;						\
	fi

# Indent only with spaces.
sc_prohibit_tab_based_indentation:
	@prohibit='^ *	'						\
	halt='TAB in indentation; use only spaces'			\
	  $(_sc_search_regexp)

# The SEE ALSO section of a man page should not be terminated with
# a period.  Check the first line after each "SEE ALSO" line in man/*.x:
sc_prohibit_man_see_also_period:
	@grep -nB1 '\.$$' $$($(VC_LIST_EXCEPT) | grep 'man/.*\.x$$')	\
	    | grep -A1 -e '-\[SEE ALSO\]' | grep '\.$$' &&		\
	  { echo '$(ME): do not end "SEE ALSO" section with a period'	\
	      1>&2; exit 1; } || :

# Don't use "indent-tabs-mode: nil" anymore.  No longer needed.
sc_prohibit_emacs__indent_tabs_mode__setting:
	@prohibit='^( *[*#] *)?indent-tabs-mode:'			\
	halt='use of emacs indent-tabs-mode: setting'			\
	  $(_sc_search_regexp)

# Ensure that each file that contains fail=1 also contains fail=0.
# Otherwise, setting file=1 in the environment would make tests fail
# unexpectedly.
sc_prohibit_fail_0:
	@prohibit='\<fail=0\>'						\
	halt='fail=0 initialization'					\
	  $(_sc_search_regexp)

# Ensure that "stdio--.h" is used where appropriate.
sc_require_stdio_safer:
	@if $(VC_LIST_EXCEPT) | grep -l '\.[ch]$$' > /dev/null; then	\
	  files=$$(grep -l '\bfreopen \?(' $$($(VC_LIST_EXCEPT)		\
	      | grep '\.[ch]$$'));					\
	  test -n "$$files" && grep -LE 'include "stdio--.h"' $$files	\
	      | grep . &&						\
	  { echo '$(ME): the above files should use "stdio--.h"'	\
		1>&2; exit 1; } || :;					\
	else :;								\
	fi

sc_prohibit_perl_hash_quotes:
	@prohibit="\{'[A-Z_]+' *[=}]"					\
	halt="in Perl code, write \$$hash{KEY}, not \$$hash{'K''EY'}"	\
	  $(_sc_search_regexp)

# Prefer xnanosleep over other less-precise sleep methods
sc_prohibit_sleep:
	@prohibit='\<(nano|u)?sleep \('					\
	halt='prefer xnanosleep over other sleep interfaces'		\
	  $(_sc_search_regexp)

# Use print_ver_ (from init.cfg), not open-coded $VERBOSE check.
sc_prohibit_verbose_version:
	@prohibit='test "\$$VERBOSE" = yes && .* --version'		\
	halt='use the print_ver_ function instead...'			\
	  $(_sc_search_regexp)

# Use framework_failure_, not the old name without the trailing underscore.
sc_prohibit_framework_failure:
	@prohibit='\<framework_''failure\>'				\
	halt='use framework_failure_ instead'				\
	  $(_sc_search_regexp)

###########################################################
_p0 = \([^"'/]\|"\([^\"]\|[\].\)*"\|'\([^\']\|[\].\)*'
_pre = $(_p0)\|[/][^"'/*]\|[/]"\([^\"]\|[\].\)*"\|[/]'\([^\']\|[\].\)*'\)*
_pre_anchored = ^\($(_pre)\)
_comment_and_close = [^*]\|[*][^/*]\)*[*][*]*/
# help font-lock mode: '

# A sed expression that removes ANSI C and ISO C99 comments.
# Derived from the one in GNU gettext's 'moopp' preprocessor.
_sed_remove_comments =					\
/[/][/*]/{						\
  ta;							\
  :a;							\
  s,$(_pre_anchored)//.*,\1,;				\
  te;							\
  s,$(_pre_anchored)/[*]\($(_comment_and_close),\1 ,;	\
  ta;							\
  /^$(_pre)[/][*]/{					\
    s,$(_pre_anchored)/[*].*,\1 ,;			\
    tu;							\
    :u;							\
    n;							\
    s,^\($(_comment_and_close),,;			\
    tv;							\
    s,^.*$$,,;						\
    bu;							\
    :v;							\
  };							\
  :e;							\
}
# Quote all single quotes.
_sed_rm_comments_q = $(subst ','\'',$(_sed_remove_comments))
# help font-lock mode: '

_space_before_paren_exempt =? \\n\\$$
_space_before_paren_exempt = \
  (^ *\#|\\n\\$$|%s\(to %s|(date|group|character)\(s\))
# Ensure that there is a space before each open parenthesis in C code.
sc_space_before_open_paren:
	@if $(VC_LIST_EXCEPT) | grep -l '\.[ch]$$' > /dev/null; then	\
	  fail=0;							\
	  for c in $$($(VC_LIST_EXCEPT) | grep '\.[ch]$$'); do		\
	    sed '$(_sed_rm_comments_q)' $$c 2>/dev/null			\
	      | grep -i '[[:alnum:]]('					\
	      | grep -vE '$(_space_before_paren_exempt)'		\
	      | grep . && { fail=1; echo "*** $$c"; };			\
	  done;								\
	  test $$fail = 1 &&						\
	    { echo '$(ME): the above files lack a space-before-open-paren' \
		1>&2; exit 1; } || :;					\
	else :;								\
	fi

# Similar to the gnulib maint.mk rule for sc_prohibit_strcmp
# Use STREQ_LEN or STRPREFIX rather than comparing strncmp == 0, or != 0.
sc_prohibit_strncmp:
	@grep -nE '! *str''ncmp *\(|\<str''ncmp *\(.+\) *[!=]='		\
	    $$($(VC_LIST_EXCEPT))					\
	  | grep -vE ':# *define STR(N?EQ_LEN|PREFIX)\(' &&		\
	  { echo '$(ME): use STREQ_LEN or STRPREFIX instead of str''ncmp' \
		1>&2; exit 1; } || :

# Enforce recommended preprocessor indentation style.
sc_preprocessor_indentation:
	@if cppi --version >/dev/null 2>&1; then			\
	  $(VC_LIST_EXCEPT) | grep '\.[ch]$$' | xargs cppi -a -c	\
	    || { echo '$(ME): incorrect preprocessor indentation' 1>&2;	\
		exit 1; };						\
	else								\
	  echo '$(ME): skipping test $@: cppi not installed' 1>&2;	\
	fi

# Override the default Cc: used in generating an announcement.
announcement_Cc_ = $(translation_project_), \
  coreutils@gnu.org, coreutils-announce@gnu.org

-include $(srcdir)/dist-check.mk

update-copyright-env = \
  UPDATE_COPYRIGHT_USE_INTERVALS=1 \
  UPDATE_COPYRIGHT_MAX_LINE_LENGTH=79

# List syntax-check exemptions.
exclude_file_name_regexp--sc_space_tab = \
  ^(tests/pr/|tests/misc/nl$$|gl/.*\.diff$$)
exclude_file_name_regexp--sc_bindtextdomain = ^(gl/.*|lib/euidaccess-stat)\.c$$
exclude_file_name_regexp--sc_unmarked_diagnostics =    ^build-aux/cvsu$$
exclude_file_name_regexp--sc_error_message_uppercase = ^build-aux/cvsu$$
exclude_file_name_regexp--sc_trailing_blank = ^tests/pr/
exclude_file_name_regexp--sc_system_h_headers = \
  ^src/((system|copy)\.h|libstdbuf\.c)$$

_src = (false|lbracket|ls-(dir|ls|vdir)|tac-pipe|uname-(arch|uname))
exclude_file_name_regexp--sc_require_config_h_first = \
  (^lib/buffer-lcm\.c|src/$(_src)\.c)$$
exclude_file_name_regexp--sc_require_config_h = \
  $(exclude_file_name_regexp--sc_require_config_h_first)

exclude_file_name_regexp--sc_po_check = ^gl/
exclude_file_name_regexp--sc_prohibit_always-defined_macros = ^src/seq\.c$$
exclude_file_name_regexp--sc_prohibit_empty_lines_at_EOF = ^tests/pr/
exclude_file_name_regexp--sc_program_name = ^(gl/.*|lib/euidaccess-stat)\.c$$
exclude_file_name_regexp--sc_file_system = \
  NEWS|^(tests/init\.cfg|src/df\.c|tests/misc/df-P)$$
exclude_file_name_regexp--sc_prohibit_always_true_header_tests = \
  ^m4/stat-prog\.m4$$
exclude_file_name_regexp--sc_prohibit_fail_0 = \
  (^tests/init\.sh|Makefile\.am|\.mk)$$
exclude_file_name_regexp--sc_prohibit_atoi_atof = ^lib/euidaccess-stat\.c$$
exclude_file_name_regexp--sc_prohibit_tab_based_indentation = \
  ^tests/pr/|(^gl/lib/reg.*\.c\.diff|Makefile(\.am)?|\.mk|^man/help2man)$$
exclude_file_name_regexp--sc_preprocessor_indentation = \
  ^(gl/lib/rand-isaac\.[ch]|gl/tests/test-rand-isaac\.c)$$


exclude_file_name_regexp--sc_prohibit_stat_st_blocks = \
  ^(src/system\.h|tests/du/2g)$$
