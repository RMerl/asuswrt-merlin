etags:
	etags `find $(srcdir) -name "*.[ch]"`

ctags:
	ctags `find $(srcdir) -name "*.[ch]"`

.SUFFIXES: .1 .1.xml .3 .3.xml .xml .html .c .o

.c.o:
	@echo Compiling $*.c
	@mkdir -p `dirname $@`
	@$(CC) $(CFLAGS) $(PICFLAG) -c $< -o $@

.c.po:
	@echo Compiling $*.c
	@mkdir -p `dirname $@`
	@$(CC) -fPIC $(CFLAGS) -c $< -o $@

showflags::
	@echo 'ldb will be compiled with flags:'
	@echo '  CFLAGS = $(CFLAGS)'
	@echo '  SHLD_FLAGS = $(SHLD_FLAGS)'
	@echo '  LIBS = $(LIBS)'

distclean::
	rm -f *~ */*~
