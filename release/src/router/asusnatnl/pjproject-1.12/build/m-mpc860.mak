#
# PowerPC MPC860 specific.
# It's a PowerPC without floating point support.
#
export M_CFLAGS := $(CC_DEF)PJ_M_POWERPC=1 $(CC_DEF)PJ_HAS_FLOATING_POINT=0 -mcpu=860
export M_CXXFLAGS :=
export M_LDFLAGS := -mcpu=860
export M_SOURCES :=

