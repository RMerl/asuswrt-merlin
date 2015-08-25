#
# OS specific configuration for Win32 OS target. 
#

#
# PJLIB_OBJS specified here are object files to be included in PJLIB
# (the library) for this specific operating system. Object files common 
# to all operating systems should go in Makefile instead.
#
export PJLIB_OBJS += 	addr_resolv_sock.o guid_win32.o  \
			log_writer_stdout.o os_core_win32.o \
			os_error_win32.o os_time_bsd.o os_timestamp_common.o \
			os_timestamp_win32.o \
			pool_policy_malloc.o sock_bsd.o sock_select.o

#export PJLIB_OBJS +=	ioqueue_winnt.o
export PJLIB_OBJS +=	ioqueue_select.o

export PJLIB_OBJS +=	file_io_win32.o file_access_win32.o
#export PJLIB_OBJS +=	file_io_ansi.o

#
# TEST_OBJS are operating system specific object files to be included in
# the test application.
#
export TEST_OBJS +=	main.o

#
# TARGETS are make targets in the Makefile, to be executed for this given
# operating system.
#
export TARGETS	    =	pjlib pjlib-test

