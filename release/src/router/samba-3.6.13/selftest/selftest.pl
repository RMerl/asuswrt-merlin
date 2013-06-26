#!/usr/bin/perl
# Bootstrap Samba and run a number of tests against it.
# Copyright (C) 2005-2010 Jelmer Vernooij <jelmer@samba.org>
# Copyright (C) 2007-2009 Stefan Metzmacher <metze@samba.org>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

=pod

=head1 NAME

selftest - Samba test runner

=head1 SYNOPSIS

selftest --help

selftest [--srcdir=DIR] [--builddir=DIR] [--exeext=EXT][--target=samba4|samba3|win|kvm] [--socket-wrapper] [--quick] [--exclude=FILE] [--include=FILE] [--one] [--prefix=prefix] [--testlist=FILE] [TESTS]

=head1 DESCRIPTION

A simple test runner. TESTS is a regular expression with tests to run.

=head1 OPTIONS

=over 4

=item I<--help>

Show list of available options.

=item I<--srcdir=DIR>

Source directory.

=item I<--builddir=DIR>

Build directory.

=item I<--exeext=EXT>

Executable extention

=item I<--prefix=DIR>

Change directory to run tests in. Default is 'st'.

=item I<--target samba4|samba3|win|kvm>

Specify test target against which to run. Default is 'samba4'.

=item I<--quick>

Run only a limited number of tests. Intended to run in about 30 seconds on 
moderately recent systems.
		
=item I<--socket-wrapper>

Use socket wrapper library for communication with server. Only works 
when the server is running locally.

Will prevent TCP and UDP ports being opened on the local host but 
(transparently) redirects these calls to use unix domain sockets.

=item I<--exclude>

Specify a file containing a list of tests that should be skipped. Possible 
candidates are tests that segfault the server, flip or don't end. 

=item I<--include>

Specify a file containing a list of tests that should be run. Same format 
as the --exclude flag.

Not includes specified means all tests will be run.

=item I<--one>

Abort as soon as one test fails.

=item I<--testlist>

Load a list of tests from the specified location.

=back

=head1 ENVIRONMENT

=over 4

=item I<SMBD_VALGRIND>

=item I<TORTURE_MAXTIME>

=item I<VALGRIND>

=item I<TLS_ENABLED>

=item I<srcdir>

=back

=head1 LICENSE

selftest is licensed under the GNU General Public License L<http://www.gnu.org/licenses/gpl.html>.

=head1 AUTHOR

Jelmer Vernooij

=cut

use strict;

use FindBin qw($RealBin $Script);
use File::Spec;
use File::Temp qw(tempfile);
use Getopt::Long;
use POSIX;
use Cwd qw(abs_path);
use lib "$RealBin";
use Subunit;
use SocketWrapper;

eval {
require Time::HiRes;
Time::HiRes->import("time");
};
if ($@) {
	print "You don't have Time::Hires installed !\n";
}

my $opt_help = 0;
my $opt_target = "samba4";
my $opt_quick = 0;
my $opt_socket_wrapper = 0;
my $opt_socket_wrapper_pcap = undef;
my $opt_socket_wrapper_keep_pcap = undef;
my $opt_one = 0;
my @opt_exclude = ();
my @opt_include = ();
my $opt_verbose = 0;
my $opt_image = undef;
my $opt_testenv = 0;
my $opt_list = 0;
my $ldap = undef;
my $opt_resetup_env = undef;
my $opt_bindir = undef;
my $opt_load_list = undef;
my @testlists = ();

my $srcdir = ".";
my $builddir = ".";
my $exeext = "";
my $prefix = "./st";

my @includes = ();
my @excludes = ();

sub pipe_handler {
	my $sig = shift @_;
	print STDERR "Exiting early because of SIGPIPE.\n";
	exit(1);
}

$SIG{PIPE} = \&pipe_handler;

sub find_in_list($$)
{
	my ($list, $fullname) = @_;

	foreach (@$list) {
		if ($fullname =~ /$$_[0]/) {
			 return ($$_[1]) if ($$_[1]);
			 return "";
		}
	}

	return undef;
}

sub skip($)
{
	my ($name) = @_;

	return find_in_list(\@excludes, $name);
}

sub getlog_env($);

