GENLOBJ=genl.o

include ../Config
SHARED_LIBS ?= y

CFLAGS += -fno-strict-aliasing

GENLMODULES :=
GENLMODULES += ctrl.o

GENLOBJ += $(GENLMODULES)

GENLLIB :=

ifeq ($(SHARED_LIBS),y)
LDFLAGS += -Wl,-export-dynamic
LDLIBS  += -lm -ldl
endif

all: genl

genl: $(GENLOBJ) $(LIBNETLINK) $(LIBUTIL) $(GENLLIB)

install: all
	install -m 0755 genl $(DESTDIR)$(SBINDIR)

clean:
	rm -f $(GENLOBJ) $(GENLLIB) genl

ifneq ($(SHARED_LIBS),y)

genl: static-syms.o
static-syms.o: static-syms.h
static-syms.h: $(wildcard *.c)
	files="$^" ; \
	for s in `grep -B 3 '\<dlsym' $$files | sed -n '/snprintf/{s:.*"\([^"]*\)".*:\1:;s:%s::;p}'` ; do \
		sed -n '/'$$s'[^ ]* =/{s:.* \([^ ]*'$$s'[^ ]*\) .*:extern char \1[] __attribute__((weak)); if (!strcmp(sym, "\1")) return \1;:;p}' $$files ; \
	done > $@

endif
