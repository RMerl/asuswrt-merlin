
scsiincludedir      = $(includedir)/scsi

if OS_LINUX

scsiinclude_HEADERS = \
	sg_lib.h \
	sg_cmds.h \
	sg_cmds_basic.h \
	sg_cmds_extra.h \
	sg_pt.h \
	sg_linux_inc.h \
	sg_io_linux.h
	
noinst_HEADERS = \
	sg_pt_win32.h

endif


if OS_WIN32_MINGW

scsiinclude_HEADERS = \
	sg_lib.h \
	sg_cmds.h \
	sg_cmds_basic.h \
	sg_cmds_extra.h \
	sg_pt.h \
	sg_pt_win32.h
	
noinst_HEADERS = \
	sg_linux_inc.h \
	sg_io_linux.h

endif


if OS_WIN32_CYGWIN

scsiinclude_HEADERS = \
	sg_lib.h \
	sg_cmds.h \
	sg_cmds_basic.h \
	sg_cmds_extra.h \
	sg_pt.h \
	sg_pt_win32.h
	
noinst_HEADERS = \
	sg_linux_inc.h \
	sg_io_linux.h

endif


if OS_FREEBSD

scsiinclude_HEADERS = \
	sg_lib.h \
	sg_cmds.h \
	sg_cmds_basic.h \
	sg_cmds_extra.h \
	sg_pt.h
	
noinst_HEADERS = \
	sg_linux_inc.h \
	sg_io_linux.h \
	sg_pt_win32.h

endif


if OS_SOLARIS

scsiinclude_HEADERS = \
	sg_lib.h \
	sg_cmds.h \
	sg_cmds_basic.h \
	sg_cmds_extra.h \
	sg_pt.h
	
noinst_HEADERS = \
	sg_linux_inc.h \
	sg_io_linux.h \
	sg_pt_win32.h

endif


if OS_OSF

scsiinclude_HEADERS = \
	sg_lib.h \
	sg_cmds.h \
	sg_cmds_basic.h \
	sg_cmds_extra.h \
	sg_pt.h
	
noinst_HEADERS = \
	sg_linux_inc.h \
	sg_io_linux.h \
	sg_pt_win32.h

endif

