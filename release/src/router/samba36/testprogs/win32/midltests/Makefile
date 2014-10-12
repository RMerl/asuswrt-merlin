
all:
	@echo "nmake targets:"
	@echo "   clean"
	@echo "   simple"
	@echo "   tcp"

clean:
	@call nmake /f Makefile.simple /A /NOLOGO clean
	@call nmake /f Makefile.tcp /A /NOLOGO clean

simple:
	@call nmake /f Makefile.simple /A /NOLOGO all

tcp:
	@call nmake /f Makefile.tcp /A /NOLOGO all
