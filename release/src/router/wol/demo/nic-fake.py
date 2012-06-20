#!/usr/bin/env python

from socket import *
from struct import *

while 1:
	s = socket (AF_INET, SOCK_DGRAM)
	s.bind (("localhost", 40000))
	r = s.recv (1024)
	l = len (r)
	data = unpack (`l` + 'B', r)

	header = data[0:6]

	if l > 102: # secureon
		magic = data[6:l-5]
		secureon = data[l-6:]
	else:
		magic = data[6:]

	print
	print "NEW PACKET RECEIVED: " + `l` + " bytes"

	print "Header:   %2x %2x %2x %2x %2x %2x" % (header[0], header[1], header[2], header[3], header[4], header[5])

	print "Magic:    %2x %2x %2x %2x %2x %2x" % (magic[0], magic[1], magic[2], magic[3], magic[4], magic[5])

	for i in range (1, 16):
		off = i * 6
		print "          %2x %2x %2x %2x %2x %2x" % (magic[0+off], magic[1+off], magic[2+off], magic[3+off], magic[4+off], magic[5+off])

	if l > 102:
		print "SecureOn: %2x-%2x-%2x-%2x-%2x-%2x" % (secureon[0], secureon[1], secureon[2], secureon[3], secureon[4], secureon[5])

	print
	print "Woke up   %2x:%2x:%2x:%2x:%2x:%2x" % (magic[0], magic[1], magic[2], magic[3], magic[4], magic[5])
