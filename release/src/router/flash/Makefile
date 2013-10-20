include ../common.mak

CFLAGS += -I. -I$(TOP)/shared -I$(SRCBASE)/include
LDFLAGS += -L$(TOP)/nvram -lnvram
LDFLAGS +=-L$(TOP)/shared -lshared
EXEC = flash
OBJS = flash_mtd.o

ifneq ($(wildcard ./prebuild),)
all:
	-cp prebuild/$(EXEC) $(EXEC)
else
all: $(EXEC) 
endif

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

install: all
	install -D $(EXEC) $(INSTALLDIR)/bin/$(EXEC)
	$(STRIP) $(INSTALLDIR)/bin/$(EXEC)
	chmod 0500 $(INSTALLDIR)/bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.o

