.SUFFIXES: .i _wrap.c

showflags::
	@echo 'libtevent will be compiled with flags:'
	@echo '  CFLAGS = $(CFLAGS)'
	@echo '  CPPFLAGS = $(CPPFLAGS)'
	@echo '  LDFLAGS = $(LDFLAGS)'
	@echo '  LIBS = $(LIBS)'

.SUFFIXES: .c .o

.c.o:
	@echo Compiling $*.c
	@mkdir -p `dirname $@`
	@$(CC) $(PICFLAG) $(CFLAGS) $(ABI_CHECK) -c $< -o $@

distclean::
	rm -f *~ */*~