sub setup_pcap($)
{
	my ($name) = @_;

	return unless ($opt_socket_wrapper_pcap);
	return unless defined($ENV{SOCKET_WRAPPER_PCAP_DIR});

	my $fname = $name;
	$fname =~ s%[^abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\-]%_%g;

	my $pcap_file = "$ENV{SOCKET_WRAPPER_PCAP_DIR}/$fname.pcap";

	SocketWrapper::setup_pcap($pcap_file);

	return $pcap_file;
}

sub cleanup_pcap($$)
{
	my ($pcap_file, $exitcode) = @_;

	return unless ($opt_socket_wrapper_pcap);
	return if ($opt_socket_wrapper_keep_pcap);
	return unless ($exitcode == 0);
	return unless defined($pcap_file);

	unlink($pcap_file);
}

# expand strings from %ENV
sub expand_environment_strings($)
{
	my $s = shift;
	# we use a reverse sort so we do the longer ones first
	foreach my $k (sort { $b cmp $a } keys %ENV) {
		$s =~ s/\$$k/$ENV{$k}/g;
	}
	return $s;
}

sub run_testsuite($$$$$)
{
	my ($envname, $name, $cmd, $i, $totalsuites) = @_;
	my $pcap_file = setup_pcap($name);

	Subunit::start_testsuite($name);
	Subunit::progress_push();
	Subunit::report_time(time());
	system($cmd);
	Subunit::report_time(time());
	Subunit::progress_pop();

	if ($? == -1) {
		Subunit::progress_pop();
		Subunit::end_testsuite($name, "error", "Unable to run $cmd: $!");
		exit(1);
	} elsif ($? & 127) {
		Subunit::end_testsuite($name, "error",
			sprintf("%s died with signal %d, %s coredump\n", $cmd, ($? & 127),  ($? & 128) ? 'with' : 'without'));
		exit(1);
	}

	my $exitcode = $? >> 8;

	my $envlog = getlog_env($envname);
	if ($envlog ne "") {
		print "envlog: $envlog\n";
	}

	print "command: $cmd\n";
	printf "expanded command: %s\n", expand_environment_strings($cmd);

	if ($exitcode == 0) {
		Subunit::end_testsuite($name, "success");
	} else {
		Subunit::end_testsuite($name, "failure", "Exit code was $exitcode");
	}

	cleanup_pcap($pcap_file, $exitcode);

	if (not $opt_socket_wrapper_keep_pcap and defined($pcap_file)) {
		print "PCAP FILE: $pcap_file\n";
	}

	if ($exitcode != 0) {
		exit(1) if ($opt_one);
	}

	return $exitcode;
}

sub ShowHelp()
{
	print "Samba test runner
Copyright (C) Jelmer Vernooij <jelmer\@samba.org>
Copyright (C) Stefan Metzmacher <metze\@samba.org>

Usage: $Script [OPTIONS] TESTNAME-REGEX

Generic options:
 --help                     this help page
 --target=samba[34]|win|kvm Samba version to target
 --testlist=FILE	    file to read available tests from

Paths:
 --prefix=DIR               prefix to run tests in [st]
 --srcdir=DIR               source directory [.]
 --builddir=DIR             output directory [.]
 --exeext=EXT               executable extention []

Target Specific:
 --socket-wrapper-pcap	    save traffic to pcap directories
 --socket-wrapper-keep-pcap keep all pcap files, not just those for tests that 
                            failed
 --socket-wrapper           enable socket wrapper
 --bindir=PATH              path to target binaries

Samba4 Specific:
 --ldap=openldap|fedora-ds  back samba onto specified ldap server

Kvm Specific:
 --image=PATH               path to KVM image

Behaviour:
 --quick                    run quick overall test
 --one                      abort when the first test fails
 --verbose                  be verbose
 --testenv                  run a shell in the requested test environment
 --list                     list available tests
";
	exit(0);
}

