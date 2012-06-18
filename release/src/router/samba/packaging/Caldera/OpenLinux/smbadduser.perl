#!/usr/bin/perl -w
#
# smbadduser - Written by Mike Zakharoff
#              perl-rewrite by Raymund Will
#

$smbpasswd = "/etc/samba.d/smbpasswd";
$user_map  = "/etc/samba.d/smbusers";
#
# Set to site specific passwd command
#
$passwd    = "cat /etc/passwd |";
#$passwd    = "niscat passwd.org_dir |";
if ( -e "/var/run/ypbind.pid" ) {
  $passwd    = "(cat /etc/passwd; ypcat passwd ) |";
}

$line = "-" x 58;
if ($#ARGV < 0) {
  print <<EoM;
$line
Written: Mike Zakharoff email: michael.j.zakharoff\@boeing.com

   1) Updates $smbpasswd
   2) Updates $user_map
   3) Executes smbpasswd for each new user

smbadduser unixid[:ntid] unixid[:ntid] ...

Example: smbadduser zak:zakharoffm jdoe johns:smithj
$line
EoM
  exit 1;
}

# get valid UNIX-Ids (later skip missing)
%U = ();
{
  my $X = "X" x 32;
  my @t = ();
  
  open( IN, $passwd) || die( "ERROR: open($passwd): $!\n");
  while ( <IN> ) {
    next unless (/^[A-Za-z0-9_]+:/);
    @t = split(/:/);
    $U{$t[0]} = join( ":", ($t[0], $t[2], $X, $X, $t[4], $t[5], $t[6]));
  }
  close( IN);
}
# get all smb passwords (later skip already existent)
%S = ();
$Cs = "";
if ( -r $smbpasswd ) {
  open( IN, $smbpasswd) || die( "ERROR: open($smbpasswd): $!\n");
  while ( <IN> ) {
    if ( /^\#/ ) {
      $Cs .= $_;  next;
    } elsif ( ! /^([A-Za-z0-9_]+):/ ) {
      chop; print STDERR "ERROR: $_: invalid smbpasswd entry!\n"; next;
    }
    $S{$1} = $_;
  }
  close( IN);
}
# get all map entries
%M = ();
$Cm = "";
if ( -r $user_map ) {
  open( IN, $user_map) || die( "ERROR: open($user_map): $!\n");
  while ( <IN> ) {
    if ( /^\#/ ) {
      $Cm .= $_;  next;
    } elsif ( ! /^([A-Za-z0-9_]+)\s*=\s*(\S.+\S)\s*/ ) {
      chop; print STDERR "ERROR: $_: invalid user-map entry!\n"; next;
    }
    $M{$1} = $2;
  }
  close( IN);
}
# check parameter syntax
%N = ();
{
  foreach ( @ARGV ) {
    my ( $u, $s, @R) = split(/:/);
    if (  $#R >= 0 ) {
      print STDERR "ERROR: $_: Must use unixid[:ntid] SKIPPING...\n";
      next;
    }
    $s = $u unless ( defined( $s) );
    if ( ! exists( $U{$u}) ) {
      print STDERR "ERROR: $u: Not in passwd database SKIPPING...\n";
      next;
    }
    if ( exists( $S{$u}) ) {
      print STDERR "ERROR: $u: Already in smbpasswd database SKIPPING...\n";
      next;
    }
    print  "Adding: $u to $smbpasswd\n";
    $S{$u} = $U{$u};
    if ( $u ne $s ) {
      if ( exists( $M{$u}) ) {
	if ( $M{$u} !~ /\b$s\b/ ) {
	  print "Adding: $s to $u in $user_map\n";
	  $M{$u} .= " $s";
	}
      } else {
	print "Mapping: $s to $u in $user_map\n";
	$M{$u} = $s;
      }
    }
    $N{$u} = $s;
  }
}
# rewrite $smbpasswd
{
  open( OUT, "> $smbpasswd.new")  || die( "ERROR: open($smbpasswd.new): $!\n");
  $Cs = "#\n# SMB password file.\n#\n" unless ( $Cs );
  print OUT $Cs;
  foreach ( sort( keys( %S)) ) {
    print OUT $S{$_};
  }
  close( OUT);
  rename( $smbpasswd, $smbpasswd . "-");
  rename( $smbpasswd . ".new", $smbpasswd) || die;
}
# rewrite $user_map
{
  open( OUT, "> $user_map.new")  || die( "ERROR: open($user_map.new): $!\n");
  $Cm = "# Unix_name = SMB_name1 SMB_name2 ...\n" unless ( $Cm );
  print OUT $Cm;
  foreach ( sort( keys( %M)) ) {
    print OUT "$_ = $M{$_}\n";
  }
  close( OUT);
  rename( $user_map, $user_map . "-");
  rename( $user_map . ".new", $user_map) || die;
}
# call 'smbpasswd' for each new
{
  foreach ( sort( keys( %N)) ) {
    print $line . "\n";
    print "ENTER password for $_\n";
    system( "smbpasswd $_");
  }
}

