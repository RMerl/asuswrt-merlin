#!/usr/bin/perl
#
#	libfoo.pl
#	Copyright (C) 2006-2008 Jonathan Zarate
#
#	- strip un-needed objects
#	- create xref of symbols used
#

$root = $ENV{"TARGETDIR"};
$uclibc = $ENV{"TOOLCHAIN"};
$router = $ENV{"SRCBASE"} . "/router";

sub error
{
	print STDERR "\n*** ERROR: " . (shift) . "\n\n";
	exit 1;
}

sub basename
{
	my $fn = shift;
	if ($fn =~ /([^\/]+)$/) {
		return $1;
	}
	return $fn;
}

sub load
{
    my $fname = shift;

	if ((-l $fname) ||
		($fname =~ /\/lib\/modules\/\d+\.\d+\.\d+/) ||
		($fname =~ /\.(asp|gif|png|svg|js|jsx|css|txt|pat|sh)$/)) {
		return;
	}
	
	if (-d $fname) {
		my $d;
		if (opendir($d, $fname)) {
			foreach (readdir($d)) {
				if ($_ !~ /^\./) {
					load($fname . "/" . $_);
				}
			}
			closedir($d);
		}
		return;
	}


	my $f;
	my $base;
	my $ok;
	my $s;

	$base = basename($fname);
	print LOG "\n\nreadelf $base:\n";
	
	open($f, "arm-linux-readelf -WhsdD ${fname} 2>&1 |") || error("readelf - $!\n");
	
	while (<$f>) {
		print LOG;

		if (/\s+Type:\s+(\w+)/) {
			$elf_type{$base} = $1;
			$ok = 1;
			last;
		}
	}
	
	if (!$ok) {
		close($f);
		return;
	}

	print "$elf_type{$base} $base", " " x 30, "\r";
	
	push(@elfs, $base);
	
	while (<$f>) {
		print LOG;
		
		if (/\(NEEDED\)\s+Shared library: \[(.+)\]/) {
			push(@{$elf_lib{$base}}, $1);
		}
		elsif (/Symbol table for image:/) {
			last;
		}
	}
	
	while (<$f>) {
		print LOG;

		if (/\s+(WEAK|GLOBAL)\s+(?:DEFAULT|VISIBLE)\s+(\w+)\s+(\w+)/) {
			$s = $3;
			if ($2 eq 'UND') {
				if ($1 eq 'GLOBAL') {
					$elf_ext{$base}{$s} = 1;
				}
				else {
					print LOG "*** not GLOBAL\n";
				}
			}
			elsif ($2 eq 'ABS') {
			}
			elsif ($2 =~ /^\d+$/) {
				$elf_exp{$base}{$s} = 1;
			}
			else {
				print LOG "*** unknown type\n";
			}
		}
		elsif (!/Num Buc:/) {
			print LOG "*** strange line\n";
		}
	}

	close($f);
}

sub fixDynDep
{
	my ($user, $dep) = @_;
	
	if (!defined $elf_dyn{$user}{$dep}) {
		push(@{$elf_lib{$user}}, $dep);
		$elf_dyn{$user}{$dep} = 1;

		print LOG "FixDynDep: $user = $dep\n";
	}
}

