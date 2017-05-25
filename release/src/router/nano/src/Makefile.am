AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -DSYSCONFDIR=\"$(sysconfdir)\" \
	-I$(top_builddir)/lib -I$(top_srcdir)/lib

CLEANFILES = revision.h

if BUILDING_FROM_GIT
SOMETHING = "REVISION \"`git describe --tags 2>/dev/null`\""
else
SOMETHING = "NOTHING \"from tarball\""
endif

nano.o: revision.h
winio.o: revision.h

# Always run the commands in this rule.  But update the target
# only when the revision actually changed.
revision.h: FORCE
	@[ -f $@ ] || touch $@
	@echo "#define $(SOMETHING)" | cmp -s $@ - || \
			echo "#define $(SOMETHING)" > $@
FORCE:

bin_PROGRAMS = 	nano
nano_SOURCES =	browser.c \
		chars.c \
		color.c \
		cut.c \
		files.c \
		global.c \
		help.c \
		move.c \
		nano.c \
		nano.h \
		prompt.c \
		proto.h \
		rcfile.c \
		search.c \
		text.c \
		utils.c \
		winio.c 

nano_LDADD = @LIBINTL@ $(top_builddir)/lib/libgnu.a $(LIB_CLOCK_GETTIME)

install-exec-hook:
	cd $(DESTDIR)$(bindir) && rm -f rnano && $(LN_S) nano rnano
uninstall-hook:
	cd $(DESTDIR)$(bindir) && rm -f rnano
