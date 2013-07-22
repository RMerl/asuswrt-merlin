#!@PERL@ -w
#
# chkdupexe version 2.1.1
#
# Simple script to look for and list duplicate executables and dangling
# symlinks in the system executable directories.
#
# Copyright 1993 Nicolai Langfeldt. janl@math.uio.no
#  Distribute under gnu copyleft (included in perl package) 
#
# Modified 1995-07-04 Michael Shields <shields@tembel.org>
#     Don't depend on GNU ls.
#     Cleanups.
#     Merge together $ENV{'PATH'} and $execdirs.
#     Don't break if there are duplicates in $PATH.
#
# Modified 1996-02-16 Nicolai Langfeldt (janl@math.uio.no).
#     I was thinking admins would edit the $execdirs list to suit their
#     machine(s) when I wrote this.  This is ofcourse not the case, thus
#     Michaels fixes.  And my fixes to his :-)
#     - Working duplicate dirs detection.
#     - Added more checks
#     - Took out $PATH from the list of checked directories and added a
#	check for $execdirs and $PATH consistency instead
#     - Made it possible to run with perl -w

$execdirs='/bin /sbin /usr/bin /usr/sbin /usr/local/bin /usr/local/sbin '.
  '/usr/X11/bin /usr/bin/X11 /usr/local/X11/bin '.
  '/usr/TeX/bin /usr/tex/bin /usr/games '.
  '/usr/local/games';

# Turn off buffering for the output channel.
$|=1;

# Values from /usr/include/linux/errno.h.  Existence of linux/errno.ph is not
# something to count on... :-(
$ENOENT=2;

%didthis=();

foreach $dir (split(/\s+/, "$execdirs"), "\0", split(/:/, $ENV{PATH})) {

  if ($dir eq "\0") { $checkingpath = 1; next; }

  # It's like this: One directory corresponds to one $device,$inode tuple
  # If a symlink points to a directory we already checked that directory
  # will have the same $device,$inode tuple.

  # Does this directory have any real exstence outside the ravings of
  # symlinks pointing hither and dither?
  ($device,$inode)=stat($dir); 
  if (!defined($device)) {
    # Nonexistant directory, or dangling symlink?
    ($dum)=lstat($dir);
    next if $! == $ENOENT;
    if (!$dum) {
      print "Dangling symlink: $dir\n";
      next;
    }
    warn "Nonexistent directory: $dir\n" if ($checkingpath);
    next;
  }

  if (!-d _) {
    print "Not a directory: $dir\n";
    next;
  }

  next if defined($didthis{$device,$inode});

  $didthis{$device,$inode}=1;

  chdir($dir) || die "Could not chdir $dir: $!\n";
# This would give us the true directory name, do we want that?
#  chop($dir=`pwd`);
  opendir(DIR,".") || 
    die "NUTS! Personaly I think your perl or filesystem is broken.\n".
      "I've done all sorts of checks on $dir, and now I can't open it!\n";
  foreach $_ (readdir(DIR)) {
    lstat($_);
    if (-l _) {
      ($dum)=stat($_);
      print "Dangling symlink: $dir/$_\n" unless defined($dum);
      next;
    }
    next unless -f _ && -x _;	# Only handle regular executable files
    if (defined($count{$_})) {
      $progs{$_}.=" $dir/$_";
      $count{$_}++;
    } else {
      $progs{$_}="$dir/$_";
      $count{$_}=1;
    }
  }
  closedir(DIR);
}

open(LS,"| xargs -r ls -ldU");
while (($prog,$paths)=each %progs) {
  print LS "$paths\n" if ($count{$prog}>1);
}
close(LS);

exit 0;

@unchecked=();
# Check if the users PATH contains something I've not checked. The site admin
# might want to know about inconsistencies in user PATHs and chkdupexec 
# configuration
foreach $dir (split(/:/,$ENV{'PATH'})) {
  ($device,$inode)=stat($dir);
  next unless defined($device);
  next if defined($didthis{$device,$inode});
  push(@unchecked,$dir);
  $didthis{$device,$inode}=1;
}

print "Warning: Your path contains these directories which chkdupexe has not checked:\n",join(',',@unchecked),
  ".\nPlease review the execdirs list in chkdupexe.\n"
    if ($#unchecked>=$[);
