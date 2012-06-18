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
@makefile = <MAKEFILE>;
@sprogs = grep(/^SPROGS /,@makefile);
@progs1 = grep(/^PROGS1 /,@makefile);
@progs2 = grep(/^PROGS2 /,@makefile);
@mprogs = grep(/^MPROGS /,@makefile);
@progs = grep(/^PROGS /,@makefile);
@scripts = grep(/^SCRIPTS /,@makefile);
@codepagelist = grep(/^CODEPAGELIST/,@makefile);
close MAKEFILE;

if (@sprogs) {
  @sprogs[0] =~ s/^.*\=//;
  @sprogs = split(' ',@sprogs[0]);
}
if (@progs) {
  @progs[0] =~ s/^.*\=//;
  @progs[0] =~ s/\$\(\S+\)\s//g;
  @progs = split(' ',@progs[0]);
}
if (@mprogs) {
  @mprogs[0] =~ s/^.*\=//;
  @mprogs = split(' ',@mprogs[0]);
}
if (@progs1) {
  @progs1[0] =~ s/^.*\=//;
  @progs1 = split(' ',@progs1[0]);
}
if (@progs2) {
  @progs2[0] =~ s/^.*\=//;
  @progs2 = split(' ',@progs2[0]);
}
if (@scripts) {
  @scripts[0] =~ s/^.*\=//;
  @scripts[0] =~ s/\$\(srcdir\)\///g;
  @scripts = split(' ',@scripts[0]);
}

# we need to create codepages for the package
@codepagelist[0] =~ s/^.*\=//;
chdir "$SRCDIR/source";
system("chmod +x ./script/installcp.sh");
system("./script/installcp.sh . . ../packaging/SGI/codepages ./bin @codepagelist[0]");
chdir $curdir;
opendir(DIR,"$SRCDIR/packaging/SGI/codepages") || die "Can't open codepages directory";
@codepage = sort readdir(DIR);
closedir(DIR);

# install the swat files
chdir "$SRCDIR/source";
system("chmod +x ./script/installswat.sh");
system("./script/installswat.sh  ../packaging/SGI/swat ./");
system("cp -f ../swat/README ../packaging/SGI/swat");
chdir $curdir;

# add my local files to the list of binaries to install
@bins = sort byfilename (@sprogs,@progs,@progs1,@progs2,@mprogs,@scripts,("/findsmb","/sambalp","/smbprint"));

# get a complete list of all files in the tree
chdir "$SRCDIR/";
&dodir('.');
chdir $curdir;

