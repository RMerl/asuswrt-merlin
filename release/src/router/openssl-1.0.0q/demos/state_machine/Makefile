CFLAGS=-I../../include -Wall -Werror -g

all: state_machine

state_machine: state_machine.o
	$(CC) -o state_machine state_machine.o -L../.. -lssl-1.0.0q -lcrypto-1.0.0q

test: state_machine
	./state_machine 10000 ../../apps/server.pem ../../apps/server.pem
