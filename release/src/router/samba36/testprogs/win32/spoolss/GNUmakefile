CFLAGS=-I../../../
MINGW_CC = i586-mingw32msvc-cc
CC = $(MINGW_CC)

LIBS=-lwinspool

all: testspoolss.exe

clean:
	rm -vf *.exe tags

ctags:
	ctags `find . -name "*.[ch]" | grep -v include/proto.h`
	ctags --c-kinds=-p -a `find /usr/i686-pc-mingw32/sys-root/mingw/include -name "*.[ch]" | grep -v /CVS/`

proto:
	mkproto.pl printlib.c --private=printlib_proto.h --public=printlib_proto_pub.h --srcdir=. --builddir=.
	mkproto.pl torture.c --private=torture_proto.h --public=torture_proto_pub.h --srcdir=. --builddir=.

testspoolss.exe: testspoolss.c printlib.c torture.c error.c
	@echo Compiling testspoolss.exe
	@$(CC) $(CFLAGS) $(LDFLAGS) testspoolss.c printlib.c torture.c error.c $(LIBS) -o testspoolss.exe

