#!/bin/sh
###########################################################################
# LPRng - An Extended Print Spooler System
#
# Copyright 1988-1995 Patrick Powell, San Diego State University
#     papowell@sdsu.edu
# See LICENSE for conditions of use.
#
###########################################################################
# MODULE: TESTSUPPORT/makedb
# PURPOSE: process a raw configuration or printcap file and
#  generate a modified form
# makedb,v 3.1 1996/12/28 21:40:46 papowell Exp
# Input file format:                 Output File
# # comment                         #comment
# #start xx                         #start xx
# text                              #xx:text      - prefix #xx: 
# text                              #xx:text
# #end                              #end
# #all:text                         #all:all:text - remove # from all
# 
########################################################################## 
awk '
BEGIN{
	header = "";
}
/^#all/{
	print header substr( $0, 2 )
	next;
}
/^#start/ {
	header = "#" $2 ":";
	print $0;
	next;
}
/^#end/ {
	header = "";
	print $0;
	next;
}
{
	print header $0
}' $*
