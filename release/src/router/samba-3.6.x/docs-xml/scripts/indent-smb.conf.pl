#!/usr/bin/perl

while(<STDIN>) {
	if(/^$/) { }
	elsif(/^([ \t]*)#(.*)/) { print "#$2\n"; }
	elsif(/^([ \t]*)(.*) = (.*)$/) { print "\t$2 = $3\n"; }
	elsif(/^([ \t]*)\[(.*)\]([ \t]*)$/) { print "\n[$2]\n"; }
}
