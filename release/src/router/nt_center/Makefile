include ./Makefile.config

ifeq ($(ASUSWRT_SDK),y)
MAKE = make
CFLAGS = -Wall -O2 -g  $(EXTRACFLAGS)
CFLAGS += -I$(SRCBASE) -I$(SRCBASE)/include -I. -I$(TOP)/nt_center/lib/
CFLAGS += -I$(TOP)/sqlite 
CFLAGS += -I$(TOP)/shared #Just For rtconfig.h , do not include share library.
CFLAGS += -DASUSWRT_SDK

LDFLAGS = -L$(TOP)/nt_center/lib/ -lnt
LDFLAGS += -L$(TOP)/sqlite/.libs -lsqlite3
LIBS = -lpthread -lgcc_s

MDIRS = lib actMail

all: ntdir nt_center Notify_Event2NC  nt_monitor

nt_center: nt_center.o
Notify_Event2NC: Notify_Event2NC.o
nt_monitor: nt_monitor.o

ntdir:
	set -e; for d in ${MDIRS}; do $(MAKE) -C $${d}; done
nt_center:
	$(CC) ${CFLAGS} $< -o $@ ${LDFLAGS} $(LIBS)
	@$(STRIP) $@

Notify_Event2NC:
	$(CC) ${CFLAGS} $< -o $@ ${LDFLAGS} $(LIBS)
	@$(STRIP) $@

nt_monitor:
	$(CC) ${CFLAGS} $< -o $@ ${LDFLAGS} $(LIBS)
	@$(STRIP) $@

else 

CC = mips-linux-uclibc-gcc
MAKE = make 

CFLAGS = -Wall -O2 -g 
CFLAGS += $(TC_CFLAGS) -I.
CFLAGS += -I$(APP_NOTIFICATION_CENTER_DIR)/lib/
CFLAGS += -I$(APP_SQLITE_DIR)/

LDFLAGS = -L$(APP_NOTIFICATION_CENTER_DIR)/lib/ -lnt -lpthread
LDFLAGS += -L$(APP_SQLITE_DIR)/.libs -lsqlite3
LIBS =

MDIRS+= lib actMail


all: ntdir nt_center Notify_Event2NC nt_monitor

nt_center : nt_center.o 
Notify_Event2NC  : Notify_Event2NC.o
nt_monitor: nt_monitor.o

ntdir:
	set -e; for d in ${MDIRS}; do $(MAKE) -C $${d}; done

nt_center:
	${CC} ${CFLAGS} $< -o $@ ${LDFLAGS}
Notify_Event2NC:
	${CC} ${CFLAGS} $< -o $@ ${LDFLAGS}
nt_monitor:
	$(CC) ${CFLAGS} $< -o $@ ${LDFLAGS} $(LIBS)

endif

.PHONY : all clean
clean:
	set -e; for d in ${MDIRS}; do $(MAKE) -C $${d} clean; done
	rm -rf *.o nt_center Notify_Event2NC nt_monitor

