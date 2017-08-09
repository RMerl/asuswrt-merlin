#!/usr/bin/perl
# Script that finds macros in a configure script that are not 
# used in a set of C files.
# Copyright Jelmer Vernooij <jelmer@samba.org>, GPL
#
# Usage: ./$ARGV[0] configure.in [c-files...]

use strict;

sub autoconf_parse($$$$)
{
	my $in = shift;
	my $defines = shift;
	my $functions = shift;
	my $headers = shift;

	open(IN, $in) or die("Can't open $in");

	my $ln = 0;

	foreach(<IN>) {
		$ln++;

		if(/AC_DEFINE\(([^,]+),/) { 
			$defines->{$1} = "$in:$ln";
		} 
	
		if(/AC_CHECK_FUNCS\(\[*(.[^],)]+)/) {
			foreach(split / /, $1) { 
				$functions->{$_} = "$in:$ln";
			}
		}

		if(/AC_CHECK_FUNC\(([^,)]+)/) {
			$functions->{$1} = "$in:$ln";
		}

		if(/AC_CHECK_HEADERS\(\[*([^],)]+)/) {
			foreach(split / /, $1) { 
				$headers->{$_} = "$in:$ln";
			}
		}

		if(/AC_CHECK_HEADER\(([^,)]+)/) {
			$headers->{$1} = "$in:$ln";
		}

		if(/sinclude\(([^,]+)\)/) {
			autoconf_parse($1, $defines, $functions, $headers);
		}
	}

	close IN;
}

# Return the symbols and headers used by a C file
sub cfile_parse($$$)
{
	my $in = shift;
	my $symbols = shift;
	my $headers = shift;

	open(FI, $in) or die("Can't open $in");
	my $ln = 0;	
	my $line;
	while($line = <FI>) { 
		$ln++;
		$_ = $line;
		if (/\#([ \t]*)include ["<]([^">]+)/) { 
			$headers->{$2} = "$in:$ln";
		}

		$_ = $line;
		while(/([A-Za-z0-9_]+)/g) { 
			$symbols->{$1} = "$in:$ln";
		}
	}
	close FI;
}

my %ac_defines = ();
my %ac_func_checks = ();
my %ac_headers = ();
my %symbols = ();
my %headers = ();

if (scalar(@ARGV) <= 1) {
	print("Usage: configure_find_unused.pl configure.in [CFILE...]\n");
	exit 0;
}

autoconf_parse(shift(@ARGV), \%ac_defines, \%ac_func_checks, \%ac_headers);
cfile_parse($_, \%symbols, \%headers) foreach(@ARGV);

(keys %ac_defines) or warn("No defines found in configure.in file, parse error?");

foreach (keys %ac_defines) {
	if (not defined($symbols{$_})) {
		print "$ac_defines{$_}: Autoconf-defined $_ is unused\n";
	}
}

(keys %ac_func_checks) or warn("No function checks found in configure.in file, parse error?");

foreach (keys %ac_func_checks) {
	my $def = "HAVE_".uc($_);
	if (not defined($symbols{$_})) {
		print "$ac_func_checks{$_}: Autoconf-checked function `$_' is unused\n";
	} elsif (not defined($symbols{$def})) {
		print "$ac_func_checks{$_}: Autoconf-define `$def' for function `$_' is unused\n";
	}
}

(keys %ac_headers) or warn("No headers found in configure.in file, parse error?");

foreach (keys %ac_headers) {
	my $def = "HAVE_".uc($_);
	$def =~ s/[\/\.]/_/g;
	if (not defined($headers{$_})) {
		print "$ac_headers{$_}: Autoconf-checked header `$_' is unused\n";
	} elsif (not defined($symbols{$def})) {
		print "$ac_headers{$_}: Autoconf-define `$def' for header `$_' is unused\n"; 
	}
}
