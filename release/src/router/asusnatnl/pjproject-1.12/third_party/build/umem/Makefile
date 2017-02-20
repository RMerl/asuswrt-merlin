include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../

RULES_MAK := $(PJDIR)/build/rules.mak

export UMEM_LIB := ../../lib/libumem-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../umem \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS) 
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export UMEM_SRCDIR = ../../umem
export UMEM_OBJS = init_lib.o \
					umem_agent_support.o \
					umem_fail.o \
					umem_fork.o \
					umem_update_thread.o \
					vmem_mmap.o \
					vmem_sbrk.o \
					envvar.o \
					getpcstack.o \
					misc.o \
					vmem_base.o \
					umem.o \
					vmem.o
export UMEM_INC = ../../umem
ifneq (,$(findstring mips,$(CC)))
    USER_UMEM_CFLAGS+=-DMIPS
endif
ifneq (,$(findstring arm,$(CC)))
    USER_UMEM_CFLAGS+=-DARM
endif
ifneq (,$(findstring aarch64,$(CC)))
    USER_UMEM_CFLAGS+=-DARM64
endif
ifneq (,$(findstring android,$(CC)))
    USER_UMEM_CFLAGS+=-DANDROID
endif
ifneq (,$(findstring x86_64,$(TARGET_NAME)))
    USER_UMEM_CFLAGS+=-DX86_64
endif
ifneq (,$(findstring asustor,$(TARGET_NAME)))
    USER_UMEM_CFLAGS+=-DASUSTOR
endif
#USER_UMEM_CFLAGS+=-DUMEM_STANDALONE
export UMEM_CFLAGS = $(_CFLAGS) $(USER_UMEM_CFLAGS) 
export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := libumem

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend libumem clean realclean distclean

libumem:
	$(MAKE) -f $(RULES_MAK) APP=UMEM app=libumem $(UMEM_LIB)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=UMEM app=libumem $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.libumem-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=UMEM app=libumem $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=UMEM app=libumem $@


