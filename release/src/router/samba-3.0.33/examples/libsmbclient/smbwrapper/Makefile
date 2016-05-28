LIBS = -lsmbclient -ldl
DEFS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE

CFLAGS = -I$(SAMBA_INCL) $(EXTLIB_INCL)

LDFLAGS = -L/usr/local/samba/lib

SMBINCLUDE = -I../../../source/include
CFLAGS= -fpic -g -O0 $(DEFS) $(SMBINCLUDE)

BIN	= .

STRFUNC = bsd-strlcat.o bsd-strlcpy.o


SMBWRAPPER_OBJS	= smbw.o smbw_dir.o smbw_stat.o wrapper.o select.o $(STRFUNC)
SMBSH_OBJS	= smbsh.o $(STRFUNC)

all:	$(BIN)/smbwrapper.so $(BIN)/smbsh

$(BIN)/smbwrapper.so:	$(SMBWRAPPER_OBJS)
	$(CC) -g \
		-Wl,-init=smbw_initialize \
		-shared \
		--export-all-symbols \
		-o $(BIN)/smbwrapper.so \
		$(SMBWRAPPER_OBJS) \
		$(LDFLAGS) \
		$(LIBS) \
		-Wl,-soname=`basename $@`

$(BIN)/smbsh: $(SMBSH_OBJS)
	$(CC) -g -o $(BIN)/smbsh $(SMBSH_OBJS) $(LIBS) $(LDFLAGS)

opendir_smbsh:	opendir_smbsh.o
	$(CC) -g -o opendir_smbsh opendir_smbsh.o $(LIBS) $(DMALLOC)

clean:
	rm -f *.o *~ opendir_smbsh smbsh smbwrapper.so
