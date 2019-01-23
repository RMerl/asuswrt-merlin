# This file is part of MiniDLNA.
#
# MiniDLNA is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# MiniDLNA is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with MiniDLNA; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

AM_CFLAGS = -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 @STATIC_CFLAGS@

SUBDIRS=po

sbin_PROGRAMS = minidlnad
check_PROGRAMS = testupnpdescgen
minidlnad_SOURCES = minidlna.c upnphttp.c upnpdescgen.c upnpsoap.c \
			upnpreplyparse.c minixml.c clients.c \
			getifaddr.c process.c upnpglobalvars.c \
			options.c minissdp.c uuid.c upnpevents.c \
			sql.c utils.c metadata.c scanner.c monitor.c \
			tivo_utils.c tivo_beacon.c tivo_commands.c \
			playlist.c image_utils.c albumart.c log.c \
			containers.c avahi.c tagutils/tagutils.c

if HAVE_VORBISFILE
vorbislibs = -lvorbis -logg
else
if NEED_OGG
flacogglibs = -logg
endif
endif

if TIVO_SUPPORT
if HAVE_AVAHI
avahilibs = -lavahi-client -lavahi-common
endif
endif

minidlnad_LDADD = \
	@LIBJPEG_LIBS@ \
	@LIBID3TAG_LIBS@ \
	@LIBSQLITE3_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBAVUTIL_LIBS@ \
	@LIBEXIF_LIBS@ \
	@LIBINTL@ \
	@LIBICONV@ \
	-lFLAC $(flacogglibs) $(vorbislibs) $(avahilibs)

minidlnad_LDFLAGS = @STATIC_LDFLAGS@

testupnpdescgen_SOURCES = testupnpdescgen.c upnpdescgen.c
testupnpdescgen_LDADD = \
	@LIBJPEG_LIBS@ \
	@LIBID3TAG_LIBS@ \
	@LIBSQLITE3_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBAVUTIL_LIBS@ \
	@LIBEXIF_LIBS@ \
	-lFLAC $(flacogglibs) $(vorbislibs) $(avahilibs)

SUFFIXES = .tmpl .

.tmpl:
	sed -e s@:SBINDIR:@${sbindir}@ <$< >$@

GENERATED_FILES = \
	linux/minidlna.init.d.script

TEMPLATES = \
	linux/minidlna.init.d.script.tmpl

CLEANFILES = $(GENERATED_FILES)

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4/ChangeLog $(TEMPLATES)
noinst_DATA = $(GENERATED_FILES)
