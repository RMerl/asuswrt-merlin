# third_party/build/portaudio/os-auto.mak.  Generated from os-auto.mak.in by configure.

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

# For Unix, specify if OSS should be supported
AC_PA_USE_OSS=1

# Additional PortAudio CFLAGS are in  -DHAVE_SYS_SOUNDCARD_H -DHAVE_LINUX_SOUNDCARD_H -DPA_LITTLE_ENDIAN


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

export CFLAGS +=  -DHAVE_SYS_SOUNDCARD_H -DHAVE_LINUX_SOUNDCARD_H -DPA_LITTLE_ENDIAN
endif

# Include OSS?
ifeq ($(AC_PA_USE_OSS),1)
export CFLAGS += -DPA_USE_OSS=1
export PORTAUDIO_OBJS += pa_unix_oss.o
endif

#
# PortAudio on MacOS X (using current PortAudio)
#
ifeq ($(AC_PJMEDIA_SND),pa_darwinos)
export PORTAUDIO_OBJS +=pa_mac_hostapis.o \
			pa_unix_util.o \
			pa_mac_core.o \
			pa_mac_core_blocking.o \
			pa_mac_core_utilities.o \
			pa_ringbuffer.o
export CFLAGS += -DPA_USE_COREAUDIO=1
export CFLAGS +=  -DHAVE_SYS_SOUNDCARD_H -DHAVE_LINUX_SOUNDCARD_H -DPA_LITTLE_ENDIAN
endif

#
# PortAudio on MacOS X (using old PortAudio, for MacOS X 10.2.x)
#
ifeq ($(AC_PJMEDIA_SND),pa_old_darwinos)
export PORTAUDIO_OBJS +=pa_mac_hostapis.o \
			pa_unix_util.o \
			pa_mac_core_old.o 
export CFLAGS += -DPA_USE_COREAUDIO=1
export CFLAGS +=  -DHAVE_SYS_SOUNDCARD_H -DHAVE_LINUX_SOUNDCARD_H -DPA_LITTLE_ENDIAN
endif

#
#
# PortAudio on Win32 (WMME)
#
ifeq ($(AC_PJMEDIA_SND),pa_win32)
export PORTAUDIO_OBJS += pa_win_hostapis.o pa_win_util.o \
		       pa_win_wmme.o pa_win_waveformat.o
export CFLAGS += -DPA_NO_ASIO -DPA_NO_DS
endif
