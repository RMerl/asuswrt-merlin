EXES=bonnie++ zcav

all: $(EXES)

SCRIPTS=bon_csv2html bon_csv2txt

prefix=/tmp/bonnie++-1.03e/debian/bonnie++/usr
eprefix=${prefix}
#MORE_WARNINGS=-Weffc++ -Wcast-align
WFLAGS=-Wall -W -Wshadow -Wpointer-arith -Wwrite-strings -pedantic -ffor-scope $(MORE_WARNINGS)
CFLAGS=-O2  -DNDEBUG $(WFLAGS) $(MORECFLAGS)
CXX=g++ $(CFLAGS)

INSTALL=/usr/bin/install -c
INSTALL_PROGRAM=${INSTALL}

BONSRC=bon_io.cpp bon_file.cpp bon_time.cpp semaphore.cpp forkit.cpp \
 bon_suid.cpp
BONOBJS=$(BONSRC:.cpp=.o)

MAN1=bon_csv2html.1 bon_csv2txt.1
MAN8=bonnie++.8 zcav.8

ZCAVSRC=bon_suid.cpp
ZCAVOBJS=$(ZCAVSRC:.cpp=.o)

ALLOBJS=$(BONOBJS) $(ZCAVOBJS)

bonnie++: bonnie++.cpp $(BONOBJS)
	$(CXX) bonnie++.cpp -o bonnie++ $(BONOBJS) $(LFLAGS)

zcav: zcav.cpp $(ZCAVOBJS)
	$(CXX) zcav.cpp -o zcav $(ZCAVOBJS) $(LFLAGS)

install-bin: $(EXES)
	mkdir -p $(eprefix)/bin $(eprefix)/sbin
	${INSTALL} -s $(EXES) $(eprefix)/sbin
	${INSTALL} $(SCRIPTS) $(eprefix)/bin

install: install-bin
	mkdir -p /tmp/bonnie++-1.03e/debian/bonnie++/usr/share/man/man1 /tmp/bonnie++-1.03e/debian/bonnie++/usr/share/man/man8
	${INSTALL} -m 644 $(MAN1) /tmp/bonnie++-1.03e/debian/bonnie++/usr/share/man/man1
	${INSTALL} -m 644 $(MAN8) /tmp/bonnie++-1.03e/debian/bonnie++/usr/share/man/man8

%.o: %.cpp %.h bonnie.h port.h
	$(CXX) -c $<

bon_suid.o: bon_suid.cpp bonnie.h port.h
	$(CXX) -c $<

clean:
	rm -f $(EXES) $(ALLOBJS) build-stamp install-stamp
	rm -rf debian/tmp core debian/*.debhelper
	rm -f debian/{substvars,files} config.log

realclean: clean
	rm -f config.* Makefile bonnie.h port.h bonnie++.spec bon_csv2html bon_csv2txt sun/pkginfo
