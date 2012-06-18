INCLUDES=-I 
CFLAGS=$(INCLUDES) -Zi -nologo

all: testmailslot.exe

clean:
	del *~ *.obj testmailslot.exe 

testmailslot.exe: testmailslot.obj 
	$(CC) $(CFLAGS) -o testmailslot.exe testmailslot.obj $(LIBS)
