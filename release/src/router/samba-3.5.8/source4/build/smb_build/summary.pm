# Samba Build System
# - write out summary
#
#  Copyright (C) Jelmer Vernooij 2006
#  Released under the GNU GPL

package summary;
use smb_build::config;
use strict;

sub enabled($)
{
    my ($val) = @_;

    return (defined($val) && $val =~ m/yes|true/i);
}

sub showitem($$$)
{
	my ($output,$desc,$items) = @_;

	my @need = ();

	foreach (@$items) {
		push (@need, $_) if (enabled($config::enable{$_}));
	}

	print "Support for $desc: ";
	if ($#need >= 0) {
		print "no (install " . join(',',@need) . ")\n";
	} else {
		print "yes\n";
	}
}

sub showisexternal($$$)
{
	my ($output, $desc, $name) = @_;
	print "Using external $desc: ";
	if ($output->{$name}->{TYPE} eq "SUBSYSTEM" or
	    $output->{$name}->{TYPE} eq "LIBRARY") {
		print "no"; 
	} else {
		print "yes";
	}
	print "\n";
}

sub show($$)
{
	my ($output,$config) = @_;

	print "Summary:\n\n";
	showitem($output, "SSL in SWAT and LDAP", ["GNUTLS"]);
	showitem($output, "threads in server (see --with-pthread)", ["PTHREAD"]);
	showitem($output, "intelligent command line editing", ["READLINE"]);
	showitem($output, "changing process titles (see --with-setproctitle)", ["SETPROCTITLE"]);
	showitem($output, "using extended attributes", ["XATTR"]);
	showitem($output, "using libblkid", ["BLKID"]);
	showitem($output, "using iconv", ["ICONV"]);
	showitem($output, "using pam", ["PAM"]);
	showitem($output, "python bindings", ["LIBPYTHON"]);
	showisexternal($output, "popt", "LIBPOPT");
	showisexternal($output, "talloc", "LIBTALLOC");
	showisexternal($output, "tdb", "LIBTDB");
	showisexternal($output, "tevent", "LIBTEVENT");
	showisexternal($output, "ldb", "LIBLDB");
	print "Developer mode: ".(enabled($config->{developer})?"yes":"no")."\n";
	print "Automatic dependencies: ".
	    (enabled($config->{automatic_dependencies})
		    ? "yes" : "no (install GNU make >= 3.81 and see --enable-automatic-dependencies)") .
	     "\n";
	
	print "Building shared libraries: " .
	    (enabled($config->{BLDSHARED})
		    ? "yes" : "no (not supported on this system)") .
	    "\n";
	print "Using shared libraries internally: " .
	    (enabled($config->{USESHARED})
		    ? "yes" : "no (specify --enable-dso)") .
	    "\n";

	print "\n";
}

1;
