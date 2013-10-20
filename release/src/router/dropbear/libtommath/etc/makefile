CFLAGS += -Wall -W -Wshadow -O3 -fomit-frame-pointer -funroll-loops -I../

# default lib name (requires install with root)
# LIBNAME=-ltommath

# libname when you can't install the lib with install
LIBNAME=../libtommath.a

#provable primes
pprime: pprime.o
	$(CC) pprime.o $(LIBNAME) -o pprime

# portable [well requires clock()] tuning app
tune: tune.o
	$(CC) tune.o $(LIBNAME) -o tune
	
# same app but using RDTSC for higher precision [requires 80586+], coff based gcc installs [e.g. ming, cygwin, djgpp]
tune86: tune.c
	nasm -f coff timer.asm
	$(CC) -DX86_TIMER $(CFLAGS) tune.c timer.o  $(LIBNAME) -o tune86
	
# for cygwin
tune86c: tune.c
	nasm -f gnuwin32 timer.asm
	$(CC) -DX86_TIMER $(CFLAGS) tune.c timer.o  $(LIBNAME) -o tune86

#make tune86 for linux or any ELF format
tune86l: tune.c
	nasm -f elf -DUSE_ELF timer.asm
	$(CC) -DX86_TIMER $(CFLAGS) tune.c timer.o $(LIBNAME) -o tune86l
        
# spits out mersenne primes
mersenne: mersenne.o
	$(CC) mersenne.o $(LIBNAME) -o mersenne

# fines DR safe primes for the given config
drprime: drprime.o
	$(CC) drprime.o $(LIBNAME) -o drprime
	
# fines 2k safe primes for the given config
2kprime: 2kprime.o
	$(CC) 2kprime.o $(LIBNAME) -o 2kprime

mont: mont.o
	$(CC) mont.o $(LIBNAME) -o mont

        
clean:
	rm -f *.log *.o *.obj *.exe pprime tune mersenne drprime tune86 tune86l mont 2kprime pprime.dat \
         *.da *.dyn *.dpi *~
