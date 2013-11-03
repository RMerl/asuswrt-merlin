#!/usr/bin/perl
#
# usage: make-precompose.h.pl UnicodeData.txt > precompose.h
#
# (c) 2008-2011 by HAT <hat@fa2.so-net.ne.jp>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 

# See
# http://www.unicode.org/Public/UNIDATA/UCD.html
# http://www.unicode.org/reports/tr15/
# http://www.unicode.org/Public/*/ucd/UnicodeData*.txt
# http://www.unicode.org/Public/UNIDATA/UnicodeData.txt


# temp files for binary search (compose.TEMP, compose_sp.TEMP) -------------

open(UNICODEDATA, "<$ARGV[0]");

open(COMPOSE_TEMP, ">compose.TEMP");
open(COMPOSE_SP_TEMP, ">compose_sp.TEMP");

while (<UNICODEDATA>) {
    chop;
    (
     $code0,
     $Name1,
     $General_Category2,
     $Canonical_Combining_Class3,
     $Bidi_Class4,
     $Decomposition_Mapping5,
     $Numeric_Value6,
     $Numeric_Value7,
     $Numeric_Value8,
     $Bidi_Mirrored9,
     $Unicode_1_Name10,
     $ISO_Comment11,
     $Simple_Uppercase_Mapping12,
     $Simple_Lowercase_Mapping13,
     $Simple_Titlecase_Mapping14
    ) = split(/\;/);

    if (($Decomposition_Mapping5 ne "") && ($Decomposition_Mapping5 !~ /\</) && ($Decomposition_Mapping5 =~ / /)) {
	($base, $comb) = split(/ /,$Decomposition_Mapping5);

	$leftbracket  = "  { ";
	$rightbracket =" },     ";

	# AFP 3.x Spec
	if ( ((0x2000  <= hex($code0)) && (hex($code0) <=  0x2FFF))
	     || ((0xFE30  <= hex($code0)) && (hex($code0) <=  0xFE4F))
	     || ((0x2F800 <= hex($code0)) && (hex($code0) <= 0x2FA1F))) {
	    $leftbracket  = "\/\*{ ";
	    $rightbracket =" },\*\/   ";
	}

	if (hex($code0) > 0xFFFF) {

	    $code0_sp_hi = 0xD800 - (0x10000 >> 10) + (hex($code0) >> 10);
	    $code0_sp_lo = 0xDC00 + (hex($code0) & 0x3FF);

	    $base_sp_hi = 0xD800 - (0x10000 >> 10) + (hex($base) >> 10);
	    $base_sp_lo = 0xDC00 + (hex($base) & 0x3FF);

	    $comb_sp_hi = 0xD800 - (0x10000 >> 10) + (hex($comb) >> 10);
	    $comb_sp_lo = 0xDC00 + (hex($comb) & 0x3FF);

	    printf(COMPOSE_SP_TEMP "%s0x%04X%04X, 0x%04X%04X, 0x%04X%04X%s\/\* %s \*\/\n",
		   $leftbracket, $code0_sp_hi ,$code0_sp_lo, $base_sp_hi, $base_sp_lo, $comb_sp_hi, $comb_sp_lo, $rightbracket, $Name1);

	    $leftbracket  = "\/\*{ ";
	    $rightbracket =" },\*\/   ";
	}

	printf(COMPOSE_TEMP "%s0x%08X, 0x%08X, 0x%08X%s\/\* %s \*\/\n", $leftbracket, hex($code0), hex($base), hex($comb), $rightbracket, $Name1);

    }
}

close(UNICODEDATA);

close(COMPOSE_TEMP);
close(COMPOSE_SP_TEMP);

# macros for BMP (PRECOMP_COUNT, DECOMP_COUNT, MAXCOMBLEN) ----------------

open(COMPOSE_TEMP, "<compose.TEMP");

@comp_table = ();
$comp_count = 0;

while (<COMPOSE_TEMP>) {
    if (m/^\/\*/) {
	next;
    }
    $comp_table[$comp_count][0] = substr($_, 4, 10);
    $comp_table[$comp_count][1] = substr($_, 16, 10);
    $comp_count++;
}

$maxcomblen = 2;      # Hangul's maxcomblen is already 2. That is, VT.

for ($i = 0 ; $i < $comp_count ; $i++) {
    $base = $comp_table[$i][1];
    $comblen = 1;
    $j = 0;
    while ($j < $comp_count) {
	if ($base ne $comp_table[$j][0]) {
	    $j++;
	    next;
	} else {
	    $comblen++;
	    $base =  $comp_table[$j][1];
	    $j = 0;
	}
    }
    $maxcomblen = ($maxcomblen > $comblen) ? $maxcomblen : $comblen;
}

close(COMPOSE_TEMP);

# macros for SP (PRECOMP_SP_COUNT,DECOMP_SP_COUNT, MAXCOMBSPLEN) -----------

