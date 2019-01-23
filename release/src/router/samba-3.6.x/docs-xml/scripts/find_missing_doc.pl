#!/usr/bin/perl

my %doc;

$topdir = (shift @ARGV) or $topdir = ".";

##################################################
# Reading links from manpage

$curdir = $ENV{PWD};

chdir("smbdotconf");

open(IN,"xsltproc --xinclude --param smb.context ALL generate-context.xsl parameters.all.xml|");

while(<IN>) {
	if( /<samba:parameter .*?name="([^"]*?)"/g ){
		my $name = $1;
	    $name =~ s/ //g;
		$doc{$name} = "NOTFOUND";
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
  next unless $ln =~ /\s*\.label\s*=\s*\"(.*)\".*/;

  my $name = $1;
  $name =~ s/ //g;

  if($doc{lc($name)}) {
	$doc{lc($name)} = "FOUND";
  } else {
	print "'$name' is not documented\n";
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
