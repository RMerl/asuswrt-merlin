#! /usr/bin/env perl -w
    eval 'exec /usr/bin/env perl -S $0 ${1+"$@"}'
        if 0; #$running_under_some_shell

use strict;
use File::Find ();
use File::Path qw(mkpath);
use Cwd 'abs_path';

# Set the variable $File::Find::dont_use_nlink if you're using AFS,
# since AFS cheats.

# for the convenience of &wanted calls, including -eval statements:
use vars qw/*name *dir *prune/;
*name   = *File::Find::name;
*dir    = *File::Find::dir;
*prune  = *File::Find::prune;
my $builddir = abs_path($ENV{builddir});
my $srcdir = abs_path($ENV{srcdir});
sub wanted;



# Traverse desired filesystems
File::Find::find({wanted => \&wanted, no_chdir => 1}, $srcdir);
exit;


sub wanted {
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$newdir);

    if ((($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_)) &&
	(-d _) && (($newdir = abs_path($_)) !~ /$builddir/)) 
        { 
	  $newdir =~ s!$srcdir!$builddir!; 
	  mkpath($newdir);
	  print("Creating $newdir\n");
	}
}

