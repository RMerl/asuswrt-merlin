CC=gcc
INCLUDES= -I`pwd` -I../../../source/ -I../../../source/include -I../../../source/ubiqx

DEFS= -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

CFLAGS= -g -Wall -ansi $(INCLUDES) 

OBJ= util.o mgr_group.o mgr_user.o

LDFLAGS=-L. -L../../bin/ 
LIBS=../../../source/bin/libmsrpc.so

all: cacusermgr

cacusermgr: cacusermgr.o $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(OBJ) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o cacusermgr
