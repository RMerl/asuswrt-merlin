# Makefile.am for libgpg-error.
# Copyright (C) 2003, 2004 g10 Code GmbH
# 
# This file is part of libgpg-error.
# 
# libgpg-error is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2.1 of the
# License, or (at your option) any later version.
# 
# libgpg-error is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

# We distribute the generated sources err-sources.h and err-codes.h,
# because they are needed to build the po directory, and they don't
# depend on the configuration anyway. 


if HAVE_W32CE_SYSTEM
gpg_extra_headers = gpg-extra/errno.h
extra_cppflags = -idirafter gpg-extra
else
gpg_extra_headers = 
extra_cppflags =  
endif

localedir = $(datadir)/locale
bin_PROGRAMS = gpg-error
lib_LTLIBRARIES = libgpg-error.la
include_HEADERS = gpg-error.h
bin_SCRIPTS = gpg-error-config
m4datadir = $(datadir)/aclocal
m4data_DATA = gpg-error.m4

EXTRA_DIST = mkstrtable.awk err-sources.h.in err-codes.h.in \
	mkerrnos.awk errnos.in README \
	mkerrcodes.awk mkerrcodes1.awk mkerrcodes2.awk mkerrcodes.c \
	mkheader.awk gpg-error.h.in mkw32errmap.c w32-add.h w32ce-add.h \
	err-sources.h err-codes.h gpg-error-config.in gpg-error.m4 \
	gpg-error.def.in versioninfo.rc.in

BUILT_SOURCES = err-sources.h err-codes.h code-to-errno.h code-from-errno.h \
	err-sources-sym.h err-codes-sym.h errnos-sym.h gpg-error.h \
	gpg-error.def extra-h.in mkw32errmap.map.c 

tmp_files = _mkerrcodes.h _gpg-error.def.h mkw32errmap.tab.h mkw32errmap.map.c 

CLEANFILES = err-sources.h err-codes.h code-to-errno.h code-from-errno.h \
	gpg-error.h mkerrcodes mkerrcodes.h gpg-error.def mkw32errmap.tab.h \
	mkw32errmap.map.c err-sources-sym.h err-codes-sym.h errnos-sym.h \
	gpg-extra/errno.h extra-h.in $(tmp_files)

if HAVE_W32_SYSTEM
arch_sources = w32-gettext.c

