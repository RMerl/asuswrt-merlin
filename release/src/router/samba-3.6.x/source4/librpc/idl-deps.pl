#!/usr/bin/perl
use strict;
use File::Basename;

sub add($$)
{
	my ($name, $val) = @_;

	print "$name += $val\n";
}

my %vars = ();

foreach(@ARGV) {
	my $f = $_;
	my $b = basename($f);

	$b =~ s/\.idl//;

	my $gen_ndr = dirname($f);
	$gen_ndr =~ s/\/idl$/\/gen_ndr/;

	print "# $f\n";
	add("IDL_FILES", $f);
	add("IDL_HEADER_FILES", "$gen_ndr/$b.h");
	add("IDL_NDR_PARSE_H_FILES", "$gen_ndr/ndr_$b.h");
	add("IDL_NDR_PARSE_C_FILES", "$gen_ndr/ndr_$b.c");
	add("IDL_NDR_CLIENT_C_FILES", "$gen_ndr/ndr_$b\_c.c");
	add("IDL_NDR_CLIENT_H_FILES", "$gen_ndr/ndr_$b\_c.h");
	add("IDL_SWIG_FILES", "$gen_ndr/$b.i");
	add("IDL_NDR_SERVER_C_FILES", "$gen_ndr/ndr_$b\_s.c");
	add("IDL_NDR_EJS_C_FILES", "$gen_ndr/ndr_$b\_ejs.c");
	add("IDL_NDR_EJS_H_FILES", "$gen_ndr/ndr_$b\_ejs.h");
	add("IDL_NDR_PY_C_FILES", "$gen_ndr/py_$b.c");
	add("IDL_NDR_PY_H_FILES", "$gen_ndr/py_$b.h");
	print "\n";
}
