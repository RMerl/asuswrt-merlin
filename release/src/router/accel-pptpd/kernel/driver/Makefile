MDIR = extra

KDIR ?= $(shell sh find_kernel_headers)

obj-m      += pptp.o
#obj-m      += gre.o

CURRENT = $(shell uname -r)

ifndef MAKING_MODULES
all: kernel_headers
	make -C $(KDIR) SUBDIRS=$(PWD) modules
endif


ifneq (,$(findstring 2.4.,$(CURRENT)))
install:
	@if test ! -d /lib/modules/$(CURRENT)/extra; then \
	    mkdir /lib/modules/$(CURRENT)/extra; \
	fi; \
	cp -v $(TARGET).o /lib/modules/$(CURRENT)/extra/$(TARGET).o && /sbin/depmod -a
else
install:	
	make -C $(KDIR) M=$(PWD) modules_install
endif

kernel_headers:
	@if test -z "$(KDIR)"; then \
	    echo "kernel headers not found"; \
	    exit 1; \
	else \
	    echo "using \"$(KDIR)\" kernel headers"; \
	fi

default: all

clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c

ifneq (,$(findstring 2.4.,$(CURRENT)))
include $(KDIR)/Rules.make
endif
