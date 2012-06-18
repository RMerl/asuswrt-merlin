include ../common.mak

CFLAGS	= -Os -Wall $(EXTRACFLAGS) -fPIC 
LIBS = 
LDFLAGS	= 

CFLAGS	+= -I$(TOP)/openssl/include
LIBS += -L$(TOP)/openssl -lssl -lcrypto

LIBS += -lpthread
OBJ = mssl.o

all: libmssl.so libmssl.a

libmssl.so: $(OBJ)
	$(CC) -shared -o $@ $^ $(LIBS)
	$(STRIP) $@
	$(CPTMP)

libmssl.a: $(OBJ)
	rm -f $@
	$(AR) cruv $@ $^

clean:
	rm -f .*.depend *.o *.so *.a

install: all
	@echo " [mssl] Installing..."
	install -D libmssl.so $(INSTALLDIR)/usr/lib/libmssl.so
	$(STRIP) $(INSTALLDIR)/usr/lib/libmssl.so

%.o: %.c .%.depend
	@echo " [mssl] CC $@"
	@$(CC) $(CFLAGS) -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)
