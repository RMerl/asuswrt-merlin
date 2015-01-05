CFLAGS += -O2 -I$(TOP)/libnl-tiny-0.1/include -I$(KERNEL_HEADER_DIR) 
LIBS=-L$(TOP)/libnl-tiny-0.1 -lnl-tiny

all: swconfig

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

swconfig: cli.o swlib.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f *.o swconfig
