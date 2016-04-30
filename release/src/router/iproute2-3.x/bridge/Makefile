BROBJ = bridge.o fdb.o monitor.o link.o mdb.o vlan.o

include ../Config

ifeq ($(IP_CONFIG_SETNS),y)
	CFLAGS += -DHAVE_SETNS
endif

all: bridge

bridge: $(BROBJ) $(LIBNETLINK) 

install: all
	install -m 0755 bridge $(DESTDIR)$(SBINDIR)

clean:
	rm -f $(BROBJ) bridge