my $result = GetOptions (
		'help|h|?' => \$opt_help,
		'target=s' => \$opt_target,
		'prefix=s' => \$prefix,
		'socket-wrapper' => \$opt_socket_wrapper,
		'socket-wrapper-pcap' => \$opt_socket_wrapper_pcap,
		'socket-wrapper-keep-pcap' => \$opt_socket_wrapper_keep_pcap,
		'quick' => \$opt_quick,
		'one' => \$opt_one,
		'exclude=s' => \@opt_exclude,
		'include=s' => \@opt_include,
		'srcdir=s' => \$srcdir,
		'builddir=s' => \$builddir,
		'exeext=s' => \$exeext,
		'verbose' => \$opt_verbose,
		'testenv' => \$opt_testenv,
		'list' => \$opt_list,
		'ldap:s' => \$ldap,
		'resetup-environment' => \$opt_resetup_env,
		'bindir:s' => \$opt_bindir,
		'image=s' => \$opt_image,
		'testlist=s' => \@testlists,
		'load-list=s' => \$opt_load_list,
	    );

exit(1) if (not $result);

ShowHelp() if ($opt_help);

die("--list and --testenv are mutually exclusive") if ($opt_list and $opt_testenv);

# we want unbuffered output
$| = 1;

my @tests = @ARGV;

# quick hack to disable rpc validation when using valgrind - its way too slow
unless (defined($ENV{VALGRIND})) {
	$ENV{VALIDATE} = "validate";
	$ENV{MALLOC_CHECK_} = 2;
}

# make all our python scripts unbuffered
$ENV{PYTHONUNBUFFERED} = 1;

my $bindir = ($opt_bindir or "$builddir/bin");
my $bindir_abs = abs_path($bindir);

# Backwards compatibility:
if (defined($ENV{TEST_LDAP}) and $ENV{TEST_LDAP} eq "yes") {
	if (defined($ENV{FEDORA_DS_ROOT})) {
		$ldap = "fedora-ds";
	} else {
		$ldap = "openldap";
	}
}

my $torture_maxtime = ($ENV{TORTURE_MAXTIME} or 1200);
if ($ldap) {
	# LDAP is slow
	$torture_maxtime *= 2;
}

$prefix =~ s+//+/+;
$prefix =~ s+/./+/+;
$prefix =~ s+/$++;

die("using an empty prefix isn't allowed") unless $prefix ne "";

#Ensure we have the test prefix around
mkdir($prefix, 0777) unless -d $prefix;

my $prefix_abs = abs_path($prefix);
my $tmpdir_abs = abs_path("$prefix/tmp");
mkdir($tmpdir_abs, 0777) unless -d $tmpdir_abs;

my $srcdir_abs = abs_path($srcdir);
my $builddir_abs = abs_path($builddir);

die("using an empty absolute prefix isn't allowed") unless $prefix_abs ne "";
die("using '/' as absolute prefix isn't allowed") unless $prefix_abs ne "/";

$ENV{PREFIX} = $prefix;
$ENV{KRB5CCNAME} = "$prefix/krb5ticket";
$ENV{PREFIX_ABS} = $prefix_abs;
$ENV{SRCDIR} = $srcdir;
$ENV{SRCDIR_ABS} = $srcdir_abs;
$ENV{BUILDDIR} = $builddir;
$ENV{BUILDDIR_ABS} = $builddir_abs;
$ENV{BINDIR} = $bindir_abs;
$ENV{EXEEXT} = $exeext;

my $tls_enabled = not $opt_quick;
$ENV{TLS_ENABLED} = ($tls_enabled?"yes":"no");

sub prefix_pathvar($$)
{
	my ($name, $newpath) = @_;
	if (defined($ENV{$name})) {
		$ENV{$name} = "$newpath:$ENV{$name}";
	} else {
		$ENV{$name} = $newpath;
	}
}
prefix_pathvar("PKG_CONFIG_PATH", "$bindir_abs/pkgconfig");
prefix_pathvar("PYTHONPATH", "$bindir_abs/python");

if ($opt_socket_wrapper_keep_pcap) {
	# Socket wrapper keep pcap implies socket wrapper pcap
	$opt_socket_wrapper_pcap = 1;
}

if ($opt_socket_wrapper_pcap) {
	# Socket wrapper pcap implies socket wrapper
	$opt_socket_wrapper = 1;
}

my $socket_wrapper_dir;
if ($opt_socket_wrapper) {
	$socket_wrapper_dir = SocketWrapper::setup_dir("$prefix_abs/w", $opt_socket_wrapper_pcap);
	print "SOCKET_WRAPPER_DIR=$socket_wrapper_dir\n";
} else {
	 unless ($< == 0) { 
		 print "WARNING: Not using socket wrapper, but also not running as root. Will not be able to listen on proper ports\n";
	 }
}

