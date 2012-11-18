ifneq ($(KERNELRELEASE),)
include Kbuild
else
KERNELDIR := /lib/modules/`uname -r`/build
all::
	$(MAKE) -C $KERNELDIR M=`pwd` $@

expand_macros: $(patsubst %.c, %.m.c, $(filter-out %.mod.c %.m.c, $(wildcard ip_set_*.c)))

%.m.c: %.c
	./expand_macros.pl < $< > $@

clean:
	rm -rf *.m.c

endif
