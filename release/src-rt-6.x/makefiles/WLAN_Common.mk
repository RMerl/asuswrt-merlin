ifdef _WLAN_COMMON_MK
$(if $D,$(info Info: Avoiding redundant include ($(MAKEFILE_LIST))))
else	# _WLAN_COMMON_MK
_WLAN_COMMON_MK := 1
unexport _WLAN_COMMON_MK	# in case of make -e

################################################################
# Summary and Namespace Rules
################################################################
#   This is a special makefile fragment intended for common use.
# The most important design principle is that it be used only to
# define variables and functions in a tightly controlled namespace.
# If a make include file is used to set rules, pattern rules,
# or well known variables like CFLAGS, it can have unexpected
# effects on the including makefile, with the result that people
# either stop including it or stop changing it.
# Therefore, the only way to keep this a file which can be
# safely included by any GNU makefile and extended at will is
# to allow it only to set variables and only in its own namespace.
#   The namespace is "WLAN_CamelCase" for normal variables,
# "wlan_lowercase" for functions, and WLAN_UPPERCASE for boolean
# "constants" (these are all really just make variables; only the
# usage patterns differ).
#   Internal (logically file-scoped) variables are prefixed with "-"
# and have no other namespace restrictions.
#   Every variable defined here should match one of these patterns.

################################################################
# Enforce required conditions
################################################################

ifneq (,$(filter 3.7% 3.80,$(MAKE_VERSION)))
$(error $(MAKE): Error: version $(MAKE_VERSION) too old, 3.81+ required)
endif

################################################################
# Shiny new ideas, can be enabled via environment for testing.
################################################################

ifdef WLAN_MakeBeta
$(info Info: BUILDING WITH "WLAN_MakeBeta" ENABLED!)
SHELL := /bin/bash
#.SUFFIXES:
#MAKEFLAGS += -R
endif

################################################################
# Allow a makefile to force this file into all child makes.
################################################################

ifdef WLAN_StickyCommon
export MAKEFILES := $(MAKEFILES) $(abspath $(lastword $(MAKEFILE_LIST)))
endif

################################################################
# Host type determination
################################################################

_common-uname-s := $(shell uname -s)

# Typically this will not be tested explicitly; it's the default condition.
WLAN_HOST_TYPE := unix

ifneq (,$(filter Linux,$(_common-uname-s)))
  WLAN_LINUX_HOST := 1
else ifneq (,$(filter CYGWIN%,$(_common-uname-s)))
  WLAN_CYGWIN_HOST := 1
  WLAN_WINDOWS_HOST := 1
else ifneq (,$(filter Darwin,$(_common-uname-s)))
  WLAN_MACOS_HOST := 1
  WLAN_BSD_HOST := 1
else ifneq (,$(filter FreeBSD NetBSD,$(_common-uname-s)))
  WLAN_BSD_HOST := 1
else ifneq (,$(filter SunOS%,$(_common-uname-s)))
  WLAN_SOLARIS_HOST := 1
endif

################################################################
# Utility variables
################################################################

empty :=
space := $(empty) $(empty)
comma := ,

################################################################
# Utility functions
################################################################

# Provides enhanced-format messages from make logic.
wlan_die = $(error Error: $1)
wlan_warning = $(warning Warning: $1)
wlan_info = $(info Info: $1)

# Debug function to enable make verbosity.
wlan_dbg = $(if $D,$(call wlan_info,$1))

# Debug function to expose values of the listed variables.
wlan_dbgv = $(foreach _,$1,$(call wlan_dbg,$_=$($_)))

# Make-time assertion.
define wlan_assert
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
$(if $1,,$(call wlan_die,$2))
endif
endef

# Checks for the presence of an option in an option string
# like "aaa-bbb-ccc-ddd".
wlan_opt = $(if $(findstring -$1-,-$2-),1,0)

# This is a useful macro to wrap around a compiler command line,
# e.g. "$(call wlan_cc,<command-line>). It organizes flags in a
# readable way while taking care not to change any ordering
# which matters. It also provides a hook for externally
# imposed C flags which can be passed in from the top level.
# TODO this would be the best place to add a check for
# command line length. Requires a $(strlen) function;
# GMSL has one.
define wlan_cc
$(filter-out -D% -I%,$1) $(filter -D%,$1) $(filter -I%,$1) $(WLAN_EXTERNAL_CFLAGS)
endef

################################################################
# Standard make variables
################################################################

