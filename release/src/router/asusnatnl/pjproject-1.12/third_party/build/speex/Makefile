include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

export SPEEX_LIB := ../../lib/libspeex-$(TARGET_NAME)$(LIBEXT)

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../speex/include \
		   $(CC_INC)../../speex/libspeex \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS) 

export SPEEX_SRCDIR = ../../speex/libspeex
export SPEEX_OBJS = 	bits.o cb_search.o exc_10_16_table.o  \
			exc_10_32_table.o exc_20_32_table.o \
			exc_5_256_table.o exc_5_64_table.o \
			exc_8_128_table.o fftwrap.o filterbank.o \
			filters.o gain_table.o gain_table_lbr.o \
			hexc_10_32_table.o hexc_table.o \
			high_lsp_tables.o \
			kiss_fft.o kiss_fftr.o lpc.o \
			lsp.o lsp_tables_nb.o ltp.o \
			mdf.o modes.o modes_wb.o \
			nb_celp.o preprocess.o \
			quant_lsp.o resample.o sb_celp.o smallft.o \
			speex.o speex_callbacks.o speex_header.o \
			stereo.o vbr.o vq.o window.o

export SPEEX_CFLAGS = -DHAVE_CONFIG_H $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT 
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
TARGETS := libspeex

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

.PHONY: dep depend libspeex clean realclean distclean

libspeex:
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $(SPEEX_LIB)

clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.ilbc-$(TARGET_NAME).depend),$(HOST_RMR))
	
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=SPEEX app=libspeex $@


