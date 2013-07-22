ifdef _RELPATH_MK_
$(if $D,$(info =-= Avoiding redundant include ($(MAKEFILE_LIST))))
else
unexport _RELPATH_MK_	# in case of make -e
_RELPATH_MK_ := 1

# Protection against process recursion when this file is
# included by setting MAKEFILES.
ifneq (,$(filter %$(notdir $(lastword $(MAKEFILE_LIST))),$(MAKEFILES)))
MAKEFILES := $(filter-out %$(notdir $(lastword $(MAKEFILE_LIST))),$(MAKEFILES))
ifndef MAKEFILES
unexport MAKEFILES
endif
endif

# Usage: $(call relpath,[<from-dir>,]<to-dir>)
# Returns the relative path from <from-dir> to <to-dir>; <from-dir>
# may be elided in which case it defaults to $(CURDIR).

_rp_space :=
_rp_space +=
_rp_uname_s := $(shell uname -s)

# Utility functions.
_rp_compose = $(subst ${_rp_space},$(strip $1),$(strip $2))
_rp_endlist = $(wordlist $1,$(words $2),$2)
_rp_canonpath = $(if $(findstring CYGWIN,$(_rp_uname_s)),$(shell cygpath -a -u $1),$(abspath $1))

# ----relpath(): Self-recursive function which compares the first element
#                of two given paths, then calls itself with the next two
#                elements, and so on, until a difference is found.  At each
#                step, if the first element of both paths matches, that
#                element is produced.
----relpath = $(if $(filter $(firstword $1),$(firstword $2)), \
                $(firstword $1) \
                $(call $0,$(call _rp_endlist,2,$1),$(call _rp_endlist,2,$2)) \
               )
# ---relpath():  This function removes $1 from the front of both $2 and
#                $3 (removes common path prefix) and generates a relative
#                path between the locations given by $2 and $3, by replacing
#                each remaining element of $2 (after common prefix removal)
#                with '..', then appending the remainder of $3 (after common
#                prefix removal) to the string of '..'s
---relpath  = $(foreach e,$(subst /, ,$(patsubst $(if $1,/)$1/%,%,$2)),..) \
              $(if $3,$(patsubst $(if $1,/)$1/%,%,$3))
# --relpath():   This function runs the output of ----relpath() through
#                ---relpath(), and turns the result into an actual relative
#                path string, separated by '/'.
--relpath   = $(call _rp_compose,/, \
                $(call -$0,$(call _rp_compose,/,$(call --$0,$3,$4)),$1,$2) \
               )
# -relpath():    This function makes a determination about the two given
#                paths -- does one strictly prefix the other?  If so, this
#                function produces a relative path between the two inputs,
#                without calling --relpath() and taking the "long road".
#                If $1 prefixes $2, the result is the remainder of $2 after
#                removing $1.  If $2 prefixes $1, the result is the remainder
#                of $1 after removing $2, but with each element in that
#                remainder converted to '..'.
-relpath    = $(if $(filter $1,$2),., \
                $(if $(filter $1/%,$2), \
                 $(patsubst $1/%,%,$2), \
                 $(if $(filter $2/%,$1), \
                   $(call _rp_compose,/, \
                     $(foreach e,$(subst /, ,$(patsubst $2/%,%,$1)),..) \
                    ), \
                   $(call -$0,$1,$2,$(subst /, ,$1),$(subst /, ,$2)) \
                  ) \
                ) \
              )

# relpath():     This function loops over each element in $2, calculating
#                the relative path from $1 to each element of $2.
relpath     = $(if $1,,$(error Error: missing first parameter to $0))$(strip \
                $(if $2, \
                  $(foreach d,$2,$(call -$0,$(call _rp_canonpath,$1),$(call _rp_canonpath,$d))), \
                  $(foreach d,$1,$(call -$0,$(call _rp_canonpath,${CURDIR}),$(call _rp_canonpath,$d))) \
                 ) \
               )

endif #_RELPATH_MK_