# the files installed in docs include all the original files in docs plus all
# the "*.doc" files from the source tree
@docs = sort byfilename grep (!/^docs\/$/ & (/^source\/.*\.doc$/ | /^docs\//),@allfiles);

@swatfiles = sort grep(/^packaging\/SGI\/swat/, @allfiles);
@catman = sort grep(/^packaging\/SGI\/catman/ & !/\/$/, @allfiles);
@catman = sort bydirnum @catman;

# strip out all the generated directories and the "*.o" files from the source
# release
@allfiles = grep(!/^.*\.o$/ & !/^.*\.po$/ & !/^.*\.po32$/ & !/^source\/bin/ & !/^packaging\/SGI\/bins/ & !/^packaging\/SGI\/catman/ & !/^packaging\/SGI\/html/ & !/^packaging\/SGI\/codepages/ & !/^packaging\/SGI\/swat/, @allfiles);

open(IDB,"> $curdir/$PKG.idb") || die "Unable to open $PKG.idb for output\n";

print IDB "f 0644 root sys etc/config/samba $SRCPFX/packaging/SGI/samba.config $PKG.sw.base config(update)\n";
print IDB "f 0755 root sys etc/init.d/samba $SRCPFX/packaging/SGI/samba.rc $PKG.sw.base\n";
print IDB "l 0000 root sys etc/rc0.d/K39samba $SRCPFX/packaging/SGI $PKG.sw.base symval(../init.d/samba)\n";
print IDB "l 0000 root sys etc/rc2.d/S81samba $SRCPFX/packaging/SGI $PKG.sw.base symval(../init.d/samba)\n";

if ($PKG eq "samba_irix") {
  print IDB "d 0755 root sys usr/relnotes/samba_irix $SRCPFX/packaging/SGI $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/TC build/TC $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/ch1.z build/ch1.z $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba_irix/ch2.z build/ch2.z $PKG.man.relnotes\n";
}
else {
  @copyfile = grep (/^COPY/,@allfiles);
  print IDB "d 0755 root sys usr/relnotes/samba $SRCPFX/packaging/SGI $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba/@copyfile[0] $SRCPFX/@copyfile[0] $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba/legal_notice.html $SRCPFX/packaging/SGI/legal_notice.html $PKG.man.relnotes\n";
  print IDB "f 0644 root sys usr/relnotes/samba/samba-relnotes.html $SRCPFX/packaging/SGI/relnotes.html $PKG.man.relnotes\n";
}

print IDB "d 0755 root sys usr/samba $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "f 0444 root sys usr/samba/README $SRCPFX/packaging/SGI/README $PKG.sw.base\n";

print IDB "d 0755 root sys usr/samba/bin $SRCPFX/packaging/SGI $PKG.sw.base\n";
while(@bins) {
  $nextfile = shift @bins;
  ($filename = $nextfile) =~ s/^.*\///;;

  if (index($nextfile,'$')) {
    if ($filename eq "smbpasswd") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base nostrip\n";
    }
    elsif ($filename eq "findsmb") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/packaging/SGI/$filename $PKG.sw.base\n";
    }
    elsif ($filename eq "swat") {
      print IDB "f 4755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base nostrip preop(\"chroot \$rbase /etc/init.d/samba stop\") exitop(\"chroot \$rbase /usr/samba/scripts/startswat.sh\") removeop(\"chroot \$rbase /sbin/cp /etc/inetd.conf /etc/inetd.conf.O ; chroot \$rbase /sbin/sed -e '/^swat/D' -e '/^#SWAT/D' /etc/inetd.conf.O >/etc/inetd.conf; /etc/killall -HUP inetd || true\")\n";
    }
    elsif ($filename eq "sambalp") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/packaging/SGI/$filename $PKG.sw.base nostrip\n";
    }
    elsif ($filename eq "smbprint") {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/packaging/SGI/$filename $PKG.sw.base\n";
    }
    else {
      print IDB "f 0755 root sys usr/samba/bin/$filename $SRCPFX/source/$nextfile $PKG.sw.base nostrip\n";
    }
  }
}

print IDB "d 0755 root sys usr/samba/docs $SRCPFX/docs $PKG.man.doc\n";
while (@docs) {
  $nextfile = shift @docs;
  next if ($nextfile eq "CVS");
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

print IDB "d 0755 root sys usr/samba/lib $SRCPFX/packaging/SGI $PKG.sw.base\n";
print IDB "d 0755 root sys usr/samba/lib/codepages $SRCPFX/packaging/SGI $PKG.sw.base\n";
while (@codepage) {
  $nextpage = shift @codepage;
  print IDB "f 0644 root sys usr/samba/lib/codepages/$nextpage $SRCPFX/packaging/SGI/codepages/$nextpage $PKG.sw.base\n";
}
print IDB "f 0644 root sys usr/samba/lib/smb.conf $SRCPFX/packaging/SGI/smb.conf $PKG.sw.base config(suggest)\n";

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
    if (grep((/\.sh$/ | /configure$/ | /configure\.developer/ | /config\.guess/ | /config\.sub/ | /\.pl$/ | /mkman$/),$nextfile)) {
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
print IDB "f 0644 root sys usr/samba/var/locks/STATUS..LCK $SRCPFX/packaging/SGI/STATUS..LCK $PKG.sw.base\n";

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
print IDB "d 01777 nobody nobody var/spool/samba $SRCPFX/packaging/SGI $PKG.sw.base\n";

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

sub byfilename {
  ($f0,$f1) = split(/\//,$a,2);
  ($f0,$f2) = split(/\//,$b,2);
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

