INCLUDES=-I.
CFLAGS=$(INCLUDES) 
MINGW_CC = i586-mingw32msvc-cc
CC = $(MINGW_CC)

all: testmailslot.exe

clean:
	rm -f *~ *.obj testmailslot.exe 

.SUFFIXES: .obj .exe .c

testmailslot.exe: testmailslot.c

.c.exe: 
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
