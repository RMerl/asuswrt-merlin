CC=gcc
# uncomment following for performance
CCFLAGS=-O3 -Wall -fomit-frame-pointer

# uncomment following for debugging. Uncomment either this or the one above. Not both.
# CCFLAGS=-Wall -g


all: JitterTest plotJittervsFill

JitterTest: JitterTest.c Makefile
	 gcc $(CCFLAGS) -lm JitterTest.c -o JitterTest

plotJittervsFill: plotJittervsFill.c Makefile
	 gcc $(CCFLAGS) plotJittervsFill.c -o plotJittervsFill

clean:
	 rm -rf *~
	 rm -rf core
	 rm -rf *.o
	 rm -rf JitterTest


dep:
	makedepend -I./ *.c
# DO NOT DELETE

JitterTest.o: /usr/include/stdio.h /usr/include/features.h
JitterTest.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
JitterTest.o: /usr/lib/gcc-lib/i386-redhat-linux/egcs-2.91.66/include/stddef.h
JitterTest.o: /usr/lib/gcc-lib/i386-redhat-linux/egcs-2.91.66/include/stdarg.h
JitterTest.o: /usr/include/bits/types.h /usr/include/libio.h
JitterTest.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
JitterTest.o: /usr/include/string.h /usr/include/stdlib.h
JitterTest.o: /usr/include/sys/types.h /usr/include/time.h
JitterTest.o: /usr/include/endian.h /usr/include/bits/endian.h
JitterTest.o: /usr/include/sys/select.h /usr/include/bits/select.h
JitterTest.o: /usr/include/bits/sigset.h /usr/include/sys/sysmacros.h
JitterTest.o: /usr/include/alloca.h /usr/include/sys/time.h
JitterTest.o: /usr/include/bits/time.h /usr/include/signal.h
JitterTest.o: /usr/include/bits/signum.h /usr/include/bits/siginfo.h
JitterTest.o: /usr/include/bits/sigaction.h /usr/include/bits/sigcontext.h
JitterTest.o: /usr/include/asm/sigcontext.h /usr/include/bits/sigstack.h
JitterTest.o: /usr/include/sched.h /usr/include/bits/sched.h
JitterTest.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
JitterTest.o: /usr/include/bits/confname.h /usr/include/getopt.h
