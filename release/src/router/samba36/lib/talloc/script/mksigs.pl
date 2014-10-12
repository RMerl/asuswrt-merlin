#!/usr/bin/perl

# mksigs.pl - extract signatures from C headers
#
# Copyright (C) Michael Adam 2009
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.

# USAGE:  cat $header_files | mksigs.pl > $signature_file
#
# The header files to parse are read from stdin.
# The output is in a form as produced by gcc with the -aux-info switch
# and printed to stdout.

use strict;
use warnings;

my $in_comment = 0;
my $in_doxygen = 0;
my $extern_C_block = 0;

while (my $LINE = <>) {
	# find end of started multi-line-comment
	if ($in_comment) {
		if ($LINE =~ /^.*?\*\/(.*)$/) {
			$LINE = $1;
			$in_comment = 0;
		} else {
			# whole line within comment
			next;
		}
	}

	# find end of DOXYGEN section
	if ($in_doxygen) {
		if ($LINE =~ /^#\s*else(?:\s+.*)?$/) {
			$in_doxygen = 0;
		}
		next;
	}

	# strip C++-style comments
	$LINE =~ s/^(.*?)\/\/.*$/$1/;

	# strip in-line-comments:
	while ($LINE =~ /\/\*.*?\*\//) {
		$LINE =~ s/\/\*.*?\*\///;
	}

	# find starts of multi-line-comments
	if ($LINE =~ /^(.*)\/\*/) {
		$in_comment = 1;
		$LINE = $1;
	}

	# skip empty lines
	next if $LINE =~ /^\s*$/;

	# remove leading spaces
	$LINE =~ s/^\s*(.*)$/$1/;

	# concatenate lines split with "\" (usually macro defines)
	while ($LINE =~ /^(.*?)\s+\\$/) {
		my $LINE2 = <>;
		$LINE = $1;
		$LINE2 =~ s/^\s*(.*)$/$1/;
		$LINE .= " " . $LINE2;
	}

        # remove DOXYGEN sections
	if ($LINE =~ /^#\s*ifdef\s+DOXYGEN(?:\s+.*)?$/) {
		$in_doxygen = 1;
                next;
	}


	# remove all preprocessor directives
	next if ($LINE =~ /^#/);

	if ($LINE =~ /^extern\s+"C"\s+\{/) {
		$extern_C_block = 1;
		next;
	}

	if (($LINE =~ /^[^\{]*\}/) and $extern_C_block) {
		$extern_C_block = 0;
		next;
	}

	$LINE =~ s/^extern\s//;

	# concatenate braces stretched over multiple lines
	# (from structs or enums)
	my $REST = $LINE;
	my $braces = 0;
	while (($REST =~ /[\{\}]/) or ($braces)) {
		while ($REST =~ /[\{\}]/) {
			# collect opening
			while ($REST =~ /^[^\{\}]*\{(.*)$/) {
				$braces++;
				$REST = $1;
			}

			# collect closing
			while ($REST =~ /^[^\{\}]*\}(.*)$/) {
				$braces--;
				$REST = $1;
			}
		}

		# concatenate if not balanced
		if ($braces) {
			if (my $LINE2 = <>) {
				$LINE2 =~ s/^\s*(.*)$/$1/;
				chomp($LINE);
				$LINE .= " " . $LINE2;
				chomp $REST;
				$REST .= " " . $LINE2;
			} else {
				print "ERROR: unbalanced braces ($braces)\n";
				last;
			}
		}
	}

	# concetenate function prototypes that stretch over multiple lines
	$REST = $LINE;
	my $parenthesis = 0;
	while (($REST =~ /[\(\)]/) or ($parenthesis)) {
		while ($REST =~ /[\(\)]/) {
			# collect opening
			while ($REST =~ /^[^\(\)]*\((.*)$/) {
				$parenthesis++;
				$REST = $1;
			}

			# collect closing
			while ($REST =~ /^[^\(\)]*\)(.*)$/) {
				$parenthesis--;
				$REST = $1;
			}
		}

		# concatenate if not balanced
		if ($parenthesis) {
			if (my $LINE2 = <>) {
				$LINE2 =~ s/^\s*(.*)$/$1/;
				chomp($LINE);
				$LINE .= " " . $LINE2;
				chomp($REST);
				$REST .= " " . $LINE2;
			} else {
				print "ERROR: unbalanced parantheses ($parenthesis)\n";
				last;
			}
		}
	}

	next if ($LINE =~ /^typedef\s/);
	next if ($LINE =~ /^enum\s+[^\{\(]+\s+\{/);
	next if ($LINE =~ /^struct\s+[^\{\(]+\s+\{.*\}\s*;/);
	next if ($LINE =~ /^struct\s+[a-zA-Z0-9_]+\s*;/);

	# remove trailing spaces
	$LINE =~ s/(.*?)\s*$/$1/;

	$LINE =~ s/^(.*\))\s+PRINTF_ATTRIBUTE\([^\)]*\)(\s*[;,])/$1$2/;
	$LINE =~ s/^(.*\))\s*[a-zA-Z0-9_]+\s*;$/$1;/;

	# remove parameter names - slightly too coarse probably
	$LINE =~ s/([\s\(]\*?)[_0-9a-zA-Z]+\s*([,\)])/$1$2/g;

	# remedy (void) from last line
	$LINE =~ s/\(\)/(void)/g;

	# normalize spaces
	$LINE =~ s/\s*\)\s*/)/g;
	$LINE =~ s/\s*\(\s*/ (/g;
	$LINE =~ s/\s*,\s*/, /g;

	# normalize unsigned
	$LINE =~ s/([\s,\(])unsigned([,\)])/$1unsigned int$2/g;

	# normalize bool
	$LINE =~ s/(\b)bool(\b)/_Bool/g;

	print $LINE . "\n";
}
