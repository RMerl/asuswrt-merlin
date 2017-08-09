#!/usr/bin/perl
# Simple script for updating the prototypes in a C header file
#
# Copyright (C) 2006 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL

use strict;
use warnings;
use Getopt::Long;

=head1 NAME

update-proto - automatically update prototypes in header files

=head1 SYNOPSIS

update-proto [OPTIONS] <HEADER> <C-FILE>...

update-proto [OPTIONS] <HEADER> 

=head1 DESCRIPTION

Update-proto makes sure the prototypes in a C header file are current
by comparing the existing prototypes in a header with the function definition 
in the source file. It aims to keep the diff between the original header 
and generated one as small as possible.

New prototypes are inserted before any line that contains the following comment:

/* New prototypes are inserted above this line */

It will automatically parse C files after it encounters a line that contains:

/* The following definitions come from FILE */

When two or more prototypes exist for a function, only the first one 
will be kept.

=head1 OPTIONS

=over 4

=item I<--verbose|-v>

Increase verbosity. Currently only two levels of verbosity are used.

=item I<--help>

Show list of options

=back

=head1 BUGS

Strange complex functions are not recognized. In particular those 
created by macros or returning (without typedef) function pointers.

=head1 LICENSE

update-proto is licensed under the GNU General Public License L<http://www.gnu.org/licenses/gpl.html>.

=head1 AUTHOR

update-proto was written by Jelmer Vernooij L<jelmer@samba.org>.
	
=cut

sub Usage()
{
	print "Usage: update-proto.pl [OPTIONS] <HEADER> <C-FILE>...\n";
	exit 1;
}

sub Help()
{
	print "Usage: update-proto.pl [OPTIONS] <HEADER> <C-FILE>...\n";
	print "Options:\n";
	print "	--help			Show this help message\n";
	print "	--verbose		Write changes made to standard error\n\n";
	exit 0;
}

my %new_protos = ();

my $verbose = 0;

GetOptions(
	'help|h' => \&Help,
	'v|verbose' => sub { $verbose += 1; }
) or Usage();

sub count($$)
{
	my ($t, $s) = @_;
	my $count = 0;
	while($s =~ s/^(.)//) { $count++ if $1 eq $t; }
	return $count;
}

my $header = shift @ARGV;

sub process_file($)
{
	my $file = shift;
	open (IN, "<$file");
	while (my $line = <IN>) {
		$_ = $line;
		next if /^\s/;
		next unless /\(/;
		next if /^\/|[;]|^#|}|^\s*static/;
		s/\/\*(.*?)\*\///g;
		my $public = s/_PUBLIC_//g;
		s/_PRINTF_ATTRIBUTE\([^)]+\)//g;
		next unless /^(struct\s+\w+|union\s+\w+|\w+)\s+\**\s*(\w+)\s*\((.*)$/;

		my $name = $2;

		next if ($name eq "main");

		# Read continuation lines if any
		my $prn = 1 + count("(", $3) - count(")", $3);

		while ($prn) {
			my $l = <IN>;
			$l or die("EOF while parsing function prototype");
			$line .= $l;
			$prn += count("(", $l) - count(")", $l);
		}

		$line =~ s/\n$//;

		# Strip off possible start of function
		$line =~ s/{\s*$//g;
		
		$new_protos{$name} = "$line;";
	}
	close(IN);
}

process_file($_) foreach (@ARGV);

my $added = 0;
my $modified = 0;
my $deleted = 0;
my $kept = 0;

sub insert_new_protos()
{
	foreach (keys %new_protos) {
		print "$new_protos{$_}\n";
		print STDERR "Inserted prototype for `$_'\n" if ($verbose);
		$added+=1;
	}
	%new_protos = ();
}

my $blankline_due = 0;

open (HDR, "<$header");
while (my $line = <HDR>) {
	if ($line eq "\n") {
		$blankline_due = 1;
		$line = <HDR>;
	}

	# Recognize C files that prototypes came from
	if ($line =~ /\/\* The following definitions come from (.*) \*\//) {
		insert_new_protos();
		if ($blankline_due) {
			print "\n";
			$blankline_due = 0;
		}
		process_file($1);
		print "$line";
		next;
	}

	if ($blankline_due) {
		print "\n";
		$blankline_due = 0;
	}

	# Insert prototypes that weren't in the header before
	if ($line =~ /\/\* New prototypes are inserted above this line.*\*\/\s*/) {
		insert_new_protos();
		print "$line\n";
		next;
	}
	
	if ($line =~ /^\s*typedef |^\#|^\s*static/) {
		print "$line";
		next;
	}

	$_ = $line;
	s/\/\*(.*?)\*\///g;
	my $public = s/_PUBLIC_//g;
	s/_PRINTF_ATTRIBUTE\([^)]+\)//g;
	unless (/^(struct\s+\w+|union\s+\w+|\w+)\s+\**\s*(\w+)\s*\((.*)$/) {
		print "$line";
		next;
	}

	# Read continuation lines if any
	my $prn = 1 + count("(", $3) - count(")", $3);

	while ($prn) {
		my $l = <HDR>;
		$l or die("EOF while parsing function prototype");
		$line .= $l;
		$prn += count("(", $l) - count(")", $l);
	}

	my $name = $2;

	# This prototype is for a function that was removed
	unless (defined($new_protos{$name})) {
		$deleted+=1;
		print STDERR "Removed prototype for `$name'\n" if ($verbose);
		next;
	}

	my $nline = $line;
	chop($nline);

	if ($new_protos{$name} ne $nline) {
		$modified+=1;
		print STDERR "Updated prototype for `$name'\n" if ($verbose);
		print "$new_protos{$name}\n";
	} else {
		$kept+=1;
		print STDERR "Prototype for `$name' didn't change\n" if ($verbose > 1);
		print "$line";
	}

	delete $new_protos{$name};
}
close(HDR);

print STDERR "$added added, $modified modified, $deleted deleted, $kept unchanged.\n";

1;
