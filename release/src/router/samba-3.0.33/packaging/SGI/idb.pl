#!/usr/bin/perl
require "pwd.pl" || die "Required pwd.pl not found";

# This perl script automatically generates the idb file

$PKG = 'samba';
$SRCDIR = '../..';
$SRCPFX = '.';

&initpwd;
$curdir = $ENV{"PWD"};

if ($PKG eq "samba_irix") {
  open(BOOKS,"IDB.books") || die "Unable to open IDB.books file\n";
  @books = sort idbsort <BOOKS>;
  close BOOKS;
}

# We don't want the files listed in .cvsignore in the source tree
open(IGNORES,"$SRCDIR/source/.cvsignore") || die "Unable to open .cvsignore file\n";
while (<IGNORES>) {
  chop;
  next if /cvs\.log/;
  $ignores{$_}++;
}
close IGNORES;

# We don't want the files listed in .cvsignore in the source/include tree
open(IGNORES,"$SRCDIR/source/include/.cvsignore") || die "Unable to open include/.cvsignore file\n";
while (<IGNORES>) {
  chop;
  $ignores{$_}++;
}
close IGNORES;

# get the names of all the binary files to be installed
open(MAKEFILE,"$SRCDIR/source/Makefile") || die "Unable to open Makefile\n";
while (not eof(MAKEFILE)) {
  $_ = <MAKEFILE>;
  chomp;
  last if /^# object file lists/ ;
  if (/^EXEEXT/) {
    /^.*=(.*)/;
    $EXEEXT = $1;
  }
  if (/^srcdir/) {
    /^.*=(.*)/;
    $srcdir = $1;
  }
  if (/^builddir/) {
    /^.*=(.*)/;
    $builddir = $1;
  }
  if (/^SBIN_PROGS/) { @sbinprogs = get_line($_); }
  if (/^BIN_PROGS1/) { @binprogs1 = get_line($_); }
  if (/^BIN_PROGS2/) { @binprogs2 = get_line($_); }
  if (/^BIN_PROGS3/) { @binprogs3 = get_line($_); }
  if (/^BIN_PROGS/) { @binprogs = get_line($_); }
  if (/^SCRIPTS/) { @scripts = get_line($_); }
  if (/^TORTURE_PROGS/) { @tortureprogs = get_line($_); }
  if (/^SHLIBS/) { @shlibs = get_line($_); }
}
close MAKEFILE;

# add my local files to the list of binaries to install
@bins = sort byfilename (@sbinprogs,@binprogs,@binprogs1,@binprogs2,@binprogs3,@scripts,("sambalp","smbprint"));

@nsswitch = sort byfilename (@shlibs);

# install the swat files
chdir "$SRCDIR/source";
system("chmod +x ./script/installswat.sh");
system("./script/installswat.sh  ../packaging/SGI/swat ./ ../packaging/SGI/swat/using_samba");
system("cp -f ../swat/README ../packaging/SGI/swat");
chdir $curdir;

# get a complete list of all files in the tree
chdir "$SRCDIR/";
&dodir('.');
chdir $curdir;

# the files installed in docs include all the original files in docs plus all
# the "*.doc" files from the source tree
@docs = sort bynextdir grep (!/CVS/ & (/^source\/.*\.doc$/ | /^docs\/\w/),@allfiles);
@docs = grep(!/htmldocs/ & !/manpages/, @docs);
@docs = grep(!/docbook/, @docs);

@libfiles = sort byfilename (grep (/^source\/codepages\/\w/,@allfiles),("packaging/SGI/smb.conf","source/bin/libsmbclient.a","source/bin/libsmbclient.so"));

@swatfiles = sort grep(/^packaging\/SGI\/swat/, @allfiles);
@catman = sort grep(/^packaging\/SGI\/catman/ & !/\/$/, @allfiles);
@catman = sort bydirnum @catman;

# strip out all the generated directories and the "*.o" files from the source
# release
@allfiles = grep(!/^.*\.o$/ & !/^.*\.po$/ & !/^.*\.po32$/ & !/^.*\.so$/ & !/^source\/bin/ & !/^packaging\/SGI\/bins/ & !/^packaging\/SGI\/catman/ & !/^packaging\/SGI\/html/ & !/^packaging\/SGI\/codepages/ & !/^packaging\/SGI\/swat/, @allfiles);

open(IDB,"> $curdir/$PKG.idb") || die "Unable to open $PKG.idb for output\n";

