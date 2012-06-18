
teventdir="\$(libteventsrcdir)"
m4_include(../lib/tevent/libtevent.m4)

SMB_EXT_LIB(LIBTEVENT_EXT, [${TEVENT_LIBS}])
SMB_ENABLE(LIBTEVENT_EXT)

SMB_SUBSYSTEM(LIBTEVENT,
	[\$(addprefix \$(libteventsrcdir)/, ${TEVENT_OBJ})],
	[LIBTEVENT_EXT],
	[${TEVENT_CFLAGS}])
