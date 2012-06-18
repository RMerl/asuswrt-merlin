[SUBSYSTEM::ZLIB]
CFLAGS = -I$(zlibsrcdir)

ZLIB_OBJ_FILES = \
		$(zlibsrcdir)/adler32.o \
		$(zlibsrcdir)/compress.o \
		$(zlibsrcdir)/crc32.o \
		$(zlibsrcdir)/gzio.o \
		$(zlibsrcdir)/uncompr.o \
		$(zlibsrcdir)/deflate.o \
		$(zlibsrcdir)/trees.o \
		$(zlibsrcdir)/zutil.o \
		$(zlibsrcdir)/inflate.o \
		$(zlibsrcdir)/infback.o \
		$(zlibsrcdir)/inftrees.o \
		$(zlibsrcdir)/inffast.o
