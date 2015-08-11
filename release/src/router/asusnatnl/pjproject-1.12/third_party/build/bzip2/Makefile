include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../

RULES_MAK := $(PJDIR)/build/rules.mak

export BZIP2_LIB := ../../lib/libbzip2-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../bzip2 \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) 
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export BZIP2_SRCDIR = ../../bzip2
export BZIP2_OBJS = blocksort.o bzip2.o bzip2recover.o bzlib.o \
			compress.o crctable.o decompress.o huffman.o randtable.o
export BZIP2_INC = ../../bzip2

export BZIP2_CFLAGS = $(_CFLAGS)
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := libbzip2

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend libbzip2 clean realclean distclean

libbzip2:
	$(MAKE) -f $(RULES_MAK) APP=BZIP2 app=libbzip2 $(BZIP2_LIB)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=BZIP2 app=libbzip2 $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=BZIP2 app=libbzip2 $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=BZIP2 app=libbzip2 $@