sub fixDyn
{
	my $s;

	foreach (@elfs) {
		if (/^libipt_.+\.so$/) {
			fixDynDep("iptables", $_);
		}
		elsif (/^libip6t_.+\.so$/) {
			fixDynDep("ip6tables", $_);
		}
		elsif (/^CP\d+\.so$/) {
			fixDynDep("smbd", $_);
		}
	}

	fixDynDep("l2tpd", "cmd.so");
	fixDynDep("l2tpd", "sync-pppd.so");
	fixDynDep("pppd", "pppol2tp.so");
	fixDynDep("pppd", "pptp.so");
	fixDynDep("pppd", "rp-pppoe.so");
	fixDynDep("libcrypto.so.1.0.0", "libssl.so.1.0.0");
## charles add
#	    fixDynDep("libsmbclient.so.0", "libpthread.so.0");
	fixDynDep("mod_smbdav.so", "libshared.so");
	fixDynDep("mod_smbdav.so", "libnvram.so");
	fixDynDep("mod_smbdav.so", "libsqlite3.so.0");
    fixDynDep("mod_smbdav.so", "libxml2.so.2");
	fixDynDep("mod_smbdav.so", "libsmbclient.so.0");
	fixDynDep("mod_smbdav.so", "libpthread.so.0");
	   fixDynDep("lighttpd", "libpthread.so.0");
	   fixDynDep("lighttpd-arpping", "libpthread.so.0");
	   fixDynDep("lighttpd-arpping", "libsmbclient.so.0");

	fixDynDep("lighttpd", "libcrypto.so.1.0.0");
	fixDynDep("lighttpd", "libssl.so.1.0.0");
        fixDynDep("lighttpd", "libpcre.so.0.0.1");
        fixDynDep("lighttpd", "mod_accesslog.so");
        fixDynDep("lighttpd", "mod_alias.so");
        fixDynDep("lighttpd", "mod_auth.so");
        fixDynDep("lighttpd", "mod_cml.so");
        fixDynDep("lighttpd", "mod_cgi.so");
        fixDynDep("lighttpd", "mod_compress.so");
        fixDynDep("lighttpd", "mod_dirlisting.so");
        fixDynDep("lighttpd", "mod_proxy.so");
        fixDynDep("lighttpd", "mod_redirect.so");
        fixDynDep("lighttpd", "mod_rewrite.so");
        fixDynDep("lighttpd", "mod_ssi.so");
        fixDynDep("lighttpd", "mod_staticfile.so");
        fixDynDep("lighttpd", "mod_status.so");
        fixDynDep("lighttpd", "mod_trigger_b4_dl.so");
        fixDynDep("lighttpd", "mod_userdir.so");
        fixDynDep("lighttpd", "mod_webdav.so");
        fixDynDep("lighttpd", "mod_usertrack.so");
        fixDynDep("lighttpd", "mod_ssi.so");
        fixDynDep("lighttpd", "mod_scgi.so");
        fixDynDep("lighttpd", "mod_access.so");
        fixDynDep("lighttpd", "mod_evasive.so");
        fixDynDep("lighttpd", "mod_evhost.so");
        fixDynDep("lighttpd", "mod_expire.so");
        fixDynDep("lighttpd", "mod_extforward.so");
        fixDynDep("lighttpd", "mod_fastcgi.so");
        fixDynDep("lighttpd", "mod_flv_streaming.so");
        fixDynDep("lighttpd", "mod_indexfile.so");
        fixDynDep("lighttpd", "mod_magnet.so");
        fixDynDep("lighttpd", "mod_mysql_vhost.so");
        fixDynDep("lighttpd", "mod_rrdtool.so");
        fixDynDep("lighttpd", "mod_secdownload.so");
        fixDynDep("lighttpd", "mod_setenv.so");
        fixDynDep("lighttpd", "mod_simple_vhost.so");
        fixDynDep("mod_webdav.so", "libsqlite3.so.0");
        fixDynDep("mod_webdav.so", "libxml2.so.2");
## charles add
		fixDynDep("lighttpd", "mod_smbdav.so");
        fixDynDep("lighttpd", "libshared.so");
        fixDynDep("lighttpd", "libnvram.so");
		fixDynDep("lighttpd", "libxml2.so.2");
		fixDynDep("libbcm.so", "libshared.so");
		fixDynDep("libbcm.so", "libc.so.0");
#   fixDynDep("lighttpd", "libdisk.so");
        fixDynDep("lighttpd", "mod_smbdav_access.so");
        fixDynDep("mod_smbdav_access.so", "libdisk.so");

#!!TB - Updated Broadcom WL driver
	fixDynDep("libbcmcrypto.so", "libc.so.0");
	fixDynDep("nas", "libbcmcrypto.so");
	fixDynDep("wl", "libbcmcrypto.so");
	fixDynDep("nas", "libc.so.0");
	fixDynDep("wl", "libc.so.0");
}

