#!/usr/bin/perl
##
## Scan a file of route-type definitions (see eg route_types.txt) and
## generate a corresponding header file with:
##
## - enum of Zserv route-types
## - redistribute strings for the various Quagga daemons
##
## See route_types.txt for the format.
##
##
## Copyright (C) 2009 David Lamparter.
## This file is part of GNU Zebra.
##
## GNU Zebra is free software; you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by the
## Free Software Foundation; either version 2, or (at your option) any
## later version.
##
## GNU Zebra is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with GNU Zebra; see the file COPYING.  If not, write to the Free
## Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.
##

use strict;

# input processing
#
my @protos;
my %protodetail;

my %daemons;

while (<STDIN>) {
	# skip comments and empty lines
	next if (/^\s*(#|$)/);

	# strip whitespace
	chomp;
	$_ =~ s/^\s*//;
	$_ =~ s/\s*$//;

	# match help strings
	if (/^(ZEBRA_ROUTE_[^\s]+)\s*,\s*"(.*)"$/) {
		$protodetail{$1}->{'longhelp'} = $2;
		next;
	}

	$_ =~ s/\s*,\s*/,/g;

	# else: 7-field line
	my @f = split(/,/, $_);
	unless (@f == 7) {
		die "invalid input on route_types line $.\n";
	}

	my $proto = $f[0];
	$f[3] = $1 if ($f[3] =~ /^'(.*)'$/);
	$f[6] = $1 if ($f[6] =~ /^"(.*)"$/);

	$protodetail{$proto} = {
		"number" => scalar @protos,
		"type" => $f[0],
		"cname" => $f[1],
		"daemon" => $f[2],
		"char" => $f[3],
		"ipv4" => int($f[4]),
		"ipv6" => int($f[5]),
		"shorthelp" => $f[6],
	};
	push @protos, $proto;
	$daemons{$f[2]} = {
		"ipv4" => int($f[4]),
		"ipv6" => int($f[5])
	} unless ($f[2] eq "NULL");
}

# output
printf <<EOF, $ARGV[0];
/* Auto-generated from route_types.txt by %s. */
/* Do not edit! */

#ifndef _QUAGGA_ROUTE_TYPES_H
#define _QUAGGA_ROUTE_TYPES_H

/* Zebra route's types. */
EOF

push @protos, "ZEBRA_ROUTE_MAX";
my (@protosv4, @protosv6) = ((), ());
for (my $c = 0; $c < @protos; $c++) {
	my $p = $protos[$c];
	printf "#define %-32s %d\n", $p, $c;
	push @protosv4, $p if ($protodetail{$p}->{"ipv4"});
	push @protosv6, $p if ($protodetail{$p}->{"ipv6"});
}
pop @protos;

sub codelist {
	my (@protos) = @_;
	my (@lines) = ();
	my $str = "  \"Codes: ";
	for my $p (@protos) {
		my $s = sprintf("%s - %s, ",
			$protodetail{$p}->{"char"},
			$protodetail{$p}->{"shorthelp"});
		if (length($str . $s) > 70) {
			$str =~ s/ $//;
			push @lines, $str . "%s\" \\\n";
			$str = "  \"       ";
		}
		$str .= $s;
	}
	$str =~ s/ $//;
	push @lines, $str . "%s\" \\\n";
	push @lines, "  \"       > - selected route, * - FIB route%s%s\", \\\n";
	my @nl = ();
	for (my $c = 0; $c < @lines + 1; $c++) {
		push @nl, "VTY_NEWLINE"
	}
	return join("", @lines) ."  ". join(", ", @nl);
}

print "\n";
printf "#define SHOW_ROUTE_V4_HEADER \\\n%s\n", codelist(@protosv4);
printf "#define SHOW_ROUTE_V6_HEADER \\\n%s\n", codelist(@protosv6);
print "\n";

sub collect {
	my ($daemon, $ipv4, $ipv6) = @_;
	my (@names, @help) = ((), ());
	for my $p (@protos) {
		next if ($protodetail{$p}->{"daemon"} eq $daemon && $daemon ne "zebra");
		next unless (($ipv4 && $protodetail{$p}->{"ipv4"})
				|| ($ipv6 && $protodetail{$p}->{"ipv6"}));
		push @names, $protodetail{$p}->{"cname"};
		push @help, "  \"".$protodetail{$p}->{"longhelp"}."\\n\"";
	}
	return ("\"(" . join("|", @names) . ")\"", join(" \\\n", @help));
}

for my $daemon (sort keys %daemons) {
	next unless ($daemons{$daemon}->{"ipv4"} || $daemons{$daemon}->{"ipv6"});
	printf "/* %s */\n", $daemon;
	if ($daemons{$daemon}->{"ipv4"} && $daemons{$daemon}->{"ipv6"}) {
		my ($names, $help) = collect($daemon, 1, 1);
		printf "#define QUAGGA_REDIST_STR_%s \\\n  %s\n", uc $daemon, $names;
		printf "#define QUAGGA_REDIST_HELP_STR_%s \\\n%s\n", uc $daemon, $help;
		($names, $help) = collect($daemon, 1, 0);
		printf "#define QUAGGA_IP_REDIST_STR_%s \\\n  %s\n", uc $daemon, $names;
		printf "#define QUAGGA_IP_REDIST_HELP_STR_%s \\\n%s\n", uc $daemon, $help;
		($names, $help) = collect($daemon, 0, 1);
		printf "#define QUAGGA_IP6_REDIST_STR_%s \\\n  %s\n", uc $daemon, $names;
		printf "#define QUAGGA_IP6_REDIST_HELP_STR_%s \\\n%s\n", uc $daemon, $help;
	} else {
		my ($names, $help) = collect($daemon,
			$daemons{$daemon}->{"ipv4"}, $daemons{$daemon}->{"ipv6"});
		printf "#define QUAGGA_REDIST_STR_%s \\\n  %s\n", uc $daemon, $names;
		printf "#define QUAGGA_REDIST_HELP_STR_%s \\\n%s\n", uc $daemon, $help;
	}
	print "\n";
}

print <<EOF;

#ifdef QUAGGA_DEFINE_DESC_TABLE

struct zebra_desc_table
{
  unsigned int type;
  const char *string;
  char chr;
};

#define DESC_ENTRY(T,S,C) [(T)] = { (T), (S), (C) }
static const struct zebra_desc_table route_types[] = {
EOF

for (my $c = 0; $c < @protos; $c++) {
	my $p = $protos[$c];
	printf "  DESC_ENTRY\t(%s\t \"%s\",\t'%s' ),\n",
	       $p.",", $protodetail{$p}->{"cname"}, $protodetail{$p}->{"char"};
}

print <<EOF;
};
#undef DESC_ENTRY

#endif /* QUAGGA_DEFINE_DESC_TABLE */

#endif /* _QUAGGA_ROUTE_TYPES_H */
EOF

