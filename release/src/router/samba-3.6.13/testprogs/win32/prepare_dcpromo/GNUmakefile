INCLUDES=-I.
CFLAGS=$(INCLUDES)
LIBS=-ladvapi32

PREPARE_DCPROMO = prepare_dcpromo.exe

all: $(PREPARE_DCPROMO)

MINGW_CC = i586-mingw32msvc-cc
CC = $(MINGW_CC)

.SUFFIXES: .c .obj .exe

.c.obj:
	$(CC) $(CFLAGS) -c $< -o $@

.obj.exe:
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *~ *.obj *.exe
