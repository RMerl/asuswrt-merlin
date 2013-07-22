
ifeq ($(origin CC),default)
CC = gcc
endif

CFLAGS := $(CFLAGS) -Wall -g -O2 -I../../lib

LDFLAGS := $(LDFLAGS)

TARGETS = stress_1 \
	stress_2 \
	stress_3 \
	pdfrun \
	rndwrite00 \
	fwrite00 \
	rmdir00 \
	rndrm00 \
	rndrm99 \
	gcd_hupper

all: $(TARGETS)

$(TARGETS): ../../lib/tests.o

../lib/tests.o: ../../lib/tests.h

clean:
	rm -f *.o $(TARGETS) run_pdf_test_file

tests: all
	./stress_1 -e
	./stress_2
	./stress_3 -e
	./pdfrun
	./rndwrite00 -e
	./fwrite00
	./rmdir00
	./rndrm00
	./rndrm99
	./gcd_hupper