sub usersOf
{
	my $name = shift;
	my $sym = shift;
	my @x;
	my $e;
	my $l;
	
	@x = ();
	foreach $e (@elfs) {
		foreach $l (@{$elf_lib{$e}}) {
			if ($l eq $name) {
				if ((!defined $sym) || (defined $elf_ext{$e}{$sym})) {
					push(@x, $e);
				}
				last;
			}
		}
	}
	return @x;
}

sub resolve
{
	my $name = shift;
	my $sym = shift;
	my $l;
	
	foreach $l (@{$elf_lib{$name}}) {
#		print "\n$l $sym ", $elf_exp{$l}{$sym}, "\n";
		return $l if (defined $elf_exp{$l}{$sym});
	}
	return "*** unresolved ***";
}

sub fillGaps
{
	my $name;
	my $sym;
	my @users;
	my $u;
	my $t;
	my $found;

#	print "Resolving implicit links...\n";
	
	foreach $name (@elfs) {
		foreach $sym (keys %{$elf_ext{$name}}) {
			$found = 0;

			if ($sym eq '__uClibc_start_main') {
				$sym = '__uClibc_main';
			}

			#  __gnu_local_gp is defined specially by the linker on MIPS
			if ($sym eq '__gnu_local_gp') {
				$found = 1;
			}
			elsif (resolve($name, $sym) eq "*** unresolved ***") {
				@users = usersOf($name);
				foreach $u (@users) {
					# if exported by $u
					if (defined $elf_exp{$u}{$sym}) {
						fixDynDep($name, $u);
						$found = 1;
					}
					# if exported by shared libs of $u
					if (($t = resolve($u, $sym)) ne "*** unresolved ***") {
						fixDynDep($name, $t);
						$found = 1;
					}
				}
				
				if ($found == 0) {
#					if (!$name eq 'u2ec') {
					if (!$name eq 'mt-daapd-svn-1696') {
						print "Unable to resolve $sym used by $name\n", @users;
						exit 1;
					}
				}
			}
		}
	}
}

sub tab
{
	my $current = shift;
	my $target = shift;
	my $s = "";
	my $n;
	
	while (1) {
		$n = $current + (4 - ($current % 4));
		last if ($n > $target);
		$s = $s . "\t";
		$current = $n;
	}
	while ($current < $target) {
		$s = $s . " ";
		$current++;
	}
	return $s;
}

sub genXref
{
	my $f;
	my $fname;
	my $s;
	my @u;
	
#	print "Generating Xref Report...\n";
	
	open($f, ">libfoo_xref.txt");
	foreach $fname (sort keys %elf_type) {
		print $f "$fname:\n";
		
		if (scalar(@{$elf_lib{$fname}}) > 0) {
			print $f "Dependency:\n";
			foreach $s (sort @{$elf_lib{$fname}}) {
				print $f "\t$s", defined $elf_dyn{$fname}{$s} ? " (dyn)\n" : "\n";
			}
		}
		
		if (scalar(keys %{$elf_exp{$fname}}) > 0) {
			print $f "Export:\n";
			foreach $s (sort keys %{$elf_exp{$fname}}) {
				@u = usersOf($fname, $s);
				if (scalar(@u) > 0) {
					print $f "\t$s", tab(length($s) + 4, 40), " > ", join(",", @u), "\n";
				}
				else {
					print $f "\t$s\n";
				}
			}
		}
		
		if (scalar(keys %{$elf_ext{$fname}}) > 0) {
			print $f "External:\n";
			foreach $s (sort keys %{$elf_ext{$fname}}) {
				print $f "\t$s", tab(length($s) + 4, 40), " < ", resolve($fname, $s), "\n";
			}
		}
		
		print $f "\n";
	}
	close($f);
}