# This points to the root of the build tree, currently known to
# be 2 levels above the directory of this file.
ifndef WLAN_TreeBaseA
ifdef WLAN_CYGWIN_HOST
WLAN_TreeBaseA := $(shell cygpath -m -a $(dir $(lastword $(MAKEFILE_LIST)))../..)
else
WLAN_TreeBaseA := $(subst \,/,$(realpath $(dir $(lastword $(MAKEFILE_LIST)))../..))
endif
endif

# We've observed a bug, or at least a surprising behavior, in emake which
# causes the $(realpath) above to fail so this fallback is used.
ifndef WLAN_TreeBaseA
WLAN_TreeBaseA := $(shell cd $(dir $(lastword $(MAKEFILE_LIST)))../.. && pwd)
endif

# Export these values so they can be used by scripts or nmake/pmake makefiles.
export WLAN_TreeBaseA

# We may eventually remove this and require an
# explict absolute-vs-relative choice.
export WLAN_TreeBase := $(WLAN_TreeBaseA)

# Pick up the "relpath" make function.
include $(WLAN_TreeBaseA)/src-rt-6.x/makefiles/RelPath.mk

# This is a relativized version of $(WLAN_TreeBaseA).
export WLAN_TreeBaseR = $(call relpath,$(WLAN_TreeBaseA))

# For compatibility, due to the prevalence of $(SRCBASE)
WLAN_SrcBaseA := $(WLAN_TreeBaseA)/src
WLAN_SrcBaseR  = $(patsubst %/,%,$(dir $(WLAN_TreeBaseR)))

# Show makefile list before we start including things.
$(call wlan_dbgv, CURDIR MAKEFILE_LIST)

################################################################
# Pick up the "universal settings file" containing
# the list of all available software components.
################################################################

include $(WLAN_TreeBaseA)/src-rt-6.x/tools/release/WLAN.usf

################################################################
# Calculate paths to requested components.
################################################################

# This uses pattern matching to pull component paths from
# their basenames (e.g. src/wl/clm => clm).
# It also strips out component paths which don't currently exist.
# This may be required due to our "partial-source" build styles
# and the fact that linux mkdep throws an error when a directory
# specified with -I doesn't exist.
define _common-component-names-to-rel-paths
$(strip \
  $(patsubst $(WLAN_TreeBaseA)/%,%,$(wildcard $(addprefix $(WLAN_TreeBaseA)/,\
  $(sort $(foreach name,$(if $1,$1,$(WLAN_AllComponentPaths)),$(filter %/$(name),$(WLAN_AllComponentPaths))))))))
endef

# If WLAN_ComponentsInUse is unset it defaults to the full set (for now, anyway - TODO).
# It's also possible to request the full set with a literal '*'.
ifeq (,$(WLAN_ComponentsInUse))
  WLAN_ComponentsInUse		:= $(sort $(notdir $(WLAN_AllComponentPaths)))
  # $(call wlan_die,no SW component request)
else ifeq (*,$(WLAN_ComponentsInUse))
  WLAN_ComponentsInUse		:= $(sort $(notdir $(WLAN_AllComponentPaths)))
  $(call wlan_info,all SW components requested ("$(WLAN_ComponentsInUse)"))
endif
WLAN_ComponentPathsInUse	:= $(call _common-component-names-to-rel-paths,$(WLAN_ComponentsInUse))

# Test that all requested components exist.
ifneq ($(sort $(WLAN_ComponentsInUse)),$(notdir $(WLAN_ComponentPathsInUse)))
  $(call wlan_warning,bogus component request: "$(sort $(WLAN_ComponentsInUse))" != "$(notdir $(WLAN_ComponentPathsInUse))")
endif

# Generate a WLAN_ComponentBaseDir_<name> variable for each component.
$(foreach _path,$(WLAN_ComponentPathsInUse), \
  $(eval WLAN_ComponentBaseDir_$$(notdir $(_path)) := $$(WLAN_TreeBaseA)/$(_path)) \
)

# TODO - It's not clear whether the following should be exported.
# Public convenience macros based on WLAN_ComponentPathsInUse list.
export WLAN_ComponentSrcDirsR	 = $(addprefix $(WLAN_TreeBaseR)/,$(addsuffix /src,$(WLAN_ComponentPathsInUse)))
export WLAN_ComponentIncDirsR	 = $(addprefix $(WLAN_TreeBaseR)/,$(addsuffix /include,$(WLAN_ComponentPathsInUse)))
export WLAN_ComponentIncPathR	 = $(addprefix -I,$(WLAN_ComponentIncDirsR))

