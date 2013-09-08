include ../common.mak

CFLAGS	= -Os -Wall $(EXTRACFLAGS) #-mips32
CFLAGS	+= -I$(SRCBASE)/include -I$(TOP)/shared -DRTCONFIG_ISP_METER -DDEBUG
LDFLAGS	=

OBJS = rstats.o

all: rstats

rstats: $(OBJS)
	@echo " [rstats] CC -o $@"
	$(CC) $(LDFLAGS) -o $@ $(OBJS) -L$(TOP)/nvram${BCMEX} -lnvram -L$(TOP)/shared -lshared

	$(SIZECHECK)
	$(CPTMP)

clean:
	rm -f rstats .*.depend
	rm -f *.o

install: all
	@echo " [rstats] Installing..."
	install -D rstats $(INSTALLDIR)/bin/rstats
	$(STRIP) $(INSTALLDIR)/bin/rstats
	chmod 0500 $(INSTALLDIR)/bin/rstats

%.o: %.c .%.depend
	@echo " [rstats] CC $@"
	@$(CC) $(CFLAGS) -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