sub genSO
{
	my ($so, $arc, $strip, $opt) = @_;
	my $name = basename($so);
	my $sym;
	my $fn;
	my $inuse;
	my @used;
	my @unused;
	my $cmd;
	my $before, $after;

	if (!-f $so) {
		print "$name: not found, skipping...\n";
		return 0;
	}

	#!!TB
	if (!-f $arc) {
		print "$arc: not found, skipping...\n";
		return 0;
	}
	
	foreach $sym (sort keys %{$elf_exp{$name}}) {
		if ((scalar(usersOf($name, $sym)) > 0) || (${strip} eq "no")) {
			push(@used, $sym);
		}
		else {
			push(@unused, $sym);
		}
	}

#	print "\n$name: Attempting to link ", scalar(@used), " and remove ", scalar(@unused), " objects...\n";

	print LOG "\n\n${base}\n";
	
	$cmd = "arm-uclibc-ld -shared -s -z combreloc --warn-common --fatal-warnings ${opt} -soname ${name} -o ${so}";
	foreach (@{$elf_lib{$name}}) {
		if ((!$elf_dyn{$name}{$_}) && (/^lib(.+)\.so/)) {
			$cmd .= " -l$1";
		}
		else {
#			print LOG "Not marking for linkage: $_\n";
		}
	}
#	print "$cmd -u... ${arc}\n";	
	if (scalar(@used) == 0) {
		print "$name: WARNING: Library is not used by anything, deleting...\n";
		unlink $so;
#		<>;
		return 0;
	}
	$cmd .= " -u " . join(" -u ", @used) . " ". $arc;

	print LOG "Command: $cmd\n";
	print LOG "Used: ", join(",", @used), "\n";
	print LOG "Unused: ", join(",", @unused), "\n";
	
	$before = -s $so;

	system($cmd);
	if ($? != 0) {
		error("ld returned $?");
	}

	$after = -s $so;
	
	print "$name: Attempted to remove ", scalar(@unused), "/", scalar(@unused) + scalar(@used), " symbols. ";
	printf "%.2fK - %.2fK = %.2fK\n", $before / 1024, $after / 1024, ($before - $after) / 1024;
	
#	print "\n$name: Attempting to link ", scalar(@used), " and remove ", scalar(@unused), " objects...\n";
#	printf "Before: %.2fK / After: %.2fK / Removed: %.2fK\n\n", $before / 1024, $after / 1024, ($before - $after) / 1024;

	return ($before > $after)
}


##
##
##

#	print "\nlibfoo.pl - fooify shared libraries\n";
#	print "Copyright (C) 2006-2007 Jonathan Zarate\n\n";

if ((!-d $root) || (!-d $uclibc) || (!-d $router)) {
	print "Missing or invalid environment variables\n";
	exit(1);
}

#open(LOG, ">libfoo.debug");
open(LOG, ">/dev/null");

print "Loading...\r";
load($root);
print "Finished loading files.", " " x 30, "\r";

fixDyn();
fillGaps();

genXref();

$stripshared = "yes";
if ($ARGV[0] eq "--noopt") {
	$stripshared = "no";
}

# WAR for a weird "ld" bug: unless we drag in these few functions, re-linking
# libpthread.so.0 completely screws it up - at least in the current toolchain
# (binutils 2.20.1 / gcc 4.2.4) :(
$libpthreadwar = "-u pthread_mutexattr_init -u pthread_mutexattr_settype -u pthread_mutexattr_destroy";

