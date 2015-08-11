include build.mak
include build/host-$(HOST_NAME).mak

DIRS = pjlib pjlib-util pjnath pjmedia pjsip

ifdef MINSIZE
MAKE_FLAGS := MINSIZE=1
endif

export CPP_MODE=1

all clean dep depend distclean doc print realclean:
	for dir in $(DIRS); do \
		if $(MAKE) $(MAKE_FLAGS) -C $$dir/build $@; then \
		    true; \
		else \
		    exit 1; \
		fi; \
	done

