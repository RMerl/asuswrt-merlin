# Makefile.am - for gcrypt/src
# Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
#               2006, 2007  Free Software Foundation, Inc.
#
# This file is part of Libgcrypt.
#
# Libgcrypt is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# Libgcrypt is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.


## Process this file with automake to produce Makefile.in

EXTRA_DIST = Manifest libgcrypt-config.in libgcrypt.m4 libgcrypt.vers \
             gcrypt.h.in libgcrypt.def

bin_SCRIPTS = libgcrypt-config
m4datadir = $(datadir)/aclocal
m4data_DATA = libgcrypt.m4
include_HEADERS = gcrypt.h gcrypt-module.h

lib_LTLIBRARIES = libgcrypt.la
bin_PROGRAMS = dumpsexp hmac256
if USE_RANDOM_DAEMON
sbin_PROGRAMS = gcryptrnd
bin_PROGRAMS += getrandom
endif USE_RANDOM_DAEMON

# Depending on the architecture some targets require libgpg-error.
if HAVE_W32CE_SYSTEM
arch_gpg_error_cflags = $(GPG_ERROR_CFLAGS)
arch_gpg_error_libs   = $(GPG_ERROR_LIBS)
else
arch_gpg_error_cflags =
arch_gpg_error_libs   =
endif


if HAVE_LD_VERSION_SCRIPT
  libgcrypt_version_script_cmd = -Wl,--version-script=$(srcdir)/libgcrypt.vers
else
  libgcrypt_version_script_cmd =
endif

libgcrypt_la_CFLAGS = $(GPG_ERROR_CFLAGS)
libgcrypt_la_SOURCES = g10lib.h visibility.c visibility.h types.h \
	cipher.h cipher-proto.h \
	misc.c global.c sexp.c hwfeatures.c \
	stdmem.c stdmem.h secmem.c secmem.h \
	mpi.h missing-string.c module.c fips.c \
	hmac256.c hmac256.h \
	ath.h ath.c

if HAVE_W32_SYSTEM

RCCOMPILE = $(RC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
	$(libgcrypt_la_CPPFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS)
LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RCCOMPILE)

SUFFIXES = .rc .lo

.rc.lo:
	$(LTRCCOMPILE) -i "$<" -o "$@"

gcrypt_res = versioninfo.lo
no_undefined = -no-undefined
export_symbols = -export-symbols $(srcdir)/libgcrypt.def

install-def-file:
	$(INSTALL) $(srcdir)/libgcrypt.def $(DESTDIR)$(libdir)/libgcrypt.def

uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/libgcrypt.def

gcrypt_deps = $(gcrypt_res) libgcrypt.def

else !HAVE_W32_SYSTEM

gcrypt_res =
gcrypt_res_ldflag =
no_undefined =
export_symbols =
install-def-file:
uninstall-def-file:

gcrypt_deps =

endif !HAVE_W32_SYSTEM


libgcrypt_la_LDFLAGS = $(no_undefined) $(export_symbols) \
	$(libgcrypt_version_script_cmd) -version-info \
	@LIBGCRYPT_LT_CURRENT@:@LIBGCRYPT_LT_REVISION@:@LIBGCRYPT_LT_AGE@
libgcrypt_la_DEPENDENCIES = \
	../cipher/libcipher.la \
	../random/librandom.la \
	../mpi/libmpi.la \
	../compat/libcompat.la \
	$(srcdir)/libgcrypt.vers $(gcrypt_deps)
libgcrypt_la_LIBADD = $(gcrypt_res) \
	../cipher/libcipher.la \
	../random/librandom.la \
	../mpi/libmpi.la \
	../compat/libcompat.la  $(GPG_ERROR_LIBS)


dumpsexp_SOURCES = dumpsexp.c
dumpsexp_CFLAGS = $(arch_gpg_error_cflags)
dumpsexp_LDADD = $(arch_gpg_error_libs)

hmac256_SOURCES = hmac256.c
hmac256_CFLAGS = -DSTANDALONE $(arch_gpg_error_cflags)
hmac256_LDADD = $(arch_gpg_error_libs)

if USE_RANDOM_DAEMON
gcryptrnd_SOURCES = gcryptrnd.c
gcryptrnd_CFLAGS = $(GPG_ERROR_CFLAGS) $(PTH_CFLAGS)
gcryptrnd_LDADD = libgcrypt.la $(GPG_ERROR_LIBS) $(PTH_LIBS)

getrandom_SOURCES = getrandom.c
endif USE_RANDOM_DAEMON


install-data-local: install-def-file

uninstall-local: uninstall-def-file

# FIXME: We need to figure out how to get the actual name (parsing
# libgcrypt.la?) and how to create the hmac file already at link time
# so that it can be used without installing libgcrypt first.
#install-exec-hook:
#	./hmac256 "What am I, a doctor or a moonshuttle conductor?" \
#          < $(DESTDIR)$(libdir)/libgcrypt.so.11.5.0 \
#          > $(DESTDIR)$(libdir)/.libgcrypt.so.11.5.0.hmac
