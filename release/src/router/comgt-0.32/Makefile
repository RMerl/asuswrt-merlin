#
#   Makefile - build and install the comgt package
#   Copyright (C) 2005  Martin Gregorie
#   Copyright (C) 2006  Paul Hardwick
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#    martin@gregorie.org, paul@peck.org.uk
#
#    $Id: Makefile,v 1.2 2009-02-09 06:58:39 michael Exp $
#
#

CPROG	= comgt

all: $(CPROG)

install:
	install -D $(CPROG) $(INSTALLDIR)/bin/$(CPROG)
	install -D scripts/sigmon $(INSTALLDIR)/sbin/sigmon
	install -D scripts/read_sms $(INSTALLDIR)/sbin/read_sms
	install -D scripts/send_sms $(INSTALLDIR)/sbin/send_sms
	rm -rf $(INSTALLDIR)/rom/etc/ppp/3g
	mkdir -p $(INSTALLDIR)/rom/etc/ppp/3g
	cp -rf scripts/devices/* $(INSTALLDIR)/rom/etc/ppp/3g/
	install -D scripts/command $(INSTALLDIR)/rom/etc/ppp/3g/command
	install -D scripts/dump $(INSTALLDIR)/rom/etc/ppp/3g/dump
	install -D scripts/operator $(INSTALLDIR)/rom/etc/ppp/3g/operator
	install -D scripts/getinfo $(INSTALLDIR)/rom/etc/ppp/3g/getinfo

clean:
	rm -f *.o $(CPROG)

comgt: comgt.o
	$(CC) $^ $(LDFLAGS) -o $@
	$(STRIP) $@

comgt.o: comgt.c comgt.h
	$(CC) -c comgt.c $(CFLAGS) 

