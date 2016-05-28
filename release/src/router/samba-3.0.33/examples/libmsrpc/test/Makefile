CC=gcc
INCLUDES= -I`pwd` -I../../../source/ -I../../../source/include -I../../../source/ubiqx

DEFS= -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
#CFLAGS= -O -D_SAMBA_BUILD_ -gstabs -Wall -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -DDEBUG_PASSWORD -DDEVELOPER -Wdeclaration-after-statement -g $(INCLUDES) $(DEFS) -fPIC

CFLAGS= -g -Wall -ansi $(INCLUDES) 

LDFLAGS=-L. -L../../bin/ 
LIBS=../../../source/bin/libmsrpc.so

TESTS= lsapol lsaq lsaenum lsaenumprivs lsapriv ear \
       regkey regopenkey regkeyenum regvalenum regsetval regqueryval regdelete security \
		 adduser samenum samlookup samgroup enable disable dominfo samuser \
		 svc \
		 smbc

all: $(TESTS)

lsapol: lsa/lsapol.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

lsapriv: lsa/lsapriv.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

lsaq: lsa/lsaq.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

lsaenum: lsa/lsaenum.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

lsaenumprivs: lsa/lsaenumprivs.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

lsaaddrights: lsa/lsaaddrights.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

ear: lsa/ear.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

regkey: reg/regkey.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

regopenkey: reg/regopenkey.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

regkeyenum: reg/regkeyenum.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

regkeycreate: reg/regkeycreate.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

regvalenum: reg/regvalenum.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

regsetval: reg/regsetval.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

regqueryval: reg/regqueryval.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

regdelete: reg/regdelete.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

security: reg/security.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

adduser: sam/adduser.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

samenum: sam/samenum.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

samlookup: sam/samlookup.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

samgroup: sam/samgroup.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

enable: sam/enable.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

disable: sam/disable.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

samuser: sam/samuser.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

dominfo: sam/dominfo.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

svc: svcctl/svc.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS)

smbc: smbc_test/smbc.o test_util.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< test_util.o $(LIBS) ../../../source/bin/libsmbclient.so

clean:
	rm -f $(TESTS) *.o lsa/*.o reg/*.o sam/*.o