print IDB "f 0644 root sys etc/config/samba $SRCPFX/packaging/SGI/samba.config $PKG.sw.base config(update)\n";
print IDB "f 0644 root sys etc/config/winbind $SRCPFX/packaging/SGI/winbindd.config $PKG.sw.base config(update)\n";
print IDB "f 0755 root sys etc/init.d/samba $SRCPFX/packaging/SGI/samba.rc $PKG.sw.base\n";
print IDB "f 0755 root sys etc/init.d/winbind $SRCPFX/packaging/SGI/winbindd.rc $PKG.sw.base\n";
print IDB "l 0000 root sys etc/rc0.d/K36winbind $SRCPFX/packaging/SGI $PKG.sw.base symval(../init.d/winbind)\n";
print IDB "l 0000 root sys etc/rc0.d/K37samba $SRCPFX/packaging/SGI $PKG.sw.base symval(../init.d/samba)\n";
print IDB "l 0000 root sys etc/rc2.d/S81samba $SRCPFX/packaging/SGI $PKG.sw.base symval(../init.d/samba)\n";
print IDB "l 0000 root sys etc/rc2.d/S82winbind $SRCPFX/packaging/SGI $PKG.sw.base symval(../init.d/winbind)\n";

if ($PKG eq "samba_irix") {
  print IDB "d 0755 root sys usr/relnotes/samba_irix $SRCPFX/packaging/SGI $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/TC relnotes/TC $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/ch1.z relnotes/ch1.z $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/ch2.z relnotes/ch2.z $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/ch3.z relnotes/ch3.z $PKG.man.relnotes\n";
}
else {
  @copyfile = grep (/^COPY/,@allfiles);
  print IDB "d 0755 root sys usr/relnotes/samba $SRCPFX/packaging/SGI $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba/@copyfile[0] $SRCPFX/@copyfile[0] $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba/legal_notice.html $SRCPFX/packaging/SGI/legal_notice.html $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba/samba-relnotes.html $SRCPFX/packaging/SGI/relnotes.html $PKG.man.relnotes\n";
}

print IDB "d 0755 root sys usr/samba $SRCPFX/packaging/SGI $PKG.sw.base\n";

print IDB "d 0755 root sys usr/samba/bin $SRCPFX/packaging/SGI $PKG.sw.base\n";
while(@bins) {
  $nextfile = shift @bins;
  ($filename = $nextfile) =~ s/^.*\///;;

  if (index($nextfile,'$')) {
    if ($filename eq "smbpasswd") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base \n";
    }
    elsif ($filename eq "swat") {
      print IDB "f 4755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base preop(\"chroot \$rbase /etc/init.d/samba stop\") exitop(\"chroot \$rbase /usr/samba/scripts/startswat.sh\") removeop(\"chroot \$rbase /sbin/cp /etc/inetd.conf /etc/inetd.conf.O ; chroot \$rbase /sbin/sed -e '/^swat/D' -e '/^#SWAT/D' /etc/inetd.conf.O >/etc/inetd.conf; /etc/killall -HUP inetd || true\")\n";
    }
    elsif ($filename eq "sambalp") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/packaging/SGI/$filename $PKG.sw.base \n";
    }
    elsif ($filename eq "smbprint") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/packaging/SGI/$filename $PKG.sw.base\n";
    }
    elsif ($filename eq "smbd") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base \n";
      if (-e "$SRCDIR/source/$nextfile.noquota") {
	print IDB "f 0755 root sys usr/samba/bin/$filename.noquota $SRCPFX/source/$nextfile.noquota $PKG.sw.base \n";
      }
      if (-e "$SRCDIR/source/$nextfile.profile") {
	print IDB "f 0755 root sys usr/samba/bin/$filename.profile $SRCPFX/source/$nextfile.profile $PKG.sw.base \n";
      }
    }
    elsif ($filename eq "nmbd") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base \n";
      if (-e "$SRCDIR/source/$nextfile.profile") {
	print IDB "f 0755 root sys usr/samba/bin/$filename.profile $SRCPFX/source/$nextfile.profile $PKG.sw.base \n";
      }
    }
    else {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base \n";
    }
  }
}

