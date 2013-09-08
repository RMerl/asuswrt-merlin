## Process this file with automake to produce Makefile.in -*-Makefile-*-

## Copyright (C) 1990-1991, 1993-2011 Free Software Foundation, Inc.

## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

# These are the names of programs that are not installed by default.
# This list is *not* intended for programs like who, nice, chroot, etc.,
# that are built only when certain requisite system features are detected.
# Hence, if you want to install programs from this list anyway, say A and B,
# use --enable-install-program=A,B
no_install__progs = \
  arch hostname su

build_if_possible__progs = \
  chroot	\
  df		\
  hostid	\
  libstdbuf.so	\
  nice		\
  pinky		\
  stdbuf	\
  stty		\
  su		\
  uptime	\
  users		\
  who

AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)

EXTRA_PROGRAMS = \
  $(no_install__progs) \
  $(build_if_possible__progs) \
  [		\
  base64	\
  basename	\
  cat		\
  chcon		\
  chgrp		\
  chmod		\
  chown		\
  cksum		\
  comm		\
  cp		\
  csplit	\
  cut		\
  date		\
  dd		\
  dir		\
  dircolors	\
  dirname	\
  du		\
  echo		\
  env		\
  expand	\
  expr		\
  factor	\
  false		\
  fmt		\
  fold		\
  ginstall	\
  groups	\
  head		\
  id		\
  join		\
  kill		\
  link		\
  ln		\
  logname	\
  ls		\
  md5sum	\
  mkdir		\
  mkfifo	\
  mknod		\
  mktemp	\
  mv		\
  nl		\
  nproc		\
  nohup		\
  od		\
  paste		\
  pathchk	\
  pr		\
  printenv	\
  printf	\
  ptx		\
  pwd		\
  readlink	\
  rm		\
  rmdir		\
  runcon	\
  seq		\
  sha1sum	\
  sha224sum	\
  sha256sum	\
  sha384sum	\
  sha512sum	\
  shred		\
  shuf		\
  sleep		\
  sort		\
  split		\
  stat		\
  sum		\
  sync		\
  tac		\
  tail		\
  tee		\
  test		\
  timeout	\
  touch		\
  tr		\
  true		\
  truncate	\
  tsort		\
  tty		\
  uname		\
  unexpand	\
  uniq		\
  unlink	\
  vdir		\
  wc		\
  whoami	\
  yes

bin_PROGRAMS = $(OPTIONAL_BIN_PROGS)

noinst_PROGRAMS = setuidgid getlimits

pkglibexec_PROGRAMS = $(OPTIONAL_PKGLIB_PROGS)

noinst_HEADERS =	\
  chown-core.h		\
  copy.h		\
  cp-hash.h		\
  dircolors.h		\
  fiemap.h		\
  find-mount-point.h	\
  fs.h			\
  group-list.h		\
  ioblksize.h		\
  ls.h			\
  operand2sig.h		\
  prog-fprintf.h	\
  remove.h		\
  system.h		\
  wheel-size.h		\
  wheel.h		\
  uname.h

EXTRA_DIST = dcgen dircolors.hin tac-pipe.c \
  wheel-gen.pl extract-magic c99-to-c89.diff
BUILT_SOURCES =
CLEANFILES = $(SCRIPTS) su

# Also remove these sometimes-built programs.
# For example, even when excluded, they're built via _sc_check-AUTHORS.
CLEANFILES += $(no_install__progs)

AM_CPPFLAGS = -I$(top_srcdir)/lib

noinst_LIBRARIES = libver.a
nodist_libver_a_SOURCES = version.c version.h

# Tell the linker to omit references to unused shared libraries.
AM_LDFLAGS = $(IGNORE_UNUSED_LIBRARIES_CFLAGS)

# Sometimes, the expansion of $(LIBINTL) includes -lc which may
# include modules defining variables like `optind', so libcoreutils.a
# must precede $(LIBINTL) in order to ensure we use GNU getopt.
# But libcoreutils.a must also follow $(LIBINTL), since libintl uses
# replacement functions defined in libcoreutils.a.
LDADD = libver.a ../lib/libcoreutils.a $(LIBINTL) ../lib/libcoreutils.a

# First, list all programs, to make listing per-program libraries easier.
# See [ below.
arch_LDADD = $(LDADD)
base64_LDADD = $(LDADD)
basename_LDADD = $(LDADD)
cat_LDADD = $(LDADD)
chcon_LDADD = $(LDADD)
chgrp_LDADD = $(LDADD)
chmod_LDADD = $(LDADD)
chown_LDADD = $(LDADD)
chroot_LDADD = $(LDADD)
cksum_LDADD = $(LDADD)
comm_LDADD = $(LDADD)
nproc_LDADD = $(LDADD)
cp_LDADD = $(LDADD)
csplit_LDADD = $(LDADD)
cut_LDADD = $(LDADD)
date_LDADD = $(LDADD)
dd_LDADD = $(LDADD)
df_LDADD = $(LDADD)
# See dir_LDADD below
dircolors_LDADD = $(LDADD)
dirname_LDADD = $(LDADD)
du_LDADD = $(LDADD)
echo_LDADD = $(LDADD)
env_LDADD = $(LDADD)
expand_LDADD = $(LDADD)
expr_LDADD = $(LDADD)
factor_LDADD = $(LDADD)
false_LDADD = $(LDADD)
fmt_LDADD = $(LDADD)
fold_LDADD = $(LDADD)
getlimits_LDADD = $(LDADD)
ginstall_LDADD = $(LDADD)
groups_LDADD = $(LDADD)
head_LDADD = $(LDADD)
hostid_LDADD = $(LDADD)
hostname_LDADD = $(LDADD)
id_LDADD = $(LDADD)
join_LDADD = $(LDADD)
kill_LDADD = $(LDADD)
link_LDADD = $(LDADD)
ln_LDADD = $(LDADD)
logname_LDADD = $(LDADD)
ls_LDADD = $(LDADD)
md5sum_LDADD = $(LDADD)
mkdir_LDADD = $(LDADD)
mkfifo_LDADD = $(LDADD)
mknod_LDADD = $(LDADD)
mktemp_LDADD = $(LDADD)
mv_LDADD = $(LDADD)
nice_LDADD = $(LDADD)
nl_LDADD = $(LDADD)
nohup_LDADD = $(LDADD)
od_LDADD = $(LDADD)
paste_LDADD = $(LDADD)
pathchk_LDADD = $(LDADD)
pinky_LDADD = $(LDADD)
pr_LDADD = $(LDADD)
printenv_LDADD = $(LDADD)
printf_LDADD = $(LDADD)
ptx_LDADD = $(LDADD)
pwd_LDADD = $(LDADD)
readlink_LDADD = $(LDADD)
rm_LDADD = $(LDADD)
rmdir_LDADD = $(LDADD)
runcon_LDADD = $(LDADD)
seq_LDADD = $(LDADD)
setuidgid_LDADD = $(LDADD)
sha1sum_LDADD = $(LDADD)
sha224sum_LDADD = $(LDADD)
sha256sum_LDADD = $(LDADD)
sha384sum_LDADD = $(LDADD)
sha512sum_LDADD = $(LDADD)
shred_LDADD = $(LDADD)
shuf_LDADD = $(LDADD)
sleep_LDADD = $(LDADD)
sort_LDADD = $(LDADD)
split_LDADD = $(LDADD)
stat_LDADD = $(LDADD)
stdbuf_LDADD = $(LDADD)
stty_LDADD = $(LDADD)
su_LDADD = $(LDADD)
sum_LDADD = $(LDADD)
sync_LDADD = $(LDADD)
tac_LDADD = $(LDADD)
tail_LDADD = $(LDADD)
tee_LDADD = $(LDADD)
test_LDADD = $(LDADD)
timeout_LDADD = $(LDADD)
touch_LDADD = $(LDADD)
tr_LDADD = $(LDADD)
true_LDADD = $(LDADD)
truncate_LDADD = $(LDADD)
tsort_LDADD = $(LDADD)
tty_LDADD = $(LDADD)
uname_LDADD = $(LDADD)
unexpand_LDADD = $(LDADD)
uniq_LDADD = $(LDADD)
unlink_LDADD = $(LDADD)
uptime_LDADD = $(LDADD)
users_LDADD = $(LDADD)
# See vdir_LDADD below
wc_LDADD = $(LDADD)
who_LDADD = $(LDADD)
whoami_LDADD = $(LDADD)
yes_LDADD = $(LDADD)

# Synonyms.  Recall that Automake transliterates '[' to '_'.
__LDADD = $(test_LDADD)
dir_LDADD = $(ls_LDADD)
vdir_LDADD = $(ls_LDADD)

# Shared files
copy_LDADD =
cp_LDADD += $(copy_LDADD)
ginstall_LDADD += $(copy_LDADD)
mv_LDADD += $(copy_LDADD)

remove_LDADD =
mv_LDADD += $(remove_LDADD)
rm_LDADD += $(remove_LDADD)

# for eaccess, euidaccess
copy_LDADD += $(LIB_EACCESS)
remove_LDADD += $(LIB_EACCESS)
test_LDADD += $(LIB_EACCESS)

# for selinux use
chcon_LDADD += $(LIB_SELINUX)
copy_LDADD += $(LIB_SELINUX)
ginstall_LDADD += $(LIB_SELINUX)
id_LDADD += $(LIB_SELINUX)
ls_LDADD += $(LIB_SELINUX)
mkdir_LDADD += $(LIB_SELINUX)
mkfifo_LDADD += $(LIB_SELINUX)
mknod_LDADD += $(LIB_SELINUX)
runcon_LDADD += $(LIB_SELINUX)
stat_LDADD += $(LIB_SELINUX)

# for gettime, settime, utimecmp, utimens
copy_LDADD += $(LIB_CLOCK_GETTIME)
date_LDADD += $(LIB_CLOCK_GETTIME)
ginstall_LDADD += $(LIB_CLOCK_GETTIME)
ls_LDADD += $(LIB_CLOCK_GETTIME)
pr_LDADD += $(LIB_CLOCK_GETTIME)
timeout_LDADD += $(LIB_TIMER_TIME)
touch_LDADD += $(LIB_CLOCK_GETTIME)

# for gethrxtime
dd_LDADD += $(LIB_GETHRXTIME)

# for cap_get_file
ls_LDADD += $(LIB_CAP)

# for fdatasync
dd_LDADD += $(LIB_FDATASYNC)
shred_LDADD += $(LIB_FDATASYNC)

# for xnanosleep
sleep_LDADD += $(LIB_NANOSLEEP)
sort_LDADD += $(LIB_NANOSLEEP)
tail_LDADD += $(LIB_NANOSLEEP)

# for various GMP functions
expr_LDADD += $(LIB_GMP)
factor_LDADD += $(LIB_GMP)

# for getloadavg
uptime_LDADD += $(GETLOADAVG_LIBS)

# for crypt
su_LDADD += $(LIB_CRYPT)

# for various ACL functions
copy_LDADD += $(LIB_ACL)
ls_LDADD += $(LIB_ACL)

# for various xattr functions
copy_LDADD += $(LIB_XATTR)

# for print_unicode_char, proper_name_utf8
cat_LDADD += $(LIBICONV)
cp_LDADD += $(LIBICONV)
df_LDADD += $(LIBICONV)
du_LDADD += $(LIBICONV)
getlimits_LDADD += $(LIBICONV)
printf_LDADD += $(LIBICONV)
ptx_LDADD += $(LIBICONV)
split_LDADD += $(LIBICONV)
stdbuf_LDADD += $(LIBICONV)
timeout_LDADD += $(LIBICONV)
truncate_LDADD += $(LIBICONV)

# for canon_host
pinky_LDADD += $(GETADDRINFO_LIB)
who_LDADD += $(GETADDRINFO_LIB)

# for gethostname, uname
hostname_LDADD += $(GETHOSTNAME_LIB)
uname_LDADD += $(GETHOSTNAME_LIB)

# for strsignal
kill_LDADD += $(LIBTHREAD)

# for pthread
sort_LDADD += $(LIB_PTHREAD)

$(PROGRAMS): ../lib/libcoreutils.a

# Get the release year from ../lib/version-etc.c.
RELEASE_YEAR = \
  `sed -n '/.*COPYRIGHT_YEAR = \([0-9][0-9][0-9][0-9]\) };/s//\1/p' \
    $(top_srcdir)/lib/version-etc.c`

all-local: su$(EXEEXT)

installed_su = $(DESTDIR)$(bindir)/`echo su|sed '$(transform)'`

setuid_root_mode = a=rx,u+s

install_su =						\
  if test "$(INSTALL_SU)" = yes; then			\
    p=su;						\
    echo " $(INSTALL_PROGRAM) $$p $(installed_su)";	\
    $(INSTALL_PROGRAM) $$p $(installed_su);		\
    echo " chown root $(installed_su)";			\
    chown root $(installed_su);				\
    echo " chmod $(setuid_root_mode) $(installed_su)";	\
    chmod $(setuid_root_mode) $(installed_su);		\
  else							\
    :;							\
  fi

install-root: su$(EXEEXT)
	@$(install_su)

install-exec-hook: su$(EXEEXT)
	@if test "$(INSTALL_SU)" = yes; then			\
	  TMPFILE=$(DESTDIR)$(bindir)/.su-$$$$;			\
	  rm -f $$TMPFILE;					\
	  echo > $$TMPFILE;					\
## See if we can create a setuid root executable in $(bindir).
## If not, then don't even try to install su.
	  can_create_suid_root_executable=no;			\
	  chown root $$TMPFILE > /dev/null 2>&1			\
	    && chmod $(setuid_root_mode) $$TMPFILE > /dev/null 2>&1 \
	    && can_create_suid_root_executable=yes;		\
	  rm -f $$TMPFILE;					\
	  if test $$can_create_suid_root_executable = yes; then \
	    $(install_su);					\
	  else							\
	    echo "WARNING: insufficient access; not installing su"; \
	    echo "NOTE: to install su, run 'make install-root' as root"; \
	    rm -f $(installed_su);				\
	  fi;							\
	else :;							\
	fi

uninstall-local:
# Remove su only if it's one we installed.
	@if test "$(INSTALL_SU)" = yes; then			\
	  if grep '$(PACKAGE_NAME)' $(installed_su) > /dev/null 2>&1; then \
	    echo "  rm -f $(installed_su)"; \
	    rm -f $(installed_su); \
	  else :; \
	  fi; \
	fi

copy_sources = copy.c cp-hash.c extent-scan.c extent-scan.h

# Use `ginstall' in the definition of PROGRAMS and in dependencies to avoid
# confusion with the `install' target.  The install rule transforms `ginstall'
# to install before applying any user-specified name transformations.

transform = s/ginstall/install/; $(program_transform_name)
ginstall_SOURCES = install.c prog-fprintf.c $(copy_sources)

# This is for the '[' program.  Automake transliterates '[' to '_'.
__SOURCES = lbracket.c

cp_SOURCES = cp.c $(copy_sources)
dir_SOURCES = ls.c ls-dir.c
vdir_SOURCES = ls.c ls-vdir.c
id_SOURCES = id.c group-list.c
groups_SOURCES = groups.c group-list.c
ln_SOURCES = ln.c
ls_SOURCES = ls.c ls-ls.c
chown_SOURCES = chown.c chown-core.c
chgrp_SOURCES = chgrp.c chown-core.c
kill_SOURCES = kill.c operand2sig.c
timeout_SOURCES = timeout.c operand2sig.c

mv_SOURCES = mv.c remove.c $(copy_sources)
rm_SOURCES = rm.c remove.c

mkdir_SOURCES = mkdir.c prog-fprintf.c
rmdir_SOURCES = rmdir.c prog-fprintf.c

df_SOURCES = df.c find-mount-point.c
stat_SOURCES = stat.c find-mount-point.c

uname_SOURCES = uname.c uname-uname.c
arch_SOURCES = uname.c uname-arch.c
nproc_SOURCES = nproc.c

md5sum_SOURCES = md5sum.c
md5sum_CPPFLAGS = -DHASH_ALGO_MD5=1 $(AM_CPPFLAGS)
sha1sum_SOURCES = md5sum.c
sha1sum_CPPFLAGS = -DHASH_ALGO_SHA1=1 $(AM_CPPFLAGS)
sha224sum_SOURCES = md5sum.c
sha224sum_CPPFLAGS = -DHASH_ALGO_SHA224=1 $(AM_CPPFLAGS)
sha256sum_SOURCES = md5sum.c
sha256sum_CPPFLAGS = -DHASH_ALGO_SHA256=1 $(AM_CPPFLAGS)
sha384sum_SOURCES = md5sum.c
sha384sum_CPPFLAGS = -DHASH_ALGO_SHA384=1 $(AM_CPPFLAGS)
sha512sum_SOURCES = md5sum.c
sha512sum_CPPFLAGS = -DHASH_ALGO_SHA512=1 $(AM_CPPFLAGS)

ginstall_CPPFLAGS = -DENABLE_MATCHPATHCON=1 $(AM_CPPFLAGS)

# Ensure we don't link against libcoreutils.a as that lib is
# not compiled with -fPIC which causes issues on 64 bit at least
libstdbuf_so_LDADD =

# Note libstdbuf is only compiled if GCC is available
# (as per the check in configure.ac), so these flags should be available.
# libtool is probably required to relax this dependency.
libstdbuf_so_LDFLAGS = -shared
libstdbuf_so_CFLAGS = -fPIC $(AM_CFLAGS)

editpl = sed -e 's,@''PERL''@,$(PERL),g'

BUILT_SOURCES += dircolors.h
dircolors.h: dcgen dircolors.hin
	$(AM_V_GEN)rm -f $@ $@-t
	$(AM_V_at)$(PERL) -w -- $(srcdir)/dcgen $(srcdir)/dircolors.hin > $@-t
	$(AM_V_at)chmod a-w $@-t
	$(AM_V_at)mv $@-t $@

wheel_size = 5

BUILT_SOURCES += wheel-size.h
wheel-size.h: Makefile.am
	$(AM_V_GEN)rm -f $@ $@-t
	$(AM_V_at)echo '#define WHEEL_SIZE $(wheel_size)' > $@-t
	$(AM_V_at)chmod a-w $@-t
	$(AM_V_at)mv $@-t $@

BUILT_SOURCES += wheel.h
wheel.h: wheel-gen.pl Makefile.am
	$(AM_V_GEN)rm -f $@ $@-t
	$(AM_V_at)$(srcdir)/wheel-gen.pl $(wheel_size) > $@-t
	$(AM_V_at)chmod a-w $@-t
	$(AM_V_at)mv $@-t $@

# false exits nonzero even with --help or --version.
# test doesn't support --help or --version.
# Tell automake to exempt then from that installcheck test.
AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = false test

# Compare fs.h with the list of file system names/magic-numbers in the
# Linux statfs man page.  This target prints any new name/number pairs.
# Also compare against /usr/include/linux/magic.h
.PHONY: fs-magic-compare
fs-magic-compare: fs-magic fs-kernel-magic fs-def
	join -v1 -t@ fs-magic fs-def
	join -v1 -t@ fs-kernel-magic fs-def

CLEANFILES += fs-def
fs-def: fs.h
	grep '^# *define ' $< | $(ASSORT) > $@-t && mv $@-t $@

# Massage bits of the statfs man page and definitions from
# /usr/include/linux/magic.h to be in a form consistent with what's in fs.h.
fs_normalize_perl_subst =			\
  -e 's/MINIX_SUPER_MAGIC\b/MINIX/;'		\
  -e 's/MINIX_SUPER_MAGIC2\b/MINIX_30/;'	\
  -e 's/MINIX2_SUPER_MAGIC\b/MINIX_V2/;'	\
  -e 's/MINIX2_SUPER_MAGIC2\b/MINIX_V2_30/;'	\
  -e 's/MINIX3_SUPER_MAGIC\b/MINIX_V3/;'	\
  -e 's/CIFS_MAGIC_NUMBER/CIFS/;'		\
  -e 's/(_SUPER)?_MAGIC//;'			\
  -e 's/\s+0x(\S+)/" 0x" . uc $$1/e;'		\
  -e 's/(\s+0x)(\X{3})\b/$${1}0$$2/;'		\
  -e 's/(\s+0x)(\X{6})\b/$${1}00$$2/;'		\
  -e 's/(\s+0x)(\X{7})\b/$${1}0$$2/;'		\
  -e 's/^\s+//;'				\
  -e 's/^\#define\s+//;'			\
  -e 's/^_(XIAFS)/$$1/;'			\
  -e 's/^USBDEVICE/USBDEVFS/;'			\
  -e 's/NTFS_SB/NTFS/;'				\
  -e 's/^/\# define S_MAGIC_/;'			\
  -e 's,\s*/\* .*? \*/,,;'

CLEANFILES += fs-magic
fs-magic: Makefile
	man statfs \
	  |perl -ne '/File system types:/.../Nobody kno/ and print'	\
	  |grep 0x | perl -p						\
	    $(fs_normalize_perl_subst)					\
	  | grep -Ev 'S_MAGIC_EXT[34]|STACK_END'			\
	  | LC_ALL=C sort						\
	  > $@-t && mv $@-t $@

CLEANFILES += fs-kernel-magic
fs-kernel-magic: Makefile
	perl -ne '/^#define.*0x/ and print' /usr/include/linux/magic.h	\
	  | perl -p							\
	    $(fs_normalize_perl_subst)					\
	  | grep -Ev 'S_MAGIC_EXT[34]|STACK_END'			\
	  | LC_ALL=C sort						\
	  > $@-t && mv $@-t $@

BUILT_SOURCES += fs.h
fs.h: stat.c extract-magic
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)$(PERL) $(srcdir)/extract-magic $(srcdir)/stat.c > $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

