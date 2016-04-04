include ../Makefile.config

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

all: nt_actMail

nt_actMail: nt_actMail.o

nt_actMail:
	$(CC) ${CFLAGS} -o $@ $^ ${LDFLAGS} $(LIBS)


.PHONY : all clean
clean:
	rm -rf *.o nt_actMail
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

all: nt_actMail

nt_actMail: nt_actMail.o

nt_actMail:
	$(CC) ${CFLAGS} -o $@ $^ ${LDFLAGS} $(LIBS)


.PHONY : all clean
clean:
	rm -rf *.o nt_actMail
endif