print IDB "d 0755 root sys usr/samba/docs $SRCPFX/docs $PKG.man.doc\n";
while (@docs) {
  $nextfile = shift @docs;
  ($junk,$file) = split(/\//,$nextfile,2);
  if (grep(/\/$/,$nextfile)) {
    $file =~ s/\/$//;
    $nextfile =~ s/\/$//;
    print IDB "d 0755 root sys usr/samba/docs/$file $SRCPFX/$nextfile $PKG.man.doc\n";
  }
  else {
    print IDB "f 0644 root sys usr/samba/docs/$file $SRCPFX/$nextfile $PKG.man.doc\n";
  }
}

print IDB "d 0755 root sys usr/samba/include $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "f 0644 root sys usr/samba/include/libsmbclient.h $SRCPFX/source/include/libsmbclient.h $PKG.sw.base\n";

print IDB "d 0755 root sys usr/samba/lib $SRCPFX/packaging/SGI $PKG.sw.base\n";
while (@libfiles) {
  $nextfile = shift @libfiles;
  ($file = $nextfile) =~ s/.*\///;
  if ($file eq "smb.conf") {
    print IDB "f 0644 root sys usr/samba/lib/$file $SRCPFX/$nextfile $PKG.sw.base config(suggest)\n";
  } else {
    print IDB "f 0644 root sys usr/samba/lib/$file $SRCPFX/$nextfile $PKG.sw.base nostrip \n";
  }
}

print IDB "d 0755 lp sys usr/samba/printer $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 lp sys usr/samba/printer/W32ALPHA $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 lp sys usr/samba/printer/W32MIPS $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 lp sys usr/samba/printer/W32PPC $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 lp sys usr/samba/printer/W32X86 $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 lp sys usr/samba/printer/WIN40 $SRCPFX/packaging/SGI $PKG.sw.base\n";

print IDB "d 0644 root sys usr/samba/private $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "f 0600 root sys usr/samba/private/smbpasswd $SRCPFX/packaging/SGI/smbpasswd $PKG.sw.base config(suggest)\n";

print IDB "d 0755 root sys usr/samba/scripts $SRCPFX/packaging/SGI $PKG.src.samba\n";
print IDB "f 0755 root sys usr/samba/scripts/inetd.sh $SRCPFX/packaging/SGI/inetd.sh $PKG.sw.base\n";
print IDB "f 0755 root sys usr/samba/scripts/inst.msg $SRCPFX/packaging/SGI/inst.msg $PKG.sw.base exitop(\"chroot \$rbase /usr/samba/scripts/inst.msg\")\n";
print IDB "f 0755 root sys usr/samba/scripts/mkprintcap.sh $SRCPFX/packaging/SGI/mkprintcap.sh $PKG.sw.base\n";
print IDB "f 0755 root sys usr/samba/scripts/removeswat.sh $SRCPFX/packaging/SGI/removeswat.sh $PKG.sw.base\n";
print IDB "f 0755 root sys usr/samba/scripts/startswat.sh $SRCPFX/packaging/SGI/startswat.sh $PKG.sw.base\n";

print IDB "d 0755 root sys usr/samba/src $SRCPFX/packaging/SGI $PKG.src.samba\n";
@sorted = sort(@allfiles);
while (@sorted) {
  $nextfile = shift @sorted;
  ($file = $nextfile) =~ s/^.*\///;
  next if grep(/packaging\/SGI/& (/Makefile/ | /samba\.spec/ | /samba\.idb/),$nextfile);
  next if grep(/source/,$nextfile) && ($ignores{$file});
  next if ($nextfile eq "CVS");
  if (grep(/\/$/,$nextfile)) {
    $nextfile =~ s/\/$//;
    print IDB "d 0755 root sys usr/samba/src/$nextfile $SRCPFX/$nextfile $PKG.src.samba\n";
  }
  else {
    if (grep((/\.sh$/ | /configure$/ | /configure\.developer/ | /config\.guess/ | /config\.sub/ | /\.pl$/ | /mkman$/ | /pcp\/Install/ | /pcp\/Remove/),$nextfile)) {
	print IDB "f 0755 root sys usr/samba/src/$nextfile $SRCPFX/$nextfile $PKG.src.samba\n";
    }
    else {
        print IDB "f 0644 root sys usr/samba/src/$nextfile $SRCPFX/$nextfile $PKG.src.samba\n";
    }
  }
}

print IDB "d 0755 root sys usr/samba/swat $SRCPFX/packaging/SGI/swat $PKG.sw.base\n";
while (@swatfiles) {
  $nextfile = shift @swatfiles;
  ($file = $nextfile) =~ s/^packaging\/SGI\/swat\///;
  next if !$file;
  if (grep(/\/$/,$file)) {
    $file =~ s/\/$//;
    print IDB "d 0755 root sys usr/samba/swat/$file $SRCPFX/packaging/SGI/swat/$file $PKG.sw.base\n";
  }
  else {
    print IDB "f 0444 root sys usr/samba/swat/$file $SRCPFX/packaging/SGI/swat/$file $PKG.sw.base\n";
  }
}

print IDB "d 0755 root sys usr/samba/var $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 root sys usr/samba/var/locks $SRCPFX/packaging/SGI $PKG.sw.base\n";

if ($PKG eq "samba_irix") {
  while(@books) {
    $nextfile = shift @books;
    print IDB $nextfile;
  }
}

print IDB "d 0755 root sys usr/share/catman/u_man $SRCPFX/packaging/SGI $PKG.man.manpages\n";
$olddirnum = "0";
while (@catman) {
  $nextfile = shift @catman;
  ($file = $nextfile) =~ s/^packaging\/SGI\/catman\///;
  ($dirnum = $file) =~ s/^[\D]*//;
  $dirnum =~ s/\.z//;
  if ($dirnum ne $olddirnum) {
    print IDB "d 0755 root sys usr/share/catman/u_man/cat$dirnum $SRCPFX/packaging/SGI $PKG.man.manpages\n";
    $olddirnum = $dirnum;
  }
  print IDB "f 0664 root sys usr/share/catman/u_man/cat$dirnum/$file $SRCPFX/$nextfile $PKG.man.manpages\n";
}

if (@nsswitch) {
  print IDB "d 0755 root sys var/ns/lib $SRCPFX/packaging/SGI $PKG.sw.base\n";
  while(@nsswitch) {
    $nextfile = shift @nsswitch;
    next if $nextfile eq 'libsmbclient';
    ($filename = $nextfile) =~ s/^.*\///;
    $filename =~ s/libnss/libns/;
    print IDB "f 0644 root sys var/ns/lib/$filename $SRCPFX/source/$nextfile $PKG.sw.base \n";
  }
}

print IDB "d 01777 lp sys var/spool/samba $SRCPFX/packaging/SGI $PKG.sw.base\n";

close IDB;
print "\n\n$PKG.idb file has been created\n";

sub dodir {
    local($dir, $nlink) = @_;
    local($dev,$ino,$mode,$subcount);

    ($dev,$ino,$mode,$nlink) = stat('.') unless $nlink;

    opendir(DIR,'.') || die "Can't open current directory";
    local(@filenames) = sort readdir(DIR);
    closedir(DIR);

    if ($nlink ==2) {		# This dir has no subdirectories.
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $this =  substr($dir,2)."/$_";
	    push(@allfiles,$this);
	}
    }
    else {
	$subcount = $nlink -2;
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    next if $_ eq 'CVS';
	    ($dev,$ino,$mode,$nlink) = lstat($_);
	    $name = "$dir/$_";
	    $this = substr($name,2);
	    $this .= '/' if -d;
	    push(@allfiles,$this);
	    next if $subcount == 0;		# seen all the subdirs?

	    next unless -d _;

	    chdir $_ || die "Can't cd to $name";
	    &dodir($name,$nlink);
	    chdir '..';
	    --$subcount;
	}
    }
}

