# PJMEDIA features exclusion
export CFLAGS += 

# Define the desired sound device backend
# Valid values are:
#   - pa_unix:	    	PortAudio on Unix (OSS or ALSA)
#   - pa_darwinos:  	PortAudio on MacOSX (CoreAudio)
#   - pa_old_darwinos:  PortAudio on MacOSX (old CoreAudio, for OSX 10.2)
#   - pa_win32:	    	PortAudio on Win32 (WMME)
#   - ds:	    	Win32 DirectSound (dsound.c)
#   - null:	    	Null sound device (nullsound.c)
AC_PJMEDIA_SND=pa_win32

#
# Codecs
#
AC_NO_G711_CODEC=0
AC_NO_L16_CODEC=0
AC_NO_GSM_CODEC=0
AC_NO_SPEEX_CODEC=0
AC_NO_ILBC_CODEC=0
AC_NO_G722_CODEC=0
AC_NO_G7221_CODEC=0

export CODEC_OBJS=

ifeq ($(AC_NO_G711_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_G711_CODEC=0
else
export CODEC_OBJS +=
endif

ifeq ($(AC_NO_L16_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_L16_CODEC=0
else
export CODEC_OBJS += l16.o
endif

ifeq ($(AC_NO_GSM_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_GSM_CODEC=0
else
export CODEC_OBJS += gsm.o
endif

ifeq ($(AC_NO_SPEEX_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_SPEEX_CODEC=0
else
export CFLAGS += -I$(THIRD_PARTY)/build/speex -I$(THIRD_PARTY)/speex/include
export CODEC_OBJS += speex_codec.o

#ifeq (@ac_no_speex_aec@,1)
ifeq (0,1)
export PJMEDIA_OBJS += echo_speex.o
endif

endif

ifeq ($(AC_NO_ILBC_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_ILBC_CODEC=0
else
export CODEC_OBJS += ilbc.o
endif

ifeq ($(AC_NO_G722_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_G722_CODEC=0
else
export CODEC_OBJS += g722.o g722/g722_enc.o g722/g722_dec.o
endif

ifeq ($(AC_NO_G7221_CODEC),1)
export CFLAGS += -DPJMEDIA_HAS_G7221_CODEC=0
else
export CODEC_OBJS += g7221.o
export G7221_CFLAGS += -I$(THIRD_PARTY)
endif


#
# PortAudio
#
ifneq ($(findstring pa,$(AC_PJMEDIA_SND)),)
export CFLAGS += -I$(THIRD_PARTY)/build/portaudio -I$(THIRD_PARTY)/portaudio/include -DPJMEDIA_SOUND_IMPLEMENTATION=PJMEDIA_SOUND_PORTAUDIO_SOUND
export SOUND_OBJS = pasound.o
endif

#
# Win32 DirectSound
#
ifeq ($(AC_PJMEDIA_SND),ds)
export SOUND_OBJS = dsound.o
export CFLAGS += -DPJMEDIA_SOUND_IMPLEMENTATION=PJMEDIA_SOUND_WIN32_DIRECT_SOUND
endif

#
# Last resort, null sound device
#
ifeq ($(AC_PJMEDIA_SND),null)
export SOUND_OBJS = nullsound.o
export CFLAGS += -DPJMEDIA_SOUND_IMPLEMENTATION=PJMEDIA_SOUND_NULL_SOUND
endif