RCCOMPILE = $(RC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
            -DLOCALEDIR=\"$(localedir)\" $(AM_CPPFLAGS) $(CPPFLAGS)
LTRCCOMPILE = $(LIBTOOL) --mode=compile --tag=RC $(RCCOMPILE)

SUFFIXES = .rc .lo

.rc.lo:
	$(LTRCCOMPILE) -i "$<" -o "$@"

gpg_error_res = versioninfo.lo
no_undefined = -no-undefined
export_symbols = -export-symbols gpg-error.def

install-def-file: gpg-error.def
	$(INSTALL) gpg-error.def $(DESTDIR)$(libdir)/gpg-error.def

uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gpg-error.def

libgpg_error_la_DEPENDENCIES = $(gpg_error_res) gpg-error.def
intllibs = 

else
arch_sources =
gpg_error_res =
no_undefined =
export_symbols =
install-def-file:
uninstall-def-file:

intllibs = @LTLIBINTL@

endif


libgpg_error_la_LDFLAGS = -version-info \
  @LIBGPG_ERROR_LT_CURRENT@:@LIBGPG_ERROR_LT_REVISION@:@LIBGPG_ERROR_LT_AGE@ \
  $(no_undefined) $(export_symbols)

libgpg_error_la_SOURCES = gpg-error.h gettext.h $(arch_sources) \
	init.c init.h strsource.c strerror.c code-to-errno.c code-from-errno.c

# Note that RCCOMPILE needs the same defines as ..._la_CPPFLAGS but
# without the extra_cppflags because they may include am -idirafter
# which is not supported by the RC compiler.
libgpg_error_la_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" $(extra_cppflags)
libgpg_error_la_LIBADD = $(gpg_error_res) $(intllibs)

gpg_error_SOURCES = strsource-sym.c strerror-sym.c gpg-error.c 
gpg_error_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" $(extra_cppflags)
gpg_error_LDADD = ./libgpg-error.la @LTLIBINTL@

# We build err-sources.h and err-codes.h in the source directory.
# This is needed because gettext does only look into the source
# directory to find the files listed in po/POTFILE.in.  To make these
# rules work we also need to depend on Makefile.am and not on the
# generated files Makefile.in or Makefile.
$(srcdir)/err-sources.h: Makefile.am mkstrtable.awk err-sources.h.in
	$(AWK) -f $(srcdir)/mkstrtable.awk -v textidx=3 \
		$(srcdir)/err-sources.h.in >$@

err-sources-sym.h: Makefile mkstrtable.awk err-sources.h.in
	$(AWK) -f $(srcdir)/mkstrtable.awk -v textidx=2 -v nogettext=1 \
		$(srcdir)/err-sources.h.in >$@

$(srcdir)/err-codes.h: Makefile.am mkstrtable.awk err-codes.h.in
	$(AWK) -f $(srcdir)/mkstrtable.awk -v textidx=3 \
		$(srcdir)/err-codes.h.in >$@

err-codes-sym.h: Makefile mkstrtable.awk err-codes.h.in
	$(AWK) -f $(srcdir)/mkstrtable.awk -v textidx=2 -v nogettext=1 \
		$(srcdir)/err-codes.h.in >$@

code-to-errno.h: Makefile mkerrnos.awk errnos.in
	$(AWK) -f $(srcdir)/mkerrnos.awk $(srcdir)/errnos.in >$@

# It is correct to use $(CPP).  We want the host's idea of the error codes.
mkerrcodes.h: Makefile mkerrcodes.awk $(gpg_extra_headers)
	$(AWK) -f $(srcdir)/mkerrcodes1.awk $(srcdir)/errnos.in >_$@
	$(CPP) $(extra_cppflags) _$@ | grep GPG_ERR_ | \
               $(AWK) -f $(srcdir)/mkerrcodes.awk >$@
	-rm _$@

if HAVE_W32CE_SYSTEM
# It is correct to use $(CPP).  We want the host's idea of the error codes.
mkw32errmap.tab.h: Makefile mkw32errmap.c
	$(CPP) -DRESOLVE_MACROS $(srcdir)/mkw32errmap.c | \
	      grep '{&mkw32errmap_marker' >$@
mkw32errmap.map.c: mkw32errmap
	./mkw32errmap --map > $@
gpg-extra/errno.h: mkw32errmap
	-$(MKDIR_P) gpg-extra
	./mkw32errmap > $@
else
mkw32errmap.map.c:
	echo "/*dummy*/" > $@
endif

# We use CC proper for preprocessing thus we have to convince it that
# the data is really to be preprocessed.
gpg-error.def: Makefile gpg-error.def.in
	cat $(srcdir)/gpg-error.def.in >_$@.h
	$(CPP) $(DEFAULT_INCLUDES) $(INCLUDES) $(extra_cppflags) _$@.h | \
	  grep -v '^#' >$@
	-rm _$@.h

# It is correct to use $(CC_FOR_BUILD) here.  We want to run the
# program at build time.
mkerrcodes: mkerrcodes.c mkerrcodes.h Makefile
	$(CC_FOR_BUILD) -I. -I$(srcdir) -o $@ $(srcdir)/mkerrcodes.c

if HAVE_W32CE_SYSTEM
# It is correct to use $(CC_FOR_BUILD) here.  We want to run the
# program at build time.
mkw32errmap: mkw32errmap.c mkw32errmap.tab.h Makefile
	$(CC_FOR_BUILD) -I. -I$(srcdir) -o $@ $(srcdir)/mkw32errmap.c
endif

code-from-errno.h: mkerrcodes Makefile
	./mkerrcodes | $(AWK) -f $(srcdir)/mkerrcodes2.awk >$@

errnos-sym.h: Makefile mkstrtable.awk errnos.in
	$(AWK) -f $(srcdir)/mkstrtable.awk -v textidx=2 -v nogettext=1 \
		-v prefix=GPG_ERR_ -v namespace=errnos_ \
		$(srcdir)/errnos.in >$@

extra-h.in: Makefile w32-add.h w32ce-add.h
	-rm extra-h.in
if HAVE_W32_SYSTEM
	cat $(srcdir)/w32-add.h >>extra-h.in
endif	
if HAVE_W32CE_SYSTEM
	cat $(srcdir)/w32ce-add.h >>extra-h.in
endif	
	echo EOF >>extra-h.in

gpg-error.h: Makefile mkheader.awk err-sources.h.in err-codes.h.in \
	    	 errnos.in extra-h.in gpg-error.h.in
	$(AWK) -f $(srcdir)/mkheader.awk \
		$(srcdir)/err-sources.h.in \
		$(srcdir)/err-codes.h.in \
		$(srcdir)/errnos.in \
		extra-h.in \
		$(srcdir)/gpg-error.h.in > $@


install-data-local:
if HAVE_W32CE_SYSTEM
	-$(MKDIR_P) "$(DESTDIR)$(includedir)/gpg-extra"
	$(INSTALL_DATA) gpg-extra/errno.h \
                         "$(DESTDIR)$(includedir)/gpg-extra/errno.h"
else
        :
endif
