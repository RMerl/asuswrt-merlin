#! /usr/bin/perl

use strict;
use File::Copy;
use File::Basename;
use IO::File;
use Getopt::Std;

my $ID_NOREAD  = "NETSNMP_NO_READ_SUPPORT";
my $ID_NOWRITE = "NETSNMP_NO_WRITE_SUPPORT";
# my $ID_MIN     = "NETSNMP_MINIMAL_CODE";

my($ST_OFF, $ST_IF, $ST_ELSE, $ST_IFN, $ST_ELSEN) =
  ("off", "if", "else", "ifnot", "elsenot");

my %opts    = ();
my %thash   = ();
my $canwrite = 1; # current write state

my($appname,$apppath) = fileparse($0);

my $minimal_include_path = "include/net-snmp/net-snmp-features.h";


if ( (!getopts("rwmi:v",\%opts)) || (1 != $#ARGV) ) {
  print "$appname [options] from-directory to-direpctory\n";
  print "-r     parse out code unneeded by $ID_NOREAD ifdef\n";
  print "-w     parse out code unneeded by $ID_NOWRITE ifdef (DEFAULT)\n";
  print "-m     parse out code unneeded according minimalist ifdefs\n";
  print "       requires from-directory to be the top of the net-snmp tree\n";
  print "       (This is multiple ifdefs auto selected depending\n";
  print "        on the net-snmp source configuration)\n\n";
  print "-i 'ignore-file'  file of files to ignore (not copy)\n";
  print "-v     print verbose info to standard out\n";
  die "Error: two command line arguments required\n";
}

#default to everything
if ( (!exists $opts{r}) && (!exists $opts{w}) && (!exists $opts{m}) ) {
  $thash{"$ID_NOWRITE"} = "$ST_OFF";
}
else {
  $thash{"$ID_NOREAD"}  = "$ST_OFF" if ( exists $opts{r} );
  $thash{"$ID_NOWRITE"} = "$ST_OFF" if ( exists $opts{w} );
}

my $fromdir = $ARGV[0];
my $todir   = $ARGV[1];

if ( !(-e $fromdir) ) {
  die "Error: $appname: from directory does not exist: '$fromdir'\n";
}
if ( !(-d $fromdir) ) {
    die "Error: $appname: from directory, '$fromdir', must be a directory\n";
}

if ( exists $opts{m} ) {
    load_minamal_ifdefs();
}

# create search string from tags
# minimal must be done before this
my $search = join "|", (keys %thash);


# check for and load ignore file
my $igstring = "";
if ( exists $opts{i} ) {
  open my($IH), "< $opts{i}" or
    die "Could not open ignore-file '$opts{i}': $!";
  my @iglist = <$IH>;
  $IH->close();
  chomp @iglist;
  $igstring = join "|", @iglist;
  $igstring = "(" . $igstring . ")";
  print "ignore string: \'$igstring\'\n" if (exists $opts{v});
}



if ( -e $todir ) {
  if ( ((-d $fromdir) && !(-d $todir)) ||
	   (!(-d $fromdir) && (-d $todir)) ) {
	die "Error: $appname: from-directory and to-directory must both either be a file or both be a directory\n";
  }
}
else {
  if (-d $fromdir) {
	print "Warning: $appname: '$todir' does not exist, creating\n";
	mkdir "$todir" or
      die "Error: $appname: Unable to create to-directory '$todir': $!\n";
  }
}

if (-d $fromdir) {
  parsedirectory($fromdir, $todir);
}
else {
  parsefile($fromdir, $todir);
}



exit(0);


# PROCEDURES  PROCEDURES  PROCEDURES  PROCEDURES  PROCEDURES


sub parsedirectory {
  my($fdir, $tdir) = @_;

  my @ldirs = ();
  my $DH;
  opendir my($DH), $fdir or die "Could not open directory '$fdir': $!";
  if ( !(-e $tdir) || !(-d $tdir) ) {
    mkdir $tdir or die "Could not create directory '$tdir': $!";
  }
  my @flist = readdir $DH;
  closedir $DH;
  # remove . and ..
  @flist = grep (! /(^\.$|^\.\.$)/ , @flist);

  while (my $name = shift @flist) {
    if (-d "$fdir/$name") {
      push @ldirs, "$name";
    }
    else {
      parsefile("$fdir/$name", "$tdir/$name");
    }
  }

  while (my $name = shift @ldirs) {
    parsedirectory("$fdir/$name", "$tdir/$name")
  }

} # parsedirectory


# returns 1 if current state for tag is write, 0 otherwise
sub iswritestate {
  my $tag = "";

  foreach $tag (keys %thash) {
	if ( ($thash{$tag} eq "$ST_ELSE") ||
		 ($thash{$tag} eq "$ST_IFN") ) {
	  return(0);
    }
  }

  return(1);
} # iswritestate


# Check $line for ifdef state changes for all of the tags and change
# state.
# If there is a state change error return 0, otherwise return 1;

sub checkifdef {
  my($TF, $line, $fromfilename) = @_;

  if ( $line =~ /(#ifdef|#ifndef|#else|#endif).*($search)(\s|$|\*)/ ) {
    my $copt = $1;
    my $tag  = $2;

	if ( $copt eq "#ifdef" ) {
      if ($thash{"$tag"} eq "$ST_OFF") {
        $thash{"$tag"} = "$ST_IF";
        print "state change $tag: $ST_OFF -> $ST_IF\n" if (exists $opts{v});
        $canwrite = iswritestate();
      }
      else {
        print "Error: $fromfilename: Found '#ifdef $tag' with state $thash{$tag}\n";
        return 0;
      }
    }
    elsif ( $copt eq "#ifndef" ) {
      if ($thash{"$tag"} eq "$ST_OFF") {
        # before changing to a non-write state (ifn) print #IFNDEF
        # line, if current state is a write state.
        print $TF "$line"  if ( $canwrite );
        $thash{"$tag"} = "$ST_IFN";
        print "state change $tag: $ST_OFF -> $ST_IFN\n" if (exists $opts{v});
        $canwrite = iswritestate();
      }
      else {
        print "Error: $fromfilename: Found '#ifndef $tag' with state $thash{$tag}\n";
        return 0;
      }
    }
    elsif ( $copt eq "#else" ) {
      if ($thash{"$tag"} eq "$ST_IF") {
        # before changing to a non-write state (else) print #else
        # line, if current state is a write state.
        print $TF "$line"  if ( $canwrite );
        $thash{"$tag"} = "$ST_ELSE";
        print "state change $tag: $ST_IF -> $ST_ELSE\n" if (exists $opts{v});
        $canwrite = iswritestate();
      }
      elsif ($thash{"$tag"} eq "$ST_IFN") {
        $thash{"$tag"} = "$ST_ELSEN";
        print "state change $tag: $ST_IFN -> $ST_ELSEN\n" if (exists $opts{v});
        $canwrite = iswritestate();
      }
      else {
        print "Error: $fromfilename: Found '#else (...) $tag' with state $thash{$tag}\n";
        return 0;
      }
    }
    elsif ( $copt eq "#endif" ) {
      if (($thash{"$tag"} eq "$ST_ELSE")  || ($thash{"$tag"} eq "$ST_IF") ||
          ($thash{"$tag"} eq "$ST_ELSEN") || ($thash{"$tag"} eq "$ST_IFN"))
      {
        print "state change $tag: $thash{$tag} -> $ST_OFF\n"
          if (exists $opts{v});
        $thash{"$tag"} = "$ST_OFF";
        $canwrite = iswritestate();
      }
      else {
        print "Error: Found '#endif (...) $tag' with state $thash{$tag}\n";
        return 0;
      }
    }

  } # foreach tag

  return 1;
} # checkifdef


sub parsefile {
  my($fname, $tname) = @_;
  my $FF; my $TF;
  my @fromfile = ();
  $canwrite = 1;

  # ignore file for file names
  if ( (exists $opts{i}) && ("$fname" =~  /$igstring/) ) {
    print "IGNORING $fname\n"  if ( exists $opts{v} );
    return 1;
  }

  print "Info: Opening '$fname'\n" if ( exists $opts{v} );
  if ( !(open($FF, "< $fname")) ) {
	print "Warning: unable to open input file, skipping: '$fname': $!\n";
	return 0;
  }

  my @fromfile = <$FF>;
  $FF->close();

  if ( !(open($TF, "> $tname")) ) {
	print "Warning: unable to open output file, skipping: '$tname': $!\n";
	return 0;
  }
  my $mode = (stat("$fname"))[2];
  if ($mode) { my $resp = chmod $mode, "$tname"; }

  my $line   = "";
  my @tout  = ();
  my $retval = 1;

  while ( $line = shift @fromfile ) {
    # check for any ifdef state changes
	if ( ! checkifdef($TF, $line, $fname) ) {
      $FF->close();
      $TF->close();
      die "Error: tag error in file \'$fname\', exiting\n";
    }

    if ( $canwrite ) {
	  print $TF "$line";
	}
	else {
	  print "Info: not copying: $fname: $line" if ( exists $opts{v} );
    }

  }

  if (! $canwrite) {
      print "End of $fname reached and we're not reset into 'canwrite' state\n";
  }
  $TF->close();

  return $retval;
} # parsefile


# note, fromdir should have already been checked to exist and be a
# directory
sub load_minamal_ifdefs {
    my @filelist = ();
    my $MF;
    
    if ( !(open($MF, "< $fromdir/$minimal_include_path")) ) {
        die "Unable to open main minimal feature file: '$fromdir/$minimal_include_path'\n";
    }
    my $line;
    # skip preceding lines
    while ( ($line = <$MF>) &&
            ($line !~ /^#else.*NETSNMP_FEATURE_CHECKING/ ) ) {
    }
    # grab the fetaure .h files
    while ( ($line = <$MF>) &&
            ($line !~ /^#endif.*NET_SNMP_FEATURE_CHECKING/) ) {
        if ($line =~ /include.*<(.*.h)>/) {
            push @filelist, $1;
        }
    }

    close($MF);

    while (my $fname = shift @filelist) {
        if ( !( -e "$fromdir/include/$fname" ) )  {
            print "Warn: feature file does not exist, skipping: '$fromdir/include/$fname'";
            next;
        }
        if ( !(open($MF, "< $fromdir/include/$fname")) ) {
            die "Unable to open minimal feature file: '$fromdir/include/$fname'\n";
        }
        while ( ($line = <$MF>) ) {
            if ( $line =~ /^#define.*(NETSNMP_FEATURE_REMOVE[^ ]+) / ) {
                $thash{"$1"} = $ST_OFF;
            }
        }
        close($MF);
    }
        
} # load_minamal_ifdefs




