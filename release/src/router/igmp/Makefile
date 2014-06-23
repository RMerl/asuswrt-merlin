include ../common.mak

#CROSS =
#CC = $(CROSS)gcc
#AR = $(CROSS)ar
#STRIP = $(CROSS)strip

ALL_OBJS = igmprt.o proxy.o igmpv2.o util.o filter.o

CCFLAGS += -DHND_FIX -Wall -I$(SRCBASE)/include -I$(TOP)/shared -s -Os -fomit-frame-pointer
ifeq (2_6_36,$(LINUX_VERSION))
CCFLAGS += -I$(SRCBASE)/common/include
endif
LDFLAGS += -Wl,-allow-shlib-undefined 
LDFLAGS += -L$(TOP)/netconf -L$(INSTALLDIR)/netconf/usr/lib -lnetconf

ifeq ($(LINUX_VERSION),2_6_36)
CCFLAGS += -DLINUX_2_6_36
endif

ifneq ($(wildcard ./prebuilt),)
all:
	-cp prebuilt/igmp igmp
else
all: igmp
endif

igmp: $(ALL_OBJS)
	$(CC) $(CCFLAGS) $(LDFLAGS) -o $@ $(ALL_OBJS) $(LDLIBS)
	$(STRIP) $@

$(ALL_OBJS): %.o: %.c
	@echo " "
	@echo "Making $<"
	$(CC) $(CCFLAGS) -c $<

install: all
	install -d $(INSTALLDIR)/igmp/usr/sbin
	install -m 755 igmp $(INSTALLDIR)/igmp/usr/sbin

.PHONY:	clean
clean:
	-rm -f *.o *~ core igmp
