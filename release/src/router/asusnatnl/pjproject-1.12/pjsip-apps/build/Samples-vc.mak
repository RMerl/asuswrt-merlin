
LIBEXT = .lib

TARGET = i386-win32-vc$(VC_VER)-$(BUILD_MODE)

!if "$(BUILD_MODE)" == "debug"
BUILD_FLAGS = /MTd /Od /Zi /W4
!elseif "$(BUILD_MODE)" == "debug-static"
BUILD_FLAGS = /MTd /Od /Zi /W4
!elseif "$(BUILD_MODE)" == "debug-dynamic"
BUILD_FLAGS = /MDd /Od /Zi /W4
!elseif "$(BUILD_MODE)" == "release-static"
BUILD_FLAGS = /Ox /MT /DNDEBUG /W4
!else
BUILD_FLAGS = /Ox /MD /DNDEBUG /W4
!endif

PJLIB_LIB = ..\..\pjlib\lib\pjlib-$(TARGET)$(LIBEXT)
PJLIB_UTIL_LIB = ..\..\pjlib-util\lib\pjlib-util-$(TARGET)$(LIBEXT)
PJNATH_LIB = ..\..\pjnath\lib\pjnath-$(TARGET)$(LIBEXT)
PJMEDIA_LIB = ..\..\pjmedia\lib\pjmedia-$(TARGET)$(LIBEXT)
PJMEDIA_CODEC_LIB = ..\..\pjmedia\lib\pjmedia-codec-$(TARGET)$(LIBEXT)
PJMEDIA_AUDIODEV_LIB = ..\..\pjmedia\lib\pjmedia-audiodev-$(TARGET)$(LIBEXT)
PJSIP_LIB = ..\..\pjsip\lib\pjsip-core-$(TARGET)$(LIBEXT)
PJSIP_UA_LIB = ..\..\pjsip\lib\pjsip-ua-$(TARGET)$(LIBEXT)
PJSIP_SIMPLE_LIB = ..\..\pjsip\lib\pjsip-simple-$(TARGET)$(LIBEXT)
PJSUA_LIB_LIB = ..\..\pjsip\lib\pjsua-lib-$(TARGET)$(LIBEXT)

GSM_LIB = ..\..\third_party\lib\libgsmcodec-$(TARGET)$(LIBEXT)
ILBC_LIB = ..\..\third_party\lib\libilbccodec-$(TARGET)$(LIBEXT)
PORTAUDIO_LIB = ..\..\third_party\lib\libportaudio-$(TARGET)$(LIBEXT)
RESAMPLE_LIB = ..\..\third_party\lib\libresample-$(TARGET)$(LIBEXT)
SPEEX_LIB = ..\..\third_party\lib\libspeex-$(TARGET)$(LIBEXT)
SRTP_LIB = ..\..\third_party\lib\libsrtp-$(TARGET)$(LIBEXT)
G7221_LIB = ..\..\third_party\lib\libg7221codec-$(TARGET)$(LIBEXT)

THIRD_PARTY_LIBS = $(GSM_LIB) $(ILBC_LIB) $(PORTAUDIO_LIB) $(RESAMPLE_LIB) \
				   $(SPEEX_LIB) $(SRTP_LIB) $(G7221_LIB)

LIBS = $(PJSUA_LIB_LIB) $(PJSIP_UA_LIB) $(PJSIP_SIMPLE_LIB) \
	  $(PJSIP_LIB) $(PJMEDIA_CODEC_LIB) $(PJMEDIA_AUDIODEV_LIB) \
	  $(PJMEDIA_LIB) $(PJNATH_LIB) $(PJLIB_UTIL_LIB) $(PJLIB_LIB) \
	  $(THIRD_PARTY_LIBS)

CFLAGS 	= /DPJ_WIN32=1 /DPJ_M_I386=1 \
	  $(BUILD_FLAGS) \
	  -I..\..\pjsip\include \
	  -I..\..\pjlib\include \
	  -I..\..\pjlib-util\include \
	  -I..\..\pjmedia\include \
	  -I..\..\pjnath/include
LDFLAGS = $(BUILD_FLAGS) $(LIBS) \
	  Iphlpapi.lib ole32.lib user32.lib dsound.lib dxguid.lib netapi32.lib \
	  mswsock.lib ws2_32.lib gdi32.lib advapi32.lib

SRCDIR = ..\src\samples
OBJDIR = .\output\samples-$(TARGET)
BINDIR = ..\bin\samples\$(TARGET)


SAMPLES = $(BINDIR)\auddemo.exe \
	  $(BINDIR)\aectest.exe \
	  $(BINDIR)\confsample.exe \
	  $(BINDIR)\confbench.exe \
	  $(BINDIR)\encdec.exe \
	  $(BINDIR)\httpdemo.exe \
	  $(BINDIR)\icedemo.exe \
	  $(BINDIR)\jbsim.exe \
	  $(BINDIR)\latency.exe \
	  $(BINDIR)\level.exe \
	  $(BINDIR)\mix.exe \
	  $(BINDIR)\pcaputil.exe\
	  $(BINDIR)\pjsip-perf.exe \
	  $(BINDIR)\playfile.exe \
	  $(BINDIR)\playsine.exe\
	  $(BINDIR)\recfile.exe  \
	  $(BINDIR)\resampleplay.exe \
	  $(BINDIR)\simpleua.exe \
	  $(BINDIR)\simple_pjsua.exe \
	  $(BINDIR)\siprtp.exe \
	  $(BINDIR)\sipstateless.exe \
	  $(BINDIR)\stateful_proxy.exe \
	  $(BINDIR)\stateless_proxy.exe \
	  $(BINDIR)\stereotest.exe \
	  $(BINDIR)\streamutil.exe \
	  $(BINDIR)\strerror.exe \
	  $(BINDIR)\tonegen.exe


all: $(BINDIR) $(OBJDIR) $(SAMPLES)

$(SAMPLES): $(SRCDIR)\$(@B).c $(LIBS) $(SRCDIR)\util.h Samples-vc.mak
	cl -nologo -c $(SRCDIR)\$(@B).c /Fo$(OBJDIR)\$(@B).obj $(CFLAGS) 
	cl /nologo $(OBJDIR)\$(@B).obj /Fe$@ /Fm$(OBJDIR)\$(@B).map $(LDFLAGS)
	@rem the following two lines is just for cleaning up the 'bin' directory
	if exist $(BINDIR)\*.ilk del /Q $(BINDIR)\*.ilk
	if exist $(BINDIR)\*.pdb del /Q $(BINDIR)\*.pdb

$(BINDIR):
	if not exist $(BINDIR) mkdir $(BINDIR)

$(OBJDIR):
	if not exist $(OBJDIR) mkdir $(OBJDIR)

clean:
	echo Cleaning up samples...
	if exist $(BINDIR) del /Q $(BINDIR)\*
	if exist $(BINDIR) rmdir $(BINDIR)
	if exist $(OBJDIR) del /Q $(OBJDIR)\*.*

