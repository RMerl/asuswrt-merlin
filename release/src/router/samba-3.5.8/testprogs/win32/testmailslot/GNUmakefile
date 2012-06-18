INCLUDES=-I.
CFLAGS=$(INCLUDES) 
CC=i586-mingw32msvc-gcc

all: testmailslot.exe

clean:
	rm -f *~ *.obj testmailslot.exe 

.SUFFIXES: .obj .exe .c

testmailslot.exe: testmailslot.c

.c.exe: 
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
