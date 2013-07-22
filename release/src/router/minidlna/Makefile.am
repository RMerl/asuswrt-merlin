AM_CFLAGS = -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 @STATIC_CFLAGS@

SUBDIRS=po

sbin_PROGRAMS = minidlnad
check_PROGRAMS = testupnpdescgen
minidlnad_SOURCES = minidlna.c upnphttp.c upnpdescgen.c upnpsoap.c \
			upnpreplyparse.c minixml.c clients.c \
			getifaddr.c daemonize.c upnpglobalvars.c \
			options.c minissdp.c uuid.c upnpevents.c \
			sql.c utils.c metadata.c scanner.c inotify.c \
			tivo_utils.c tivo_beacon.c tivo_commands.c \
			tagutils/textutils.c tagutils/misc.c tagutils/tagutils.c \
			playlist.c image_utils.c albumart.c log.c

#if NEED_VORBIS
vorbisflag = -lvorbis
#endif

#if NEED_OGG
flacoggflag = -logg
#endif

minidlnad_LDADD = \
	@LIBJPEG_LIBS@ \
	@LIBID3TAG_LIBS@ \
	@LIBSQLITE3_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBAVUTIL_LIBS@ \
	@LIBEXIF_LIBS@ \
	@LIBINTL@ \
	@LIBICONV@ \
	-lFLAC  $(flacoggflag) $(vorbisflag)

minidlnad_LDFLAGS = @STATIC_LDFLAGS@

testupnpdescgen_SOURCES = testupnpdescgen.c upnpdescgen.c
testupnpdescgen_LDADD = \
	@LIBJPEG_LIBS@ \
	@LIBID3TAG_LIBS@ \
	@LIBSQLITE3_LIBS@ \
	@LIBAVFORMAT_LIBS@ \
	@LIBAVUTIL_LIBS@ \
	@LIBEXIF_LIBS@ \
	-lFLAC  $(flacoggflag) $(vorbisflag)


ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4/ChangeLog