my $target;
my $testenv_default = "none";

if ($opt_target eq "samba4") {
	$testenv_default = "all";
	require target::Samba4;
	$target = new Samba4($bindir, $ldap, $srcdir, $exeext);
} elsif ($opt_target eq "samba3") {
	if ($opt_socket_wrapper and `$bindir/smbd -b | grep SOCKET_WRAPPER` eq "") {
		die("You must include --enable-socket-wrapper when compiling Samba in order to execute 'make test'.  Exiting....");
	}
	$testenv_default = "member";
	require target::Samba3;
	$target = new Samba3($bindir, $srcdir_abs);
} elsif ($opt_target eq "win") {
	die("Windows tests will not run with socket wrapper enabled.") 
		if ($opt_socket_wrapper);
	$testenv_default = "dc";
	require target::Windows;
	$target = new Windows();
} elsif ($opt_target eq "kvm") {
	die("Kvm tests will not run with socket wrapper enabled.") 
		if ($opt_socket_wrapper);
	require target::Kvm;
	die("No image specified") unless ($opt_image);
	$target = new Kvm($opt_image, undef);
}

#
# Start a Virtual Distributed Ethernet Switch
# Returns the pid of the switch.
#
sub start_vde_switch($)
{
	my ($path) = @_;

	system("vde_switch --pidfile $path/vde.pid --sock $path/vde.sock --daemon");

	open(PID, "$path/vde.pid");
	<PID> =~ /([0-9]+)/;
	my $pid = $1;
	close(PID);

	return $pid;
}

# Stop a Virtual Distributed Ethernet Switch
sub stop_vde_switch($)
{
	my ($pid) = @_;
	kill 9, $pid;
}

