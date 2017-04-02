include ../Makefile.config

ifeq ($(ASUSWRT_SDK),y)
include $(SRCBASE)/router/.config

CFLAGS = -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. -I$(TOP)/protect_srv/include
CFLAGS += -DASUSWRT_SDK

ifeq ($(RTCONFIG_QCA),y)
LDFLAGS += -L$(LIBDIR)
EXTRACC += -ldl
endif
LDFLAGS += -lm

LD_BINARY  += $(LDFLAGS) $(EXTRACC)
LD_LIBRARY += $(LDFLAGS) $(EXTRALD)

OBJS = libptcsrv.o

ifneq ($(RTCONFIG_NOTIFICATION_CENTER),y)
OBJS += linklist.o
endif

vpath %.c $(SRCBASE)/protect_srv/

ifeq ($(wildcard $(SRCBASE)/router/protect_srv/lib/*.c),)
all: 
	-cp -f prebuilt/libptcsrv.so libptcsrv.so
else
all: libptcsrv.so libptcsrv.a
endif

libptcsrv.so: $(OBJS)
	@echo " [$@] LD $@"
	@$(LD) $(LD_LIBRARY) -shared -o $@ $^
	@$(SIZECHECK)
	@$(CPTMP)

libptcsrv.a: $(OBJS)
	@echo " [$@] AR $@"
	@rm -f $@
	@$(AR) cruv $@ $^

clean:
	rm -f *.o *.so *.a .*.depend

%.o: %.c .%.depend
	@echo " [libptcsrv] CC $@ $(CFLAGS)"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

else # DSL_ASUSWRT_SDK

CFLAGS = -Os -Wall $(EXTRACFLAGS) -fPIC
CFLAGS += -I. $(TC_CFLAGS)
CFLAGS += -I$(APP_PROTECTION_SERVER_DIR)/include

LDFLAGS += -lm

OBJS = libptcsrv.o

ifneq ($(RTCONFIG_NOTIFICATION_CENTER),y)
OBJS += linklist.o
endif

all: libptcsrv.so libptcsrv.a

libptcsrv.so: $(OBJS)
	@echo " [$@] LD $@"
	@$(CC) -shared $(CFLAGS) -o $@ $^
	@$(STRIP) $@

libptcsrv.a: $(OBJS)
	@echo " [$@] AR $@"
	@rm -f $@
	@$(AR) cruv $@ $^


clean:
	rm -f *.o *.so *.a .*.depend

%.o: %.c .%.depend
	@echo " [$@] CC $@ $(CFLAGS)"
	@$(CC) $(CFLAGS) -o $@ -c $<

.depend: $(OBJS:%.o=%.c)
	@$(CC) $(CFLAGS) -M $^ > .depend

.%.depend: %.c
	@$(CC) $(CFLAGS) -M $< > $@

-include $(OBJS:%.o=.%.depend)

endif

