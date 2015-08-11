#
# This file is used by get-footprint.py script to build samples/footprint.c
# to get the footprint report for PJSIP/PJMEDIA.
#
include ../../build.mak
include ../../build/common.mak


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(APP_CFLAGS) $(CFLAGS)
export _CXXFLAGS:= $(_CFLAGS)

export _LDFLAGS := $(APP_LDFLAGS) $(APP_LDLIBS) $(LDFLAGS)

EXE := footprint.exe

all: 
	$(APP_CC) -o $(EXE) ../src/samples/footprint.c $(FCFLAGS) $(_CFLAGS) $(_LDFLAGS)
	$(CROSS_COMPILE)strip --strip-all $(EXE)

clean:
	rm -f $(EXE)

print_name:
	@echo $(MACHINE_NAME) $(OS_NAME) $(CROSS_COMPILE)$(CC_NAME) `$(CROSS_COMPILE)$(CC_NAME) -dumpversion`

