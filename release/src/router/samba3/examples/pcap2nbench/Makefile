###############################################################################
##
##  pcap2nbench - Converts libpcap network traces to nbench input
##  Copyright (C) 2004  Jim McDonough <jmcd@us.ibm.com>
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
##  Written by Anthony Liguori <aliguori@us.ibm.com>
##
###############################################################################

OBJ=main.o ethernet.o ip.o tcp.o smb.o ntcreateandxrequest.o readandxrequest.o writeandxrequest.o closerequest.o ntcreateandxresponse.o
PROG=pcap2nbench
CXXFLAGS=-g -Wall
LDFLAGS=
LIB=-lpcap

$(PROG): $(OBJ)
	$(CXX) -o $(PROG) $(CXXFLAGS) $(OBJ) $(LDFLAGS) $(LIB)

clean:
	$(RM) $(OBJ) $(PROG)
