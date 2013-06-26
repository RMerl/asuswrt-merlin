#!/usr/bin/perl -w

###################################################
# package to parse ASN.1 files and generate code for
# LDAP functions in Samba
# Copyright tridge@samba.org 2002-2003
# Copyright metze@samba.org 2004

# released under the GNU GPL

use strict;

use FindBin qw($RealBin);
use lib "$RealBin";
use lib "$RealBin/lib";
use Getopt::Long;
use File::Basename;
use asn1;
use util;

my($opt_help) = 0;
my($opt_output);

my $asn1_parser = new asn1;

#####################################################################
# parse an ASN.1 file returning a structure containing all the data
sub ASN1Parse($)
{
    my $filename = shift;
    my $asn1 = $asn1_parser->parse_asn1($filename);
    util::CleanData($asn1);
    return $asn1;
}


#########################################
# display help text
sub ShowHelp()
{
    print "
           perl ASN.1 parser and code generator
           Copyright (C) tridge\@samba.org
           Copyright (C) metze\@samba.org

           Usage: pasn1.pl [options] <asn1file>

           Options:
             --help                this help page
             --output OUTNAME      put output in OUTNAME
           \n";
    exit(0);
}

# main program
GetOptions (
	    'help|h|?' => \$opt_help,
	    'output|o=s' => \$opt_output,
	    );

if ($opt_help) {
    ShowHelp();
    exit(0);
}

sub process_file($)
{
	my $input_file = shift;
	my $output_file;
	my $pasn1;

	my $basename = basename($input_file, ".asn1");

	if (!defined($opt_output)) {
		$output_file = util::ChangeExtension($input_file, ".pasn1");
	} else {
		$output_file = $opt_output;
	}

#	if (file is .pasn1) {
#		$pasn1 = util::LoadStructure($pasn1_file);
#		defined $pasn1 || die "Failed to load $pasn1_file - maybe you need --parse\n";
#       } else {
		$pasn1 = ASN1Parse($input_file);
		defined $pasn1 || die "Failed to parse $input_file";
		util::SaveStructure($output_file, $pasn1) ||
		    die "Failed to save $output_file\n";
	#}
}

foreach my $filename (@ARGV) {
	process_file($filename);
}
