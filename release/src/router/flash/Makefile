include ../common.mak

CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include
LDFLAGS += -L$(TOP)/nvram -lnvram
LDFLAGS +=-L$(TOP)/shared -lshared
EXEC = flash
OBJS = flash_mtd.o

ifeq ($(RTN14U),y)
ifneq ($(wildcard ./.prebuilt/$(EXEC)),)
all:
	-cp .prebuilt/$(EXEC) $(EXEC)
else
all: $(EXEC) 
endif
else
ifneq ($(wildcard ./prebuilt),)
all:
	-cp prebuilt/$(EXEC) $(EXEC)
else
all: $(EXEC) 
endif
endif

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

install: all
	install -D $(EXEC) $(INSTALLDIR)/bin/$(EXEC)
	$(STRIP) $(INSTALLDIR)/bin/$(EXEC)
	chmod 0500 $(INSTALLDIR)/bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.o

