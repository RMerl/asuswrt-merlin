#! /usr/bin/perl
##
## vi:ts=4
##
##---------------------------------------------------------------------------##
##
##  Author:
##      Markus F.X.J. Oberhumer         <markus@oberhumer.com>
##
##  Description:
##      Create short files for compression test
##
##  Copyright (C) 1996-2011 Markus Franz Xaver Johannes Oberhumer
##
##---------------------------------------------------------------------------##

$c = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
$c = "\x00\x01\x02";
$c = "\x00";

$x = $c x 1024;

for $i (0 .. 50) {
    $name = sprintf("f%04d.dat",$i);
    open(OUTFILE,">$name") || die "$0: cannot open '$name': $!";
    binmode(OUTFILE);
    print OUTFILE substr($x,0,$i);
    close(OUTFILE);
}

exit(0);