BUILT_SOURCES += version.c
version.c: Makefile
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)printf '#include <config.h>\n' > $@t
	$(AM_V_at)printf 'char const *Version = "$(PACKAGE_VERSION)";\n' >> $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

BUILT_SOURCES += version.h
version.h: Makefile
	$(AM_V_GEN)rm -f $@
	$(AM_V_at)printf 'extern char const *Version;\n' > $@t
	$(AM_V_at)chmod a-w $@t
	$(AM_V_at)mv $@t $@

DISTCLEANFILES = version.c version.h
MAINTAINERCLEANFILES = $(BUILT_SOURCES)

# Sort in traditional ASCII order, regardless of the current locale;
# otherwise we may get into trouble with distinct strings that the
# current locale considers to be equal.
ASSORT = LC_ALL=C sort

all_programs = \
    $(bin_PROGRAMS) \
    $(bin_SCRIPTS) \
  $(EXTRA_PROGRAMS)

built_programs.list:
	@echo $(bin_PROGRAMS) $(bin_SCRIPTS) | tr ' ' '\n' \
	  | sed -e 's,$(EXEEXT)$$,,' | $(ASSORT) -u | tr '\n' ' '

all_programs.list:
	@echo $(all_programs) | tr ' ' '\n' | sed -e 's,$(EXEEXT)$$,,' \
	  | sed /libstdbuf/d \
	  | $(ASSORT) -u

