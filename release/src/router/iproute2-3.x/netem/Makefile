DISTGEN = maketable normal pareto paretonormal
DISTDATA = normal.dist pareto.dist paretonormal.dist experimental.dist

HOSTCC ?= $(CC)
CCOPTS  = $(CBUILD_CFLAGS)
LDLIBS += -lm 

all: $(DISTGEN) $(DISTDATA)

$(DISTGEN):
	$(HOSTCC) $(CCOPTS) -I../include -o $@ $@.c -lm

%.dist: %
	./$* > $@

experimental.dist: maketable experimental.dat
	./maketable experimental.dat > experimental.dist

stats: stats.c
	$(HOSTCC) $(CCOPTS) -I../include -o $@ $@.c -lm

install: all
	mkdir -p $(DESTDIR)$(LIBDIR)/tc
	for i in $(DISTDATA); \
	do install -m 644 $$i $(DESTDIR)$(LIBDIR)/tc; \
	done

clean:
	rm -f $(DISTDATA) $(DISTGEN)
