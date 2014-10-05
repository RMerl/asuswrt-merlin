include ../common.mak

CFLAGS  += -I. -I$(SRCBASE)/include -Wall -fPIC $(EXTRACFLAGS)
#CFLAGS += -g -DDEBUG
CFLAGS  += -s
ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
CFLAGS  += -Os
else
CFLAGS  += -O2
endif
LDFLAGS += -L. -L$(TOP)/shared -lshared

CFLAGS += -I$(TOP)/shared

ifeq ($(STATIC),1)
CFLAGS += -static
endif

all: libnvram.so libnvram.a nvram

libnvram.so: nvram_linux.o nvram_convert.o
	@echo " [nvram] CC $@"
	@$(CC) -shared $(CFLAGS) -o $@ $^

libnvram.a: nvram_linux.o nvram_convert.o
	@echo " [nvram] AR $@"
	@$(AR) cruv $@ $^
	
nvram: nvram.o ../shared/defaults.o libnvram.so
	@echo " [nvram] CC $@"
ifeq ($(STATIC),1)
	$(CC) $(CFLAGS) -static -o $@ nvram.o ../shared/defaults.o $(LDFLAGS) -lnvram
else
	@$(CC) $(CFLAGS) -o $@ nvram.o ../shared/defaults.o $(LDFLAGS) -lnvram
endif
	
	$(SIZECHECK)
	$(CPTMP)
	
install: all
	install -D libnvram.so $(INSTALLDIR)/usr/lib/libnvram.so
	install -D nvram $(INSTALLDIR)/bin/nvram
	$(STRIP) $(INSTALLDIR)/usr/lib/libnvram.so
	$(STRIP) $(INSTALLDIR)/bin/nvram
	chmod 0555 $(INSTALLDIR)/bin/nvram

clean:
	rm -f nvram *.o *.a *.so

	
%.o: %.c .%.depend
	@echo " [nvram] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
