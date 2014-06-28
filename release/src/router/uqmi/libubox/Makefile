CFLAGS  := --std=gnu99 -g -Os $(EXTRACFLAGS)
CFLAGS  += -Wall -Werror -Wmissing-declarations
CFLAGS  += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections

TARGET = libubox.a
SOURCES = avl.c avl-cmp.c blob.c blobmsg.c uloop.c usock.c ustream.c ustream-fd.c vlist.c utils.c safe_list.c runqueue.c

OBJS=$(SOURCES:.c=.o)

# Uncomment next line in case of clock_gettime() in librt
#LIBS=-lrt

PREFIX=/usr

default_target: all
.PHONY : default_target

install: all
	install -D $(TARGET) $(INSTALLDIR)/$(PREFIX)/lib/$(TARGET)
	$(STRIP) $(INSTALLDIR)/$(PREFIX)/lib/$(TARGET)
.PHONY : install

clean:
	rm -f $(OBJS) $(TARGET)
.PHONY : clean

# The main all target
all: $(TARGET)
.PHONY : all

# Build rule for targets
$(TARGET): $(OBJS)
	$(AR) cr $@ $(OBJS)
	$(RANLIB) $@

.c.o::
	$(CC) $(CFLAGS) -c $<

# Dependencies
avl-cmp.o: avl-cmp.c avl-cmp.h
avl.o: avl.c avl.h
blob.o: blob.c blob.h
blobmsg.o: blobmsg.c blobmsg.h
uloop.o: uloop.c uloop.h utils.h
usock.o: usock.c usock.h
ustream-fd.o: ustream-fd.c ustream.h
ustream.o: ustream.c ustream.h
utils.o: utils.c utils.h
vlist.o: vlist.c vlist.h
safe_list.o: safe_list.c safe_list.h
runqueue.o: runqueue.c runqueue.h

# Headers
avl.h: list.h list_compat.h
blob.h: utils.h
blobmsg.h: blob.h
safe_list.h: list.h utils.h
uloop.h: list.h
ustream.h: uloop.h
vlist.h: avl.h
runqueue.h: list.h safe_list.h uloop.h
