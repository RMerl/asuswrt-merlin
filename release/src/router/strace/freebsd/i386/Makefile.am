# Automake input for freebsd/i386/ subdirectory of strace.

noinst_HEADERS = errnoent.h ioctlent.h signalent.h syscallent.h syscall.h
BUILT_SOURCES = $(noinst_HEADERS)


ioctlent.raw: $(srcdir)/../ioctlent.sh
	$(SHELL) $(srcdir)/../ioctlent.sh $(includedir) >$@

ioctlent.h: ioctlent.raw ioctlsort
	./ioctlsort >$@

ioctlsort.o: $(srcdir)/../../ioctlsort.c

ioctlsort.o: ioctlent.raw

errnoent.h: $(srcdir)/../../errnoent.sh $(includedir)/errno.h
	$(SHELL) $(srcdir)/../../errnoent.sh $(includedir)/errno.h >$@

signalent.h: $(srcdir)/../../signalent.sh $(includedir)/sys/signal.h
	$(SHELL) $(srcdir)/../../signalent.sh $(includedir)/sys/signal.h >$@

syscallent.h: $(srcdir)/../syscalls.pl $(srcdir)/../syscalls.cat $(srcdir)/../syscalls.print /usr/src/sys/kern/syscalls.master
	$(PERL) $(srcdir)/../syscalls.pl /usr/src/sys/kern/syscalls.master $(srcdir)/../syscalls.print $(srcdir)/../syscalls.cat >$@
