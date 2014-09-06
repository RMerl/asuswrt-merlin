#!/usr/bin/perl -w
##############################################################################
#
# Alex Burger - Oct 28th, 2004
#
# Purpose:  Modify .html files to add a header and footer for use
#           on the Net-SNMP web site.
#
#           Can also be used to change the 'section' variable
#           for use in the menu system.
#
# Notes:    A backup of each file is made to *.old.
#
#           Any DOS newlines are removed from the destination file.
#
#           Permissions are maintained.
#
##############################################################################
#
use File::Copy;
use File::stat;
use Getopt::Long;

my $tidy_options = '-f /dev/null -m -i -asxhtml -wrap 130 -quiet --quote-nbsp n';

my $pattern = '';
my $section = '';
my $tidy = 0;
my $body = 0;
my $help = 0;
my @files = ();

GetOptions 	('pattern=s' => \$pattern, 
		'section=s' => \$section,
		'tidy' => \$tidy,
		'body' => \$body,
		'help' => \$help);  

if ($help == 1)
{
$USAGE = qq/
Usage:
    add-header-footer [<options>] file1 file2 file3 ...
Options:
    --section=       Menu section
    --tidy           Run tidy on input file before processing (turns on --body)
    --body           Remove everything before <body> and after <\/body>
    --help           Display this message
    
Examples:

    add-header-footer.pl --section=tutorial --body cat.html dog.html mouse.html
    
    find . -name '*.html' | add-header-footer.pl --section=tutorial --body
    
/;
  print $USAGE;
  exit 0;
}
             
if ($ARGV[0]) {
  # Files listed on command line
  foreach my $arg (@ARGV) {
    chomp $arg;
    push @files, $arg;
    #print "$arg\n";
  }
}
else {
  # No arguments, so accept STDIN
  while (<STDIN>) {
    chomp;
    push @files, $_;
    #print "$_\n";
  }
}

if (! (@files) ) {
  exit 0;
}

#print "@files";

foreach my $file (@files) {
  chomp $file;
  print "Processing file: $file\n";

  # Grab current permissions
  my $sb = stat($file);
  my $stat_permissions = sprintf ("%04o", $sb->mode & 07777);
  my $stat_uid = $sb->uid;
  my $stat_gid = $sb->gid;
  
  my @old_file = ();
  my @new_file = ();

  my $body_count = 0;

  # Backup old file
  if (! (copy ("$file", "$file.old"))) {
    print "Could not backup existing file $file to $file.new.  Aborting.\n";
    next;
  }
  # Set permissions on old file to match original file
  chmod oct($stat_permissions), "$file.old";
  chown $stat_uid, $stat_uid, "$file.old";


  if ($tidy == 1) {
    $body = 1;          # Enable body, as tidy will add it in.
    my $tidy_command = "tidy $tidy_options $file";
    `$tidy_command`;
  }
   
  if (open (I, "<$file")) {
    # Load entire file
    while (<I>) {
      s/\015//g;          # Remove any DOS newlines
      chomp;
      push (@old_file, $_);
    }
  }
  else {
    print "Could not open file $file.  Aborting\n";
    next;
  }

  if (!@old_file) {
    print "Empty file.  Skipping\n";
    next;
  }

  # Remove empty lines at start
  while (1) {
    if ($old_file[0] eq "") {
      splice (@old_file, 0, 1);
    }
    else {
      last;
    }
  }

  # Remove empty lines at end
  while (1) {
    if ($old_file[$#old_file] eq "") {
      splice (@old_file, -1, 1);
    }
    else {
      last;
    }
  }
  
  if ($body == 1) {
    # Count the number of <body lines
    for (my $i = 0; $i <= $#old_file; $i++) {
      if ($old_file[$i] =~ /<body/) {
        $body_count++;
        next;
      }
    }
  
    # Remove anything before and including <body
    while ($body_count > 0) {
      while (! ($old_file[0] =~ /<body/)) {
        splice (@old_file, 0, 1);
      }
      splice (@old_file, 0, 1);   # <body line
      $body_count--;
    }
  }

  # Start to build new file in memory with header
  push (@new_file, "<!--#set var=\"section\" value=\"$section\" -->\n");
  push (@new_file, '<!--#include virtual="/page-top.html" -->' . "\n");
  push (@new_file, '<!-- CONTENT START -->' . "\n");

  # Add in old file, skipping existing header and footer and stopping at <body/>
  for (my $i = 0; $i <= $#old_file; $i++) {
    if (!(defined($old_file[$i]))) { next; }
    if ($body == 1 && ($old_file[$i] =~ /<\/body>/)) { last; }  
    elsif ($old_file[$i] =~ /<!--#set var="section" value=/) { next; }
    elsif ($old_file[$i] =~ /<!--#include virtual="\/page-top.html" -->/) { next; }
    elsif ($old_file[$i] =~ /<!-- CONTENT START -->/) { next; }
    elsif ($old_file[$i] =~ /<!-- CONTENT END -->/) { next; }
    elsif ($old_file[$i] =~ /<!--#include virtual="\/page-bottom.html" -->/) { next; }
    
    push (@new_file, $old_file[$i] . "\n");
  }

  # Finish to building new file in memory with footer
  push (@new_file, '<!-- CONTENT END -->' . "\n");
  push (@new_file, '<!--#include virtual="/page-bottom.html" -->' . "\n");
   
  # Save new file  
  if (open (O, ">$file")) {
    for (my $i = 0; $i <= $#new_file; $i++) {
      print O "$new_file[$i]";
    }
    print O "\n";
    close O;
    
    # Set permissions
    chmod oct($stat_permissions), $file;
    chown $stat_uid, $stat_uid, $file;
  }
  else {
    print "Could not create new file: $file.new\n"
  }
  close I;
}


