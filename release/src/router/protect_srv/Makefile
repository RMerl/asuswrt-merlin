include ./Makefile.config

ifeq ($(ASUSWRT_SDK),y)
include $(SRCBASE)/router/.config
MAKE = make
CFLAGS = -Wall -O2 -g  $(EXTRACFLAGS)
CFLAGS += -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/protect_srv/include/
CFLAGS += -DASUSWRT_SDK

ifeq ($(RTCONFIG_NOTIFICATION_CENTER),y)
CFLAGS  += -I$(TOP)/sqlite
CFLAGS  += -I$(TOP)/nt_center/lib
LDFLAGS += -L$(TOP)/nt_center/lib/ -lnt
LDFLAGS += -L$(TOP)/sqlite/.libs -lsqlite3
LDFLAGS += -lpthread #Depend on sqlite
endif

ifeq ($(CONFIG_KERNEL_PTHREAD_ONE),y)
CFLAGS += -DCONFIG_LINUX3X_OR_4X
endif
ifeq ($(HND_ROUTER),y)
CFLAGS += -DNON_P_THREAD
endif

LDFLAGS += -L$(TOP)/protect_srv/lib/ -lptcsrv

LIBS = -lpthread -lgcc_s

MDIRS = lib

ifeq ($(wildcard $(SRCBASE)/router/protect_srv/*.c),)
all: 
	-cp -f prebuilt/protect_srv protect_srv
	-cp -f prebuilt/Send_Event2ptcsrv Send_Event2ptcsrv
	-cp -f lib/prebuilt/libptcsrv.so lib
	-cp -f lib/prebuilt/*.h lib

else
all: ptcsrvdir Send_Event2ptcsrv protect_srv
endif

protect_srv: protect_srv.o
Send_Event2ptcsrv: Send_Event2ptcsrv.o

ptcsrvdir:
	set -e; for d in ${MDIRS}; do $(MAKE) -C $${d}; done
protect_srv:
	@$(CC) ${CFLAGS} $< -o $@ ${LDFLAGS} $(LIBS)
	@$(STRIP) $@

Send_Event2ptcsrv:
	@$(CC) ${CFLAGS} $< -o $@ ${LDFLAGS} $(LIBS)
	@$(STRIP) $@


else # DSL_ASUSWRT_SDK 

CC = mips-linux-uclibc-gcc
MAKE = make 

CFLAGS = -Wall -O2 -g 
CFLAGS += $(TC_CFLAGS) -I.
CFLAGS += -I$(APP_PROTECTION_SERVER_DIR)/include/

LDFLAGS = -lm
LDFLAGS += -L$(APP_PROTECTION_SERVER_DIR)/lib/ -lptcsrv
LDFLAGS += -lpthread
LIBS =

MDIRS+= lib

all: ptcsrvdir Send_Event2ptcsrv protect_srv

protect_srv: protect_srv.o
Send_Event2ptcsrv: Send_Event2ptcsrv.o

ptcsrvdir:
	set -e; for d in ${MDIRS}; do $(MAKE) -C $${d}; done

protect_srv:
	@${CC} ${CFLAGS} $< -o $@ ${LDFLAGS}
Send_Event2ptcsrv:
	@${CC} ${CFLAGS} $< -o $@ ${LDFLAGS}

endif

.PHONY : all clean
clean:
	set -e; for d in ${MDIRS}; do $(MAKE) -C $${d} clean; done
	rm -rf *.o protect_srv Send_Event2ptcsrv

