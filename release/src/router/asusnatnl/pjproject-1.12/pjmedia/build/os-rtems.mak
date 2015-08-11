#
# PJMEDIA OS specific configuration for RTEMS OS target.
#

export CFLAGS += -DPJMEDIA_SOUND_IMPLEMENTATION=PJMEDIA_SOUND_NULL_SOUND
export PJMEDIA_OBJS += nullsound.o
export SOUND_OBJS = $(NULLSOUND_OBJS)

