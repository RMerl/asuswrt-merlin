# pjmedia/build/os-auto.mak.  Generated from os-auto.mak.in by configure.

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
#   - external:		Link with no sounddev (app will provide)
AC_PJMEDIA_SND=pa_unix

# For Unix, specify if ALSA should be supported
AC_PA_USE_ALSA=0

# Additional PortAudio CFLAGS are in  -DHAVE_SYS_SOUNDCARD_H -DHAVE_LINUX_SOUNDCARD_H -DPA_LITTLE_ENDIAN

#
# Codecs
#
AC_NO_G711_CODEC=
AC_NO_L16_CODEC=
AC_NO_GSM_CODEC=
AC_NO_SPEEX_CODEC=
AC_NO_ILBC_CODEC=
AC_NO_G722_CODEC=
AC_NO_G7221_CODEC=
AC_NO_OPENCORE_AMRNB=1

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

ifneq (,1)
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

ifeq ($(AC_NO_OPENCORE_AMRNB),1)
export CFLAGS += -DPJMEDIA_HAS_OPENCORE_AMRNB_CODEC=0
else
export CODEC_OBJS += opencore_amrnb.o
endif


#
# PortAudio
#
ifneq ($(findstring pa,$(AC_PJMEDIA_SND)),)
ifeq (0,1)
# External PA
export CFLAGS += -DPJMEDIA_AUDIO_DEV_HAS_PORTAUDIO=1
else
# Our PA in third_party
export CFLAGS += -I$(THIRD_PARTY)/build/portaudio -I$(THIRD_PARTY)/portaudio/include -DPJMEDIA_AUDIO_DEV_HAS_PORTAUDIO=1
endif
endif

#
# Windows specific
#
ifneq ($(findstring win32,$(AC_PJMEDIA_SND)),)
export CFLAGS += -DPJMEDIA_AUDIO_DEV_HAS_WMME=1
else
export CFLAGS += -DPJMEDIA_AUDIO_DEV_HAS_WMME=0
endif

#
# Null sound device
#
ifeq ($(AC_PJMEDIA_SND),null)
export CFLAGS += -DPJMEDIA_AUDIO_DEV_HAS_PORTAUDIO=0 -DPJMEDIA_AUDIO_DEV_HAS_WMME=0
endif

#
# External sound device
#
ifeq ($(AC_PJMEDIA_SND),external)
export CFLAGS += -DPJMEDIA_AUDIO_DEV_HAS_PORTAUDIO=0 -DPJMEDIA_AUDIO_DEV_HAS_WMME=0
endif