export WLAN_ComponentSrcDirsA	 = $(addprefix $(WLAN_TreeBaseA)/,$(addsuffix /src,$(WLAN_ComponentPathsInUse)))
export WLAN_ComponentIncDirsA	 = $(addprefix $(WLAN_TreeBaseA)/,$(addsuffix /include,$(WLAN_ComponentPathsInUse)))
export WLAN_ComponentIncPathA	 = $(addprefix -I,$(WLAN_ComponentIncDirsA))

export WLAN_ComponentSrcDirs	 = $(WLAN_ComponentSrcDirsA)
export WLAN_ComponentIncDirs	 = $(WLAN_ComponentIncDirsA)
export WLAN_ComponentIncPath	 = $(WLAN_ComponentIncPathA)

# Dump a representative sample of derived variables in debug mode.
$(call wlan_dbgv, WLAN_TreeBaseA WLAN_TreeBaseR WLAN_SrcBaseA WLAN_SrcBaseR \
    WLAN_ComponentPathsInUse WLAN_ComponentIncPath WLAN_ComponentIncPathR \
    WLAN_ComponentSrcDirs WLAN_ComponentSrcDirsR)

# Special case for Windows to reflect CL in the build log if used.
ifdef WLAN_WINDOWS_HOST
ifdef CL
$(info Info: CL=$(CL))
endif
endif

# Variables of general utility.
WLAN_Perl := perl

################################################################
# CLM function; generates a rule to run ClmCompiler iff the XML exists.
# USAGE: $(call WLAN_GenClmCompilerRule,target-dir,src-base[,flags[,ext]])
#   This macro uses GNU make's eval function to generate an
# explicit rule to generate a particular CLM data file each time
# it's called. Make variables which should be evaluated during eval
# processing get one $, those which must defer till "runtime" get $$.
#   The CLM "base flags" are the default minimal set, the "ext flags"
# are those which must be present for all release builds.
# The "clm_compiled" phony target is provided for makefiles which need
# to defer some other processing until CLM data is ready, and "clm_clean"
# and "CLM_DATA_FILES" make it easier for client makefiles to clean up CLM data.
#   The outermost conditional allows this rule to become a no-op
# in external settings where there is no XML input file while allowing
# it to turn back on automatically if an XML file is provided.
#   Vpath is used to find the XML input because this file is not allowed
# to be present in external builds. Use of vpath allows it to be "poached"
# from the internal build if necessary.
#   There are a few ways to set ClmCompiler flags: passing them as the $3
# parameter (preferred) or by overriding CLMCOMPDEFFLAGS. Additionally,
# when the make variable CLM_TYPE is defined it points to a config file
# for the compiler. The CLMCOMPEXTFLAGS variable contains "external flags"
# which must be present for all external builds. It can be forced to "" for
# debug builds.
#   The undocumented $5 parameter has been used for dongle testing
# against variant XML but its semantics are subject to change.
CLMCOMPDEFFLAGS ?= --region '\#a/0' --region '\#r/0' --full_set
CLMCOMPEXTFLAGS := --obfuscate
define WLAN_GenClmCompilerRule
$(eval\
#$$(call wlan_assert,$$(findstring clm,$$(WLAN_ComponentsInUse)),CLM component not requested)\ ## TODO - enable
.PHONY: clm_compiled clm_clean
vpath wlc_clm_data$4.c $1 $$(abspath $1)
ifneq (,$(wildcard $(addsuffix /wl/clm/private/wlc_clm_data.xml,$2 $2/../../src $2/../../../src)))
  vpath wlc_clm_data.xml $(addsuffix /wl/clm/private,$5 $2 $2/../../src $2/../../../src)
  $1/wlc_clm_data$4.c: wlc_clm_data.xml $$(if $$(CLM_TYPE),$2/wl/clm/types/$$(CLM_TYPE).clm) ; \
    $$(strip $$(abspath $$(<D)/../../../tools/build/ClmCompiler) \
      $$(if $$(CLM_TYPE),--config_file $$(abspath $$(<D)/../types/$$(CLM_TYPE).clm) $3,$$(if $3,$3,$$(CLMCOMPDEFFLAGS))) \
      $(CLMCOMPEXTFLAGS) $$< $$@)
  clm_compiled: $1/wlc_clm_data$4.c
  clm_clean:: ; $$(RM) $1/wlc_clm_data$4.c
  CLM_DATA_FILES += $1/wlc_clm_data$4.c
else
  clm_compiled:
  clm_clean::
endif
)
endef

# Backward compatibility.
GenClmCompilerRule = $(WLAN_GenClmCompilerRule)

################################################################

endif	# _WLAN_COMMON_MK
