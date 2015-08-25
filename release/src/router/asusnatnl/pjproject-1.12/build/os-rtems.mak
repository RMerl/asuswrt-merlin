#
# Global OS specific configurations for RTEMS OS.
#
# Thanks Zetron, Inc and Phil Torre <ptorre@zetron.com> for donating PJLIB
# port to RTEMS.
#
export RTEMS_DEBUG := -ggdb3 -DRTEMS_DEBUG -DDEBUG -qrtems_debug 

export OS_CFLAGS   := $(CC_DEF)PJ_RTEMS=1 \
	-B$(RTEMS_LIBRARY_PATH)/lib/ -specs bsp_specs -qrtems

export OS_CXXFLAGS := 

export OS_LDFLAGS  := -B$(RTEMS_LIBRARY_PATH)/lib/ -specs bsp_specs -qrtems  -lm

export OS_SOURCES  := 

