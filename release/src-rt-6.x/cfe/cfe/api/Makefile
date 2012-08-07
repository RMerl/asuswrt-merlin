TOOLS_PREFIX = sb1-elf-
CPPFLAGS = -I../include
CFLAGS = -O2 -fomit-frame-pointer \
	 -Wall -Wstrict-prototypes -Wmissing-prototypes -Werror

CC = $(TOOLS_PREFIX)gcc
AR = $(TOOLS_PREFIX)ar

all: cfe_api.a cfe_api.o

API_OBJS =			\
	api_close.o		\
	api_common.o		\
	api_cpu_start.o		\
	api_cpu_stop.o		\
	api_enumenv.o		\
	api_enummem.o		\
	api_exit.o		\
	api_flushcache.o	\
	api_getdevinfo.o	\
	api_getenv.o		\
	api_getfwinfo.o		\
	api_getstdhandle.o	\
	api_getticks.o		\
	api_inpstat.o		\
	api_ioctl.o		\
	api_open.o		\
	api_read.o		\
	api_readblk.o		\
	api_setenv.o		\
	api_strlen.o		\
	api_write.o		\
	api_writeblk.o

cfe_api.a: $(API_OBJS)
	$(AR) cru cfe_api.a $?

$(API_OBJS): api_%.o: cfe_api.c Makefile cfe_api.h cfe_api_int.h
	$(CC) -c -o $@ -DCFE_API_$* $(CPPFLAGS) $(CFLAGS) $<

cfe_api.o: cfe_api.c Makefile cfe_api.h cfe_api_int.h
	$(CC) -c -o $@ -DCFE_API_ALL $(CPPFLAGS) $(CFLAGS) $<

.PHONY: clean

clean:
	rm -f *.o *.a *~