# This is required because we have broken inter-directory dependencies:
# in order to generate all man pages, even those for which we don't
# install a binary, require that all programs be built at distribution time.
dist-hook: $(all_programs)

# Ensure that all programs are built so we can for example
# subsequently syntax check all man pages.
.PHONY: all_programs
all_programs: $(all_programs)

pm = progs-makefile
pr = progs-readme
# Ensure that the list of programs in README matches the list
# of programs we can build.
check: check-README check-duplicate-no-install
.PHONY: check-README
check-README:
	$(AM_V_GEN)rm -rf $(pr) $(pm)
	$(AM_V_at)echo $(all_programs) \
	 | tr -s ' ' '\n' | sed -e 's,$(EXEEXT)$$,,;s/ginstall/install/' \
	 | sed /libstdbuf/d \
	 | $(ASSORT) -u > $(pm) && \
	sed -n '/^The programs .* are:/,/^[a-zA-Z]/p' $(top_srcdir)/README \
	  | sed -n '/^   */s///p' | tr -s ' ' '\n' > $(pr)
	$(AM_V_at)diff $(pm) $(pr) && rm -rf $(pr) $(pm)

# Ensure that a by-default-not-installed program (listed in
# $(no_install__progs) is not also listed in $(EXTRA_PROGRAMS), because
# if that were to happen, it *would* be installed by default.
.PHONY: check-duplicate-no-install
check-duplicate-no-install: tr
	$(AM_V_GEN)test -z "`echo '$(EXTRA_PROGRAMS)'| ./tr ' ' '\n' | uniq -d`"

