include ./Makefile.config

ifeq ($(ASUSWRT_SDK),y)
include $(SRCBASE)/router/.config
MAKE = make
CFLAGS = -Wall -O2 -g  $(EXTRACFLAGS)
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include -I$(SRCBASE)/common/include
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

LDFLAGS += -L$(TOP)/nvram$(BCMEX) ${EXTRA_NV_LDFLAGS} -lnvram
LDFLAGS += -L$(TOP)/shared -L$(INSTALLDIR)/shared/usr/lib -lshared
LDFLAGS += -L$(TOP)/protect_srv/lib/ -lptcsrv
ifeq ($(RTCONFIG_QTN),y)
CFLAGS += -I$(TOP)/libqcsapi_client
LDFLAGS += -L$(TOP)/libqcsapi_client -lqcsapi_client
endif
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
CFLAGS += -I. -I$(APP_SHARED_DIR) -I$(APP_TCAPILIB_DIR) $(TC_CFLAGS)

LDFLAGS = -L$(APP_SHARED_DIR) -lshared -L$(APP_TCAPILIB_DIR) -ltcapi
LDFLAGS += -L$(APP_PROTECTION_SERVER_DIR)/lib/ -lptcsrv
LDFLAGS += -lpthread

ifneq ($(strip $(RTCONFIG_NOTIFICATION_CENTER)),)
CFLAGS += -I$(APP_NOTIFICATION_CENTER_DIR) -I$(APP_NOTIFICATION_CENTER_DIR)/lib/
CFLAGS += -I$(APP_SQLITE_DIR)/
LDFLAGS += -L$(APP_NOTIFICATION_CENTER_DIR)/lib -lnt -lm
LDFLAGS += -L$(APP_SQLITE_DIR)/.libs -lsqlite3
endif
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