# genSO("${root}/lib/libc.so.0", "${uclibc}/lib/libc.a", "${stripshared}", "-init __uClibc_init ${uclibc}/lib/optinfo/interp.os");#disable for tmp
genSO("${root}/lib/libresolv.so.0", "${uclibc}/lib/libresolv.a", "${stripshared}");
genSO("${root}/lib/libcrypt.so.0", "${uclibc}/lib/libcrypt.a", "${stripshared}");
genSO("${root}/lib/libm.so.0", "${uclibc}/lib/libm.a", "${stripshared}");
genSO("${root}/lib/libpthread.so.0", "${uclibc}/lib/libpthread.a", "${stripshared}", "${libpthreadwar}");
genSO("${root}/lib/libutil.so.0", "${uclibc}/lib/libutil.a", "${stripshared}");
#  genSO("${root}/lib/libdl.so.0", "${uclibc}/lib/libdl.a", "${stripshared}");
#  genSO("${root}/lib/libnsl.so.0", "${uclibc}/lib/libnsl.a", "${stripshared}");

genSO("${root}/usr/lib/libcrypto.so.1.0.0", "${router}/openssl/libcrypto.a");
genSO("${root}/usr/lib/libssl.so.1.0.0", "${router}/openssl/libssl.a", "", "-L${router}/openssl");

genSO("${root}/usr/lib/libzebra.so", "${router}/zebra/lib/libzebra.a");
genSO("${root}/usr/lib/libz.so.1", "${router}/zlib/libz.a");
genSO("${root}/usr/lib/libjpeg.so", "${router}/jpeg/libjpeg.a");
genSO("${root}/usr/lib/libsqlite3.so.0", "${router}/sqlite/.libs/libsqlite3.a");
genSO("${root}/usr/lib/libogg.so.0", "${router}/libogg/src/.libs/libogg.a");
genSO("${root}/usr/lib/libvorbis.so.0", "${router}/libvorbis/lib/.libs/libvorbis.a", "", "-L${router}/libogg/src/.libs");
genSO("${root}/usr/lib/libid3tag.so.0", "${router}/libid3tag/.libs/libid3tag.a", "", "-L${router}/zlib");
genSO("${root}/usr/lib/libexif.so.12", "${router}/libexif/libexif/.libs/libexif.a");
genSO("${root}/usr/lib/libFLAC.so.8", "${router}/flac/src/libFLAC/.libs/libFLAC.a", "", "-L${router}/libogg/src/.libs");
genSO("${root}/usr/lib/libavcodec.so.52", "${router}/ffmpeg/libavcodec/libavcodec.a", "", "-L${router}/ffmpeg/libavutil");
genSO("${root}/usr/lib/libavutil.so.50", "${router}/ffmpeg/libavutil/libavutil.a");
genSO("${root}/usr/lib/libavformat.so.52", "${router}/ffmpeg/libavformat/libavformat.a", "", "-L${router}/ffmpeg/libavutil -L${router}/ffmpeg/libavcodec");
genSO("${root}/usr/lib/libsmb.so", "${router}/samba/source/bin/libsmb.a");
genSO("${root}/usr/lib/libbigballofmud.so", "${router}/samba3/source/bin/libbigballofmud.a");

genSO("${root}/usr/lib/liblzo2.so.2", "${router}/lzo/src/.libs/liblzo2.a");
#	genSO("${root}/usr/lib/libtamba.so", "${router}/samba3/source/bin/libtamba.a");
#	genSO("${root}/usr/lib/libiptc.so", "${router}/iptables/libiptc/libiptc.a");
#	genSO("${root}/usr/lib/libshared.so", "${router}/shared/libshared.a");
#	genSO("${root}/usr/lib/libnvram.so", "${router}/nvram/libnvram.a");
#	genSO("${root}/usr/lib/libusb-1.0.so.0", "${router}/libusb10/libusb/.libs/libusb-1.0.a");
#	genSO("${root}/usr/lib/libusb-0.1.so.4", "${router}/libusb/libusb/.libs/libusb.a", "", "-L${router}/libusb10/libusb/.libs");
genSO("${root}/usr/lib/libbcmcrypto.so", "${router}/libbcmcrypto/libbcmcrypto.a");


print "\n";

close(LOG);
exit(0);
