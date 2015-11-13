#
# This is the Makefile for wtfd.
#
# Copyright (c) 2014, AAA Internet Publishing, Inc. (DBA WTFast)
#


LIBS = -lev -lwtf -lgpnutils -lssl -lcrypto -lm -lcurl


ifeq ($(ROUTER_BUILD), y)
	LIBS += -lshared -lgcc_s -lnvram
endif

LDFLAGS += -Wl,-rpath -Wl,$(LIB)

SRCS = wtfd.c nvram.c
OBJS = $(SRCS:.c=.o)
HDRS = $(SRCS:.c=.h)

TARGETS = wtfd

INC = $(DESTDIR)/usr/include

INITD = /etc/init.d


all: $(TARGETS)

$(TARGETS):	$(OBJS)
	$(CC) $(CFLAGS)  -o $@ $(OBJS)  $(LDFLAGS) $(LIBS)

clean:
	@rm -f $(TARGETS) $(OBJS)

tags:
	@ctags -t $(SRCS) $(HDRS)

init:
	useradd wtfd -s /usr/sbin/nologin -d /var/run/wtfd
	install -o 0 -g 0 -m 755 scripts/wtfd $(INITD)
	update-rc.d wtfd defaults
	
testping:
	gcc -g -o testping util.c libpingd.c -lcurl

testy: main.c
	gcc -g main.c  -lwtf   -o testy
