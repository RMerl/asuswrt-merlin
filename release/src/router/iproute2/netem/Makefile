DISTGEN = maketable normal pareto paretonormal
DISTDATA = normal.dist pareto.dist paretonormal.dist experimental.dist

HOSTCC ?= $(CC)
LDLIBS += -lm 

all: $(DISTGEN) $(DISTDATA)

$(DISTGEN):
	$(HOSTCC) $(CCOPTS) -I../include -o $@ $@.c -lm

%.dist: %
	./$* > $@

experimental.dist: maketable experimental.dat
	./maketable experimental.dat > experimental.dist

install: all
	mkdir -p $(DESTDIR)/usr/lib/tc
	for i in $(DISTDATA); \
	do install -m 755 $$i $(DESTDIR)/usr/lib/tc; \
	done

clean:
	rm -f $(DISTDATA) $(DISTGEN)
