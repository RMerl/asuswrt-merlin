#!/usr/bin/perl
use File::Copy;
#
# This program adds some HTML entities to the text files.  This will help prevent
# missing characters when including text documents in HTML.
#
# Written by:     Alex Burger
# Date:           December 29th, 2005
# 
@files = qw"
CHANGES
ERRATA
INSTALL
NEWS
PORTING
README
README.agent-mibs
README.agentx
README.aix
README.hpux11
README.irix
README.krb5
README.mib2c
README.mibs
README.osX
README.Panasonic_AM3X.txt
README.smux
README.snmpv3
README.solaris
README.thread
README.tru64
README.win32
TODO
perl/AnyData_SNMP/README
perl/default_store/README
perl/OID/README
perl/SNMP/README
perl/TrapReceiver/README
";


foreach my $file (@files) {
  open (FILEIN, $file) || die "Could not open file \'$file\' for reading. $!";
  open (FILEOUT, ">$file.new") || die "Could not open file \'$file.new\' for writing. $!";
  
  while ($line = <FILEIN>) {
    $line =~ s/&(?!lt|gt|quot|amp)/\&amp;/g;
    $line =~ s/</\&lt;/g;
    $line =~ s/>/\&gt;/g;
    $line =~ s/\"/\&quot;/g;
    print FILEOUT "$line";
  }
  close FILE;

  if (! (move ("$file", "$file.old"))) {
    die "Could not move $file to $file.old\n";
  }
  if (! (move ("$file.new", "$file"))) {
    die "Could not move $file.new to $file\n";
  }
}

