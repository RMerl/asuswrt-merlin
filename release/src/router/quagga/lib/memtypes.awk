###
# Copyright (C) Paul Jakma 2005
#
# This file is part of Quagga.
#
# Quagga is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any
# later version.
#
# Quagga is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Quagga; see the file COPYING.  If not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.  
###
#
# Scan a file of memory definitions (see eg memtypes.c) and generate
# a corresponding header file with an enum of the MTYPE's and declarations
# for the struct memory_list arrays
#
# struct memory_list's must be declared as:
# '\nstruct memory_list memory_list_<name>[] .....'
#
# Each MTYPE_ within the definition must the second token on the line,
# tokens being delineated by whitespace. It may only consist of the set of
# characters [[:upper:]_[:digit:]]. Eg:
#
# '\n  {  MTYPE_AWESOME_IPV8 , "Amazing new protocol, says genius" {}..boo'
#
# We try to ignore lines whose first token is /* or *, ie C comment lines.
# So the following should work fine:
#
# '/* This is the best memory_list ever!
# ' * It's got all my MTYPE's */
# '
# 'struct memory_list memory_list_my_amazing_mlist[] = =
# '{
# '  { MTYPE_DONGLE, "Dongle widget" }
# '  { MTYPE_FROB, "Frobulator" },
# '{  MTYPE_WIPPLE, "Wipple combombulator"}
# '}}}
#
# Even if it isn't quite a valid C declaration.
#

BEGIN {
	mlistregex = "memory_list_(.*)\\[\\]";
	mtyperegex = "^(MTYPE_[[:upper:]_[:digit:]]+).*";
	header = "/* Auto-generated from memtypes.c by " ARGV[0] ". */\n";
	header = header "/* Do not edit! */\n";
	header = header "\n#ifndef _QUAGGA_MEMTYPES_H\n";
	header = header "#define _QUAGGA_MEMTYPES_H\n";
	footer = "\n#endif /* _QUAGGA_MEMTYPES_H */\n\n";
	mlistformat = "extern struct memory_list memory_list_%s[];";
	printf ("%s\n", header);
}

# catch lines beginning with 'struct memory list ' and try snag the
# memory_list name. Has to be 3rd field.
($0 ~ /^struct memory_list /) && (NF >= 3) {
	mlists[lcount++] = gensub(mlistregex, "\\1", "g",$3);
}

# snag the MTYPE, it must self-standing and the second field,
# though we do manage to tolerate the , C seperator being appended
($1 !~ /^\/?\*/) && ($2 ~ /^MTYPE_/) { 
	mtype[tcount++] = gensub(mtyperegex, "\\1", "g", $2);
} 

END {
	printf("enum\n{\n  MTYPE_TMP = 1,\n"); 
	for (i = 0; i < tcount; i++) {
		if (mtype[i] != "" && mtype[i] != "MTYPE_TMP")
			printf ("  %s,\n", mtype[i]);
	}
	printf ("  MTYPE_MAX,\n};\n\n");
	for (i = 0; i < lcount; i++) {
		if (mlists[i] != "")
			printf (mlistformat "\n", mlists[i]);
	}
	printf (footer);
}