sub read_test_regexes($)
{
	my ($name) = @_;
	my @ret = ();
	open(LF, "<$name") or die("unable to read $name: $!");
	while (<LF>) { 
		chomp; 
		next if (/^#/);
		if (/^(.*?)([ \t]+)\#([\t ]*)(.*?)$/) {
			push (@ret, [$1, $4]);
		} else {
			s/^(.*?)([ \t]+)\#([\t ]*)(.*?)$//;
			push (@ret, [$_, undef]); 
		}
	}
	close(LF);
	return @ret;
}

foreach (@opt_exclude) {
	push (@excludes, read_test_regexes($_));
}

foreach (@opt_include) {
	push (@includes, read_test_regexes($_));
}

my $interfaces = join(',', ("127.0.0.11/8",
			    "127.0.0.12/8",
			    "127.0.0.13/8",
			    "127.0.0.14/8",
			    "127.0.0.15/8",
			    "127.0.0.16/8"));

my $clientdir = "$prefix_abs/client";

my $conffile = "$clientdir/client.conf";
$ENV{SMB_CONF_PATH} = $conffile;

sub write_clientconf($$$)
{
	my ($conffile, $clientdir, $vars) = @_;

	mkdir("$clientdir", 0777) unless -d "$clientdir";

	if ( -d "$clientdir/private" ) {
	        unlink <$clientdir/private/*>;
	} else {
	        mkdir("$clientdir/private", 0777);
	}

	if ( -d "$clientdir/lockdir" ) {
	        unlink <$clientdir/lockdir/*>;
	} else {
	        mkdir("$clientdir/lockdir", 0777);
	}

	if ( -d "$clientdir/ncalrpcdir" ) {
	        unlink <$clientdir/ncalrpcdir/*>;
	} else {
	        mkdir("$clientdir/ncalrpcdir", 0777);
	}

	open(CF, ">$conffile");
	print CF "[global]\n";
	if (defined($ENV{VALGRIND})) {
		print CF "\ticonv:native = true\n";
	} else {
		print CF "\ticonv:native = false\n";
	}
	print CF "\tnetbios name = client\n";
	if (defined($vars->{DOMAIN})) {
		print CF "\tworkgroup = $vars->{DOMAIN}\n";
	}
	if (defined($vars->{REALM})) {
		print CF "\trealm = $vars->{REALM}\n";
	}
	if ($opt_socket_wrapper) {
		print CF "\tinterfaces = $interfaces\n";
	}
	print CF "
	private dir = $clientdir/private
	lock dir = $clientdir/lockdir
	ncalrpc dir = $clientdir/ncalrpcdir
	name resolve order = bcast file
	panic action = $RealBin/gdb_backtrace \%PID\% \%PROG\%
	max xmit = 32K
	notify:inotify = false
	ldb:nosync = true
	system:anonymous = true
	client lanman auth = Yes
	log level = 1
	torture:basedir = $clientdir
#We don't want to pass our self-tests if the PAC code is wrong
	gensec:require_pac = true
	resolv:host file = $prefix_abs/dns_host_file
#We don't want to run 'speed' tests for very long
        torture:timelimit = 1
";
	close(CF);
}

my @todo = ();

sub should_run_test($)
{
	my $name = shift;
	if ($#tests == -1) {
		return 1;
	}
	for (my $i=0; $i <= $#tests; $i++) {
		if ($name =~ /$tests[$i]/i) {
			return 1;
		}
	}
	return 0;
}

sub read_testlist($)
{
	my ($filename) = @_;

	my @ret = ();
	open(IN, $filename) or die("Unable to open $filename: $!");

	while (<IN>) {
		if (/-- TEST(-LOADLIST|-IDLIST|) --\n/) {
			my $supports_loadlist = (defined($1) and $1 eq "-LOADLIST");
			my $supports_idlist = (defined($1) and $1 eq "-IDLIST");
			my $name = <IN>;
			$name =~ s/\n//g;
			my $env = <IN>;
			$env =~ s/\n//g;
			my $cmdline = <IN>;
			$cmdline =~ s/\n//g;
			if (should_run_test($name) == 1) {
				push (@ret, [$name, $env, $cmdline, $supports_loadlist, $supports_idlist]);
			}
		} else {
			print;
		}
	}
	close(IN) or die("Error creating recipe");
	return @ret;
}

if ($#testlists == -1) {
	die("No testlists specified");
}

$ENV{SELFTEST_PREFIX} = "$prefix_abs";
$ENV{SELFTEST_TMPDIR} = "$tmpdir_abs";
$ENV{TEST_DATA_PREFIX} = "$tmpdir_abs";
if ($opt_socket_wrapper) {
	$ENV{SELFTEST_INTERFACES} = $interfaces;
} else {
	$ENV{SELFTEST_INTERFACES} = "";
}
if ($opt_verbose) {
	$ENV{SELFTEST_VERBOSE} = "1";
} else {
	$ENV{SELFTEST_VERBOSE} = "";
}
if ($opt_quick) {
	$ENV{SELFTEST_QUICK} = "1";
} else {
	$ENV{SELFTEST_QUICK} = "";
}
$ENV{SELFTEST_TARGET} = $opt_target;
$ENV{SELFTEST_MAXTIME} = $torture_maxtime;

my @available = ();
foreach my $fn (@testlists) {
	foreach (read_testlist($fn)) {
		my $name = $$_[0];
		next if (@includes and not defined(find_in_list(\@includes, $name)));
		push (@available, $_);
	}
}

my $restricted = undef;
my $restricted_used = {};

if ($opt_load_list) {
	$restricted = [];
	open(LOAD_LIST, "<$opt_load_list") or die("Unable to open $opt_load_list");
	while (<LOAD_LIST>) {
		chomp;
		push (@$restricted, $_);
	}
	close(LOAD_LIST);
}

my $individual_tests = undef;
$individual_tests = {};

foreach my $testsuite (@available) {
	my $name = $$testsuite[0];
	my $skipreason = skip($name);
	if (defined($restricted)) {
		# Find the testsuite for this test
		my $match = undef;
		foreach my $r (@$restricted) {
			if ($r eq $name) {
				$individual_tests->{$name} = [];
				$match = $r;
				$restricted_used->{$r} = 1;
			} elsif (substr($r, 0, length($name)+1) eq "$name.") {
				push(@{$individual_tests->{$name}}, $r);
				$match = $r;
				$restricted_used->{$r} = 1;
			}
		}
		if ($match) {
			if (defined($skipreason)) {
					Subunit::skip_testsuite($name, $skipreason);
			} else {
				push(@todo, $testsuite);
			}
		}
	} elsif (defined($skipreason)) {
		Subunit::skip_testsuite($name, $skipreason);
	} else {
		push(@todo, $testsuite);
	}
}

if (defined($restricted)) {
	foreach (@$restricted) {
		unless (defined($restricted_used->{$_})) {
			print "No test or testsuite found matching $_\n";
		}
	}
} elsif ($#todo == -1) {
	print STDERR "No tests to run\n";
	exit(1);
}

my $suitestotal = $#todo + 1;

Subunit::progress($suitestotal);
Subunit::report_time(time());

my $i = 0;
$| = 1;

my %running_envs = ();

sub get_running_env($)
{
	my ($name) = @_;

	my $envname = $name;

	$envname =~ s/:.*//;

	return $running_envs{$envname};
}

my @exported_envvars = (
	# domain stuff
	"DOMAIN",
	"REALM",

	# domain controller stuff
	"DC_SERVER",
	"DC_SERVER_IP",
	"DC_NETBIOSNAME",
	"DC_NETBIOSALIAS",

	# domain member
	"MEMBER_SERVER",
	"MEMBER_SERVER_IP",
	"MEMBER_NETBIOSNAME",
	"MEMBER_NETBIOSALIAS",

	# rpc proxy controller stuff
	"RPC_PROXY_SERVER",
	"RPC_PROXY_SERVER_IP",
	"RPC_PROXY_NETBIOSNAME",
	"RPC_PROXY_NETBIOSALIAS",

	# domain controller stuff for Vampired DC
	"VAMPIRE_DC_SERVER",
	"VAMPIRE_DC_SERVER_IP",
	"VAMPIRE_DC_NETBIOSNAME",
	"VAMPIRE_DC_NETBIOSALIAS",

	# server stuff
	"SERVER",
	"SERVER_IP",
	"NETBIOSNAME",
	"NETBIOSALIAS",

	# user stuff
	"USERNAME",
	"USERID",
	"PASSWORD",
	"DC_USERNAME",
	"DC_PASSWORD",

	# misc stuff
	"KRB5_CONFIG",
	"WINBINDD_SOCKET_DIR",
	"WINBINDD_PRIV_PIPE_DIR",
	"NMBD_SOCKET_DIR",
	"LOCAL_PATH"
);

$SIG{INT} = $SIG{QUIT} = $SIG{TERM} = sub { 
	my $signame = shift;
	teardown_env($_) foreach(keys %running_envs);
	die("Received signal $signame");
};

sub setup_env($$)
{
	my ($name, $prefix) = @_;

	my $testenv_vars = undef;

	my $envname = $name;
	my $option = $name;

	$envname =~ s/:.*//;
	$option =~ s/^[^:]*//;
	$option =~ s/^://;

	$option = "client" if $option eq "";

	if ($envname eq "none") {
		$testenv_vars = {};
	} elsif (defined(get_running_env($envname))) {
		$testenv_vars = get_running_env($envname);
		if (not $testenv_vars->{target}->check_env($testenv_vars)) {
			print $testenv_vars->{target}->getlog_env($testenv_vars);
			$testenv_vars = undef;
		}
	} else {
		$testenv_vars = $target->setup_env($envname, $prefix);
		if (defined($testenv_vars) && not defined($testenv_vars->{target})) {
		       $testenv_vars->{target} = $target;
		}
	}

	return undef unless defined($testenv_vars);

	$running_envs{$envname} = $testenv_vars;

	if ($option eq "local") {
		SocketWrapper::set_default_iface($testenv_vars->{SOCKET_WRAPPER_DEFAULT_IFACE});
		$ENV{SMB_CONF_PATH} = $testenv_vars->{SERVERCONFFILE};
	} elsif ($option eq "client") {
		SocketWrapper::set_default_iface(11);
		write_clientconf($conffile, $clientdir, $testenv_vars);
		$ENV{SMB_CONF_PATH} = $conffile;
	} else {
		die("Unknown option[$option] for envname[$envname]");
	}

	foreach (@exported_envvars) {
		if (defined($testenv_vars->{$_})) {
			$ENV{$_} = $testenv_vars->{$_};
		} else {
			delete $ENV{$_};
		}
	}

	return $testenv_vars;
}

sub exported_envvars_str($)
{
	my ($testenv_vars) = @_;
	my $out = "";

	foreach (@exported_envvars) {
		next unless defined($testenv_vars->{$_});
		$out .= $_."=".$testenv_vars->{$_}."\n";
	}

	return $out;
}

sub getlog_env($)
{
	my ($envname) = @_;
	return "" if ($envname eq "none");
	my $env = get_running_env($envname);
	return $env->{target}->getlog_env($env);
}

sub check_env($)
{
	my ($envname) = @_;
	return 1 if ($envname eq "none");
	my $env = get_running_env($envname);
	return $env->{target}->check_env($env);
}

sub teardown_env($)
{
	my ($envname) = @_;
	return if ($envname eq "none");
	my $env = get_running_env($envname);
	$env->{target}->teardown_env($env);
	delete $running_envs{$envname};
}

# This 'global' file needs to be empty when we start
unlink("$prefix_abs/dns_host_file");

if ($opt_testenv) {
	my $testenv_name = $ENV{SELFTEST_TESTENV};
	$testenv_name = $testenv_default unless defined($testenv_name);

	my $testenv_vars = setup_env($testenv_name, $prefix);

	die("Unable to setup environment $testenv_name") unless ($testenv_vars);

	$ENV{PIDDIR} = $testenv_vars->{PIDDIR};
	$ENV{ENVNAME} = $testenv_name;

	my $envvarstr = exported_envvars_str($testenv_vars);

	my $term = ($ENV{TERMINAL} or "xterm -e");
	system("$term 'echo -e \"
Welcome to the Samba4 Test environment '$testenv_name'

This matches the client environment used in make test
server is pid `cat \$PIDDIR/samba.pid`

Some useful environment variables:
TORTURE_OPTIONS=\$TORTURE_OPTIONS
SMB_CONF_PATH=\$SMB_CONF_PATH

$envvarstr
\" && LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH} bash'");
	teardown_env($testenv_name);
} elsif ($opt_list) {
	foreach (@todo) {
		my $cmd = $$_[2];
		my $name = $$_[0];
		my $envname = $$_[1];

		unless($cmd =~ /\$LISTOPT/) {
			warn("Unable to list tests in $name");
			next;
		}

		$cmd =~ s/\$LISTOPT/--list/g;

		system($cmd);

		if ($? == -1) {
			die("Unable to run $cmd: $!");
		} elsif ($? & 127) {
			die(snprintf("%s died with signal %d, %s coredump\n", $cmd, ($? & 127),  ($? & 128) ? 'with' : 'without'));
		}

		my $exitcode = $? >> 8;
		if ($exitcode != 0) {
			die("$cmd exited with exit code $exitcode");
		}
	}
} else {
	foreach (@todo) {
		$i++;
		my $cmd = $$_[2];
		my $name = $$_[0];
		my $envname = $$_[1];

		my $envvars = setup_env($envname, $prefix);
		if (not defined($envvars)) {
			Subunit::start_testsuite($name);
			Subunit::end_testsuite($name, "error",
				"unable to set up environment $envname - exiting");
			next;
		}

		# Generate a file with the individual tests to run, if the 
		# test runner for this test suite supports it.
		if ($individual_tests and $individual_tests->{$name}) {
			if ($$_[3]) {
				my ($fh, $listid_file) = tempfile(UNLINK => 0);
				foreach my $test (@{$individual_tests->{$name}}) {
					print $fh substr($test, length($name)+1) . "\n";
				}
				$cmd =~ s/\$LOADLIST/--load-list=$listid_file/g;
			} elsif ($$_[4]) {
				$cmd =~ s/\s+[^\s]+\s*$//;
				$cmd .= " " . join(' ', @{$individual_tests->{$name}});
			}
		}

		run_testsuite($envname, $name, $cmd, $i, $suitestotal);

		teardown_env($envname) if ($opt_resetup_env);
	}
}

print "\n";

teardown_env($_) foreach (keys %running_envs);

my $failed = 0;

# if there were any valgrind failures, show them
foreach (<$prefix/valgrind.log*>) {
	next unless (-s $_);
	print "VALGRIND FAILURE\n";
	$failed++;
	system("cat $_");
}
exit 0;
