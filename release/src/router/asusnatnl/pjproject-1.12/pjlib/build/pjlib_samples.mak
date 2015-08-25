
OUTDIR=.\output\pjlib-samples-i386-win32-$(VC)-$(MODE)

SRCDIR=../src/pjlib-samples

SAMPLES=$(OUTDIR)/except.exe \
	    $(OUTDIR)/log.exe \
		$(OUTDIR)/list.exe \

!IF "$(MODE)" == "debug"
MODE_CFLAGS=/MTd
!ELSE
MODE_CFLAGS=/MT
!ENDIF

CFLAGS=/nologo /W4 $(MODE_CFLAGS) /DPJ_WIN32=1 /DPJ_M_I386=1 /I../include

PJLIB=../lib/pjlib-i386-win32-$(VC)-$(MODE).lib

DEPEND=$(PJLIB)
LIBS=netapi32.lib mswsock.lib ws2_32.lib ole32.lib advapi32.lib
CL=cl.exe

all: "$(OUTDIR)" $(SAMPLES)

$(SAMPLES): "$(SRCDIR)/$(@B).c" $(DEPEND)
		$(CL) /Fe$@ \
		/Fo$(@R).obj \
		$(CFLAGS) \
		$** $(LIBS)

"$(OUTDIR)" :
		@IF NOT EXIST "$(OUTDIR)" MKDIR "$(OUTDIR)"

clean :
		@IF EXIST "$(OUTDIR)" DEL /Q "$(OUTDIR)\*.*" && RMDIR "$(OUTDIR)"
