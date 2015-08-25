
include ../../build/common.mak


###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(PJ_CFLAGS) $(CFLAGS)
export _CXXFLAGS:= $(PJ_CXXFLAGS)
export _LDFLAGS := $(PJ_LDFLAGS) $(PJ_LDLIBS) $(LDFLAGS)

SRCDIR := ../src/samples
OBJDIR := ./output/samples-$(TARGET_NAME)
BINDIR := ../bin/samples/$(TARGET_NAME)

SAMPLES := auddemo \
	   aectest \
	   confsample \
	   encdec \
	   httpdemo \
	   icedemo \
	   jbsim \
	   latency \
	   level \
	   mix \
	   pjsip-perf \
	   pcaputil \
	   playfile \
	   playsine \
	   recfile \
	   resampleplay \
	   simpleua \
	   simple_pjsua \
	   siprtp \
	   sipstateless \
	   stateful_proxy \
	   stateless_proxy \
	   stereotest \
	   streamutil \
	   strerror \
	   tonegen

EXES := $(foreach file, $(SAMPLES), $(BINDIR)/$(file)$(HOST_EXE))

all: $(BINDIR) $(OBJDIR) $(EXES)

$(BINDIR)/%$(HOST_EXE): $(OBJDIR)/%$(OBJEXT) $(PJ_LIB_FILES)
	$(LD) $(LDOUT)$(subst /,$(HOST_PSEP),$@) \
	    $(subst /,$(HOST_PSEP),$<) \
	    $(_LDFLAGS)

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.c
	$(CC) $(_CFLAGS) \
	  $(CC_OUT)$(subst /,$(HOST_PSEP),$@) \
	  $(subst /,$(HOST_PSEP),$<) 

$(OBJDIR):
	$(subst @@,$(subst /,$(HOST_PSEP),$@),$(HOST_MKDIR)) 

$(BINDIR):
	$(subst @@,$(subst /,$(HOST_PSEP),$@),$(HOST_MKDIR)) 

depend:

clean:
	$(subst @@,$(subst /,$(HOST_PSEP),$(OBJDIR)/*),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),$(OBJDIR)),$(HOST_RMDIR))
	$(subst @@,$(EXES),$(HOST_RM))
	rm -rf $(BINDIR)

distclean realclean: clean
#	$(subst @@,$(subst /,$(HOST_PSEP),$(EXES)) $(subst /,$(HOST_PSEP),$(EXES)),$(HOST_RM))
#	$(subst @@,$(DEP_FILE),$(HOST_RM))