# Ensure that the list of programs and author names is accurate.
# We need a UTF8 locale.  If a lack of locale support or a missing
# translation inhibits printing of UTF-8 names, just skip this test.
au_dotdot = authors-dotdot
au_actual = authors-actual
.PHONY: _sc_check-AUTHORS
_sc_check-AUTHORS: $(all_programs)
	@locale=en_US.UTF-8;				\
	LC_ALL=$$locale ./cat --version			\
	    | grep ' Torbjorn '	> /dev/null		\
	  && { echo "$@: skipping this check"; exit 0; }; \
	rm -f $(au_actual) $(au_dotdot);		\
	for i in `ls $(all_programs) | sed -e 's,$(EXEEXT)$$,,' \
	    | sed /libstdbuf/d				\
	    | $(ASSORT) -u`; do				\
	  test "$$i" = '[' && continue;			\
	  exe=$$i;					\
	  if test "$$i" = install; then			\
	    exe=ginstall;				\
	  elif test "$$i" = test; then			\
	    exe='[';					\
	  fi;						\
	  LC_ALL=$$locale ./$$exe --version		\
	    | perl -0 -pi -e 's/,\n/, /gm'		\
	    | sed -n -e '/Written by /{ s//'"$$i"': /;'	\
		  -e 's/,* and /, /; s/\.$$//; p; }';	\
	done > $(au_actual) &&				\
	sed -n '/^[^ ][^ ]*:/p' $(top_srcdir)/AUTHORS > $(au_dotdot) && \
	diff $(au_actual) $(au_dotdot) && rm -f $(au_actual) $(au_dotdot)

# Use the just-built ./ginstall, when not cross-compiling.
if CROSS_COMPILING
cu_install_program = @INSTALL_PROGRAM@
else
cu_install_program = ./ginstall
endif
INSTALL_PROGRAM = $(cu_install_program)