sub bynextdir {
  ($f0,$f1) = split(/\//,$a,2);
  ($f0,$f2) = split(/\//,$b,2);
  $f1 cmp $f2;
}

sub byfilename {
  ($f0,$f1) = split(/.*\//,$a,2);
  if ($f1 eq "") { $f1 = $f0 };
  ($f0,$f2) = split(/.*\//,$b,2);
  if ($f2 eq "") { $f2 = $f0 };
  $f1 cmp $f2;
}

sub bydirnum {
  ($f1 = $a) =~ s/^.*\///;
  ($f2 = $b) =~ s/^.*\///;
  ($dir1 = $a) =~ s/^[\D]*//;
  ($dir2 = $b) =~ s/^[\D]*//;
  if (!($dir1 <=> $dir2)) {
    $f1 cmp $f2;
  }
  else {
    $dir1 <=> $dir2;
  }
}

sub idbsort {
  ($f0,$f1,$f2,$f3) = split(/ /,$a,4);
  ($f0,$f1,$f2,$f4) = split(/ /,$b,4);
  $f3 cmp $f4;
}

sub get_line {
  local ($line) = @_;

  $line =~ s/^.*=//;
  while (($cont = index($line,"\\")) > 0) {
    $_ = <MAKEFILE>;
    chomp;
    s/^\s*/ /;
    substr($line,$cont,1) = $_;
  }
  $line =~ s/\$\(EXEEXT\)/$EXEEXT/g;
  $line =~ s/\$\(srcdir\)//g;
  $line =~ s/\$\(builddir\)//g;
  $line =~ s/\$\(\S*\)\s*//g;
  $line =~ s/\s\s*/ /g;
  @line = split(' ',$line);
  return @line;
}

