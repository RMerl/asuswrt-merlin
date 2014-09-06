include ../common.mak

CFLAGS = -O2 -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I$(TOP)/shared -I$(SRCBASE)/include -I.
LDFLAGS += -L$(TOP)/nvram -lnvram -L$(TOP)/shared -lshared

all: libnmp.so

libnmp.so: nmp.o
	@echo " [libnmp] CC $@"
#	@$(CC) -shared $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@$(CC) -shared $(CFLAGS) -o $@ $^

install: all
	install -D libnmp.so $(INSTALLDIR)/usr/lib/libnmp.so

clean:
	rm -f *.o *.a *.so
	
%.o: %.c .%.depend
	@echo " [libnmp] CC $@"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
