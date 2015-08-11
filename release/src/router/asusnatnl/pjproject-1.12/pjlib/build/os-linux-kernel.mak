#
# OS specific configuration for Linux Kernel module target. 
#

#
# PJLIB_OBJS specified here are object files to be included in PJLIB
# (the library) for this specific operating system. Object files common 
# to all operating systems should go in Makefile instead.
#
export PJLIB_OBJS +=	compat/sigjmp.o compat/setjmp_i386.o \
			compat/longjmp_i386.o compat/string.o \
			addr_resolv_linux_kernel.o \
			guid_simple.o \
			log_writer_printk.o pool_policy_kmalloc.o \
			os_error_linux_kernel.o os_core_linux_kernel.o \
			os_time_linux_kernel.o os_timestamp_common.o \
			os_timestamp_linux_kernel.o \
			sock_linux_kernel.o sock_select.o

# For IOQueue, we can use either epoll or select
export PJLIB_OBJS +=	ioqueue_epoll.o 
#export PJLIB_OBJS +=	ioqueue_select.o 

#
# TEST_OBJS are operating system specific object files to be included in
# the test application.
#
export TEST_OBJS +=	main_mod.o

#
# Additional CFLAGS
#
export TEST_CFLAGS += -msoft-float

#
# Additional LD_FLAGS for this target.
#
export TEST_LDFLAGS += -lgcc


#
# TARGETS are make targets in the Makefile, to be executed for this given
# operating system.
#
export TARGETS :=	../lib/pjlib.ko ../lib/pjlib-test.ko