open(COMPOSE_SP_TEMP, "<compose_sp.TEMP");

@comp_sp_table = ();
$comp_sp_count = 0;

while (<COMPOSE_SP_TEMP>) {
    if (m/^\/\*/) {
	next;
    }
    $comp_sp_table[$comp_sp_count][0] = substr($_, 4, 10);
    $comp_sp_table[$comp_sp_count][1] = substr($_, 16, 10);
    $comp_sp_count++;
}

$maxcombsplen = 2;     # one char have 2 codepoints, like a D8xx DCxx.

for ($i = 0 ; $i < $comp_sp_count ; $i++) {
    $base_sp = $comp_sp_table[$i][1];
    $comblen = 2;
    $j = 0;
    while ($j < $comp_sp_count) {
	if ($base_sp ne $comp_sp_table[$j][0]) {
	    $j++;
	    next;
	} else {
	    $comblen += 2;
	    $base_sp =  $comp_sp_table[$j][1];
	    $j = 0;
	}
    }
    $maxcombsplen = ($maxcombsplen > $comblen) ? $maxcombsplen : $comblen;
}

close(COMPOSE_SP_TEMP);

# macro for buffer length (COMBBUFLEN) -------------------------------------

$combbuflen = ($maxcomblen > $maxcombsplen) ? $maxcomblen : $maxcombsplen;

# sort ---------------------------------------------------------------------

system("sort -k 3 compose.TEMP \> precompose.SORT");
system("sort -k 2 compose.TEMP \>  decompose.SORT");

system("sort -k 3 compose_sp.TEMP \> precompose_sp.SORT");
system("sort -k 2 compose_sp.TEMP \>  decompose_sp.SORT");

# print  -------------------------------------------------------------------

print ("\/\* DO NOT EDIT BY HAND\!\!\!                                           \*\/\n");
print ("\/\* This file is generated by                                        \*\/\n");
printf ("\/\*       contrib/shell_utils/make-precompose.h.pl %s   \*\/\n", $ARGV[0]);
print ("\n");
printf ("\/\* %s is got from                                      \*\/\n", $ARGV[0]);
print ("\/\* http\:\/\/www.unicode.org\/Public\/UNIDATA\/UnicodeData.txt            \*\/\n");
print ("\n");

print ("\#define SBASE 0xAC00\n");
print ("\#define LBASE 0x1100\n");
print ("\#define VBASE 0x1161\n");
print ("\#define TBASE 0x11A7\n");
print ("\#define LCOUNT 19\n");
print ("\#define VCOUNT 21\n");
print ("\#define TCOUNT 28\n");
print ("\#define NCOUNT 588     \/\* (VCOUNT \* TCOUNT) \*\/\n");
print ("\#define SCOUNT 11172   \/\* (LCOUNT \* NCOUNT) \*\/\n");
print ("\n");

printf ("\#define PRECOMP_COUNT %d\n", $comp_count);
printf ("\#define DECOMP_COUNT %d\n", $comp_count);
printf ("\#define MAXCOMBLEN %d\n", $maxcomblen);
print ("\n");
printf ("\#define PRECOMP_SP_COUNT %d\n", $comp_sp_count);
printf ("\#define DECOMP_SP_COUNT %d\n", $comp_sp_count);
printf ("\#define MAXCOMBSPLEN %d\n", $maxcombsplen);
print ("\n");
printf ("\#define COMBBUFLEN %d  \/\* max\(MAXCOMBLEN\,MAXCOMBSPLEN\) \*\/\n", $combbuflen);
print ("\n");

print ("static const struct \{\n");
print ("  unsigned int replacement\;\n");
print ("  unsigned int base\;\n");
print ("  unsigned int comb\;\n");
print ("\} precompositions\[\] \= \{\n");

system("cat precompose.SORT");

print ("\}\;\n");
print ("\n");

print ("static const struct \{\n");
print ("  unsigned int replacement\;\n");
print ("  unsigned int base\;\n");
print ("  unsigned int comb\;\n");
print ("\} decompositions\[\] \= \{\n");

system("cat decompose.SORT");

print ("\}\;\n");
print ("\n");



print ("static const struct \{\n");
print ("  unsigned int replacement_sp\;\n");
print ("  unsigned int base_sp\;\n");
print ("  unsigned int comb_sp\;\n");
print ("\} precompositions_sp\[\] \= \{\n");

system("cat precompose_sp.SORT");

print ("\}\;\n");
print ("\n");

print ("static const struct \{\n");
print ("  unsigned int replacement_sp\;\n");
print ("  unsigned int base_sp\;\n");
print ("  unsigned int comb_sp\;\n");
print ("\} decompositions_sp\[\] \= \{\n");

system("cat decompose_sp.SORT");

print ("\}\;\n");
print ("\n");

print ("\/\* EOF \*\/\n");

# EOF
