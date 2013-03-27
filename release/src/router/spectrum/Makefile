include ../common.mak

CFLAGS	= -Os -Wall $(EXTRACFLAGS) #-mips32
CFLAGS	+= -I$(SRCBASE)/include -I$(TOP)/shared -DRTCONFIG_ISP_METER -DDEBUG
LDFLAGS	=

OBJS = spectrum.o

ifeq ($(wildcard $(TOP)/spectrum/*.c),)
	-cp -f $(TOP)/spectrum/prebuilt/spectrum spectrum
else
all: spectrum
endif

spectrum: $(OBJS)
	@echo " [spectrum] CC -o $@"
	@$(CC) $(LDFLAGS) -o $@ $(OBJS) -L../nvram -lnvram -L../shared -lshared

	$(SIZECHECK)
	$(CPTMP)

clean:
	rm -f spectrum .*.depend
	rm -f *.o

install: all
	@echo " [spectrum] Installing..."
	install -D spectrum $(INSTALLDIR)/bin/spectrum
	$(STRIP) $(INSTALLDIR)/bin/spectrum
	chmod 0500 $(INSTALLDIR)/bin/spectrum

%.o: %.c .%.depend
	@echo " [spectrum] CC $@"
	@$(CC) $(CFLAGS) -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
