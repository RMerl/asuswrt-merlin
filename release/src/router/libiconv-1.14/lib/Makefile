# Makefile for libiconv/lib

#### Start of system configuration section. ####

# Directories used by "make":
srcdir = .

# Directories used by "make install":
prefix = /home/mars/asus-git/asuswrt/release/src-rt/router/libiconv-1.14/out
local_prefix = /usr/local
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

# Programs used by "make":
CC = mipsel-uclibc-gcc
CFLAGS = -DBCMWPA2 -DLINUX26 -DCONFIG_BCMWL5 -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -DBCMWPA2 -funit-at-a-time -Wno-pointer-sign -mtune=mips32 -mips32 -DLINUX_KERNEL_VERSION=132630 -D__CONFIG_WPS__ -fvisibility=hidden
CPPFLAGS = -I/home/mars/asus-git/asuswrt/release/src-rt/router/libiconv-1.14/out/include
LDFLAGS =  $(LDFLAGS_no)
LDFLAGS_yes = -Wl,--export-all-symbols
LDFLAGS_no =
INCLUDES = -I. -I$(srcdir) -I../include -I$(srcdir)/../include -I.. -I$(srcdir)/..
# -DBUILDING_LIBICONV: Change expansion of LIBICONV_DLL_EXPORTED macro.
# -DBUILDING_DLL: Change expansion of RELOCATABLE_DLL_EXPORTED macro.
DEFS = -DLIBDIR=\"$(libdir)\" -DBUILDING_LIBICONV -DBUILDING_DLL \
-DENABLE_RELOCATABLE=1 -DIN_LIBRARY -DINSTALLDIR=\"$(libdir)\" -DNO_XMALLOC \
-Dset_relocation_prefix=libiconv_set_relocation_prefix \
-Drelocate=libiconv_relocate -DHAVE_CONFIG_H
LIBTOOL = /bin/sh $(top_builddir)/libtool
LIBTOOL_COMPILE = $(LIBTOOL) --mode=compile
LIBTOOL_LINK = $(LIBTOOL) --mode=link
LIBTOOL_INSTALL = $(LIBTOOL) --mode=install
LIBTOOL_UNINSTALL = $(LIBTOOL) --mode=uninstall
# Windows resource compiler (windres). Used via libtool.
RC = 
CP = cp
MV = mv
LN = ln
LN_S = ln -s
RM = rm -f

# Programs used by "make install":
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
mkinstalldirs = $(SHELL) ../build-aux/mkinstalldirs

#### End of system configuration section. ####

SHELL = /bin/sh

# Before making a release, change this according to the libtool documentation,
# section "Library interface versions".
LIBICONV_VERSION_INFO = 7:1:5

PACKAGE_VERSION = 1.14

# Needed by $(LIBTOOL).
top_builddir = ..

SOURCES = $(srcdir)/iconv.c $(srcdir)/../libcharset/lib/localcharset.c $(srcdir)/relocatable.c

OBJECTS = iconv.lo localcharset.lo relocatable.lo $(OBJECTS_EXP_no) $(OBJECTS_RES_no)
OBJECTS_EXP_yes = iconv-exports.lo
OBJECTS_EXP_no =
OBJECTS_RES_yes = libiconv.res.lo
OBJECTS_RES_no =

all : libiconv.la

libiconv.la : $(OBJECTS)
	$(LIBTOOL_LINK) $(CC) $(LDFLAGS) $(CFLAGS) -o libiconv.la -rpath $(libdir) -version-info $(LIBICONV_VERSION_INFO) -no-undefined $(OBJECTS)

iconv.lo : $(srcdir)/iconv.c $(srcdir)/converters.h \
           $(srcdir)/encodings.def $(srcdir)/encodings_aix.def $(srcdir)/encodings_osf1.def $(srcdir)/encodings_dos.def $(srcdir)/encodings_local.def \
           $(srcdir)/aliases.h $(srcdir)/aliases_sysaix.h $(srcdir)/aliases_syshpux.h $(srcdir)/aliases_sysosf1.h $(srcdir)/aliases_syssolaris.h \
           $(srcdir)/aliases_aix.h $(srcdir)/aliases_aix_sysaix.h \
           $(srcdir)/aliases_osf1.h $(srcdir)/aliases_osf1_sysosf1.h \
           $(srcdir)/aliases_dos.h \
           $(srcdir)/aliases_extra.h \
           $(srcdir)/flags.h $(srcdir)/translit.h
	$(LIBTOOL_COMPILE) $(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEFS) -c $(srcdir)/iconv.c

localcharset.lo : $(srcdir)/../libcharset/lib/localcharset.c $(srcdir)/relocatable.h
	$(LIBTOOL_COMPILE) $(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEFS) -c $(srcdir)/../libcharset/lib/localcharset.c

relocatable.lo : $(srcdir)/relocatable.c $(srcdir)/relocatable.h
	$(LIBTOOL_COMPILE) $(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEFS) -c $(srcdir)/relocatable.c

iconv-exports.lo : $(srcdir)/../woe32dll/iconv-exports.c
	$(LIBTOOL_COMPILE) $(CC) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(DEFS) -c $(srcdir)/../woe32dll/iconv-exports.c

libiconv.res.lo : $(srcdir)/../windows/libiconv.rc
	$(LIBTOOL_COMPILE) --tag=RC $(RC) `$(SHELL) $(srcdir)/../windows/windres-options --escape $(PACKAGE_VERSION)` -i $(srcdir)/../windows/libiconv.rc -o libiconv.res.lo --output-format=coff

# Installs the library and include files only. Typically called with only
# $(libdir) and $(includedir) - don't use $(prefix) and $(exec_prefix) here.
install-lib : all force
	if [ ! -d $(libdir) ] ; then $(mkinstalldirs) $(libdir) ; fi
	$(LIBTOOL_INSTALL) $(INSTALL_DATA) libiconv.la $(libdir)/libiconv.la

# On AIX, libiconv.a must include the object files of /lib/libiconv.a,
# otherwise the setlocale() call fails when invoked from executables linked
# with -rpath $(libdir), even if linked without -liconv.
install : all force
	if [ ! -d $(DESTDIR)$(libdir) ] ; then $(mkinstalldirs) $(DESTDIR)$(libdir) ; fi
	$(LIBTOOL_INSTALL) $(INSTALL_DATA) libiconv.la $(DESTDIR)$(libdir)/libiconv.la
	case "linux-gnu" in \
	  aix*) (cd $(DESTDIR)$(libdir) && \
	         objects=`ar t libiconv.a`" "`ar t /lib/libiconv.a` && \
	         ar x libiconv.a && ar x /lib/libiconv.a && \
	         ar q libiconv.new.a $$objects && \
	         rm -f $$objects && \
	         mv -f libiconv.new.a libiconv.a) ;; \
	esac

install-strip : install

installdirs : force
	if [ ! -d $(DESTDIR)$(libdir) ] ; then $(mkinstalldirs) $(DESTDIR)$(libdir) ; fi

uninstall : force
	$(LIBTOOL_UNINSTALL) $(RM) $(DESTDIR)$(libdir)/libiconv.la

check : all

mostlyclean : clean

clean : force
	$(RM) *.o *.lo *.a *.la core *.stackdump so_locations
	$(RM) -r .libs _libs

distclean : clean
	$(RM) Makefile config.h libtool

maintainer-clean : distclean

force :

