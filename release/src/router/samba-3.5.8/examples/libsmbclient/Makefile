#
CC = gcc

SAMBA_INCL  = -I/usr/local/samba/include
EXTLIB_INCL = -I/usr/include/gtk-1.2 \
	      -I/usr/include/glib-1.2 \
	      -I/usr/lib/glib/include
EXTLIB_INCL = `gtk-config --cflags`

DEFS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS = -O0 -g $(SAMBA_INCL) $(EXTLIB_INCL) $(DEFS)

LDFLAGS = -L/usr/local/samba/lib \
	  -lldap -lkrb5 -lgssapi_krb5
#LIBSMBCLIENT = /usr/local/samba/lib/libsmbclient.so
LIBSMBCLIENT = -lwbclient -lsmbclient -ltalloc -ltdb -ldl -lresolv

TESTS=	testsmbc \
	testacl \
	testacl2 \
	testacl3 \
	testbrowse \
	testbrowse2 \
	teststat \
	teststat2 \
	teststat3 \
	teststatvfs \
	testfstatvfs \
	testtruncate \
	testchmod \
	testutime \
	testread \
	testwrite

#	tree \

all:	$(TESTS) smbsh

testsmbc: testsmbc.o 
	@echo Linking testsmbc
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT)

tree: tree.o
	@echo Linking tree
	$(CC) `gtk-config --cflags` $(CFLAGS) $(LDFLAGS) -o $@ $< `gtk-config --libs` $(LIBSMBCLIENT)

testacl: testacl.o
	@echo Linking testacl
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testacl2: testacl2.o
	@echo Linking testacl2
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testacl3: testacl3.o
	@echo Linking testacl3
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testbrowse: testbrowse.o
	@echo Linking testbrowse
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testbrowse2: testbrowse2.o
	@echo Linking testbrowse2
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

teststat: teststat.o
	@echo Linking teststat
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

teststat2: teststat2.o
	@echo Linking teststat2
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

teststat3: teststat3.o
	@echo Linking teststat3
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

teststatvfs: teststatvfs.o
	@echo Linking teststatvfs
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testfstatvfs: testfstatvfs.o
	@echo Linking testfstatvfs
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testtruncate: testtruncate.o
	@echo Linking testtruncate
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testchmod: testchmod.o
	@echo Linking testchmod
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testutime: testutime.o
	@echo Linking testutime
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testread: testread.o
	@echo Linking testread
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testwrite: testwrite.o
	@echo Linking testwrite
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

testctx: testctx.o
	@echo Linking testctx
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBSMBCLIENT) -lpopt

smbsh:
	make -C smbwrapper

clean:
	@rm -f *.o *~ $(TESTS)
	@make -C smbwrapper clean
