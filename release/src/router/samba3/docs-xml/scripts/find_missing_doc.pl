#!/usr/bin/perl

my %doc;

$topdir = (shift @ARGV) or $topdir = ".";

##################################################
# Reading links from manpage

$curdir = $ENV{PWD};

chdir("smbdotconf");

open(IN,"xsltproc --xinclude --param smb.context ALL generate-context.xsl parameters.all.xml|");

while(<IN>) {
	if( /<listitem><para><link linkend="([^"]*)"><parameter moreinfo="none">([^<]*)<\/parameter><\/link><\/para><\/listitem>/g ){
		$doc{$2} = $1;
	}
}

close(IN);

chdir($curdir);

#################################################
# Reading entries from source code


open(SOURCE,"$topdir/param/loadparm.c") or die("Can't open $topdir/param/loadparm.c: $!");

while ($ln = <SOURCE>) {
  last if $ln =~ m/^static\ struct\ parm_struct\ parm_table.*/;
} #burn through the preceding lines

while ($ln = <SOURCE>) {
  last if $ln =~ m/^\s*\}\;\s*$/;
  #pull in the param names only
  next if $ln =~ m/.*P_SEPARATOR.*/;
  next unless $ln =~ /\s*\{\"(.*)\".*/;

  if($doc{lc($1)}) {
	$doc{lc($1)} = "FOUND";
  } else {
	print "'$1' is not documented\n";
  }
}
close SOURCE;

##################################################
# Trying to find missing references

foreach (keys %doc) {
	if($doc{$_} cmp "FOUND") {
		print "'$_' is documented but is not a configuration option\n";
	}
}
