# @configure_input@

# Define the desired sound device backend
# Valid values are:
#   - pa_unix:	    	PortAudio on Unix (OSS or ALSA)
#   - pa_darwinos:  	PortAudio on MacOSX (CoreAudio)
#   - pa_old_darwinos:  PortAudio on MacOSX (old CoreAudio, for OSX 10.2)
#   - pa_win32:	    	PortAudio on Win32 (WMME)
#
# There are other values below, but these are handled by PJMEDIA's Makefile
#   - ds:	    	Win32 DirectSound (dsound.c)
#   - null:	    	Null sound device (nullsound.c)
AC_PJMEDIA_SND=pa_unix

# For Unix, specify if ALSA should be supported
AC_PA_USE_ALSA=0

#
# PortAudio on Unix
#
ifeq ($(AC_PJMEDIA_SND),pa_unix)
# Host APIs and utils
export PORTAUDIO_OBJS += pa_unix_hostapis.o pa_unix_util.o

# Include ALSA?
ifeq ($(AC_PA_USE_ALSA),1)
export CFLAGS += -DPA_USE_ALSA=1
export PORTAUDIO_OBJS += pa_linux_alsa.o
endif

export CFLAGS += -DPA_USE_OSS=1 -DHAVE_SYS_SOUNDCARD_H
export PORTAUDIO_OBJS += pa_unix_oss.o
endif

