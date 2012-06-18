# TSP - Berkely UNIX Time Synchronization Protocol
# Pattern attributes: good veryfast fast overmatch
# Protocol groups: time_synchronization open_source
# Wiki: http://www.protocolinfo.org/wiki/TSP
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# http://ftp.svbug.com/ftp/pub/manuals/pdf/smm.22.timed.pdf
# http://docs.freebsd.org/44doc/smm/12.timed/paper.pdf
# 
# This pattern is barely tested.

tsp
# type, version (1), sequence number, 8 type specific bytes, machine name
^[\x01-\x13\x16-$]\x01.?.?.?.?.?.?.?.?.?.?[ -~]+
