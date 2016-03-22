CC=cc
CFLAGS= -g -I../../include -Wall
LIBS=  -L../.. -lcrypto-1.0.0q
EXAMPLES=sign

all: $(EXAMPLES) 

sign: sign.o
	$(CC) -o sign sign.o $(LIBS)

clean:	
	rm -f $(EXAMPLES) *.o

test: all
	./sign
