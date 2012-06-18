#!/usr/bin/perl
# Bootstrap Samba and run a number of tests against it.
# Copyright (C) 2005-2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later.

package Samba3;

use strict;
use Cwd qw(abs_path);
use FindBin qw($RealBin);
use POSIX;

sub binpath($$)
{
	my ($self, $binary) = @_;

	if (defined($self->{bindir})) {
		my $path = "$self->{bindir}/$binary";
		-f $path or die("File $path doesn't exist");
		return $path;
	}

	return $binary;
}

sub new($$) {
	my ($classname, $bindir) = @_;
	my $self = { bindir => $bindir };
	bless $self;
	return $self;
}

sub teardown_env($$)
{
	my ($self, $envvars) = @_;

	my $smbdpid = read_pid($envvars, "smbd");
	my $nmbdpid = read_pid($envvars, "nmbd");
	my $winbinddpid = read_pid($envvars, "winbindd");

	$self->stop_sig_term($smbdpid);
	$self->stop_sig_term($nmbdpid);
	$self->stop_sig_term($winbinddpid);

	sleep(2);

	$self->stop_sig_kill($smbdpid);
	$self->stop_sig_kill($nmbdpid);
	$self->stop_sig_kill($winbinddpid);

	return 0;
}

sub getlog_env_app($$$)
{
	my ($self, $envvars, $name) = @_;

	my $title = "$name LOG of: $envvars->{NETBIOSNAME}\n";
	my $out = $title;

	open(LOG, "<".$envvars->{$name."_TEST_LOG"});

	seek(LOG, $envvars->{$name."_TEST_LOG_POS"}, SEEK_SET);
	while (<LOG>) {
		$out .= $_;
	}
	$envvars->{$name."_TEST_LOG_POS"} = tell(LOG);
	close(LOG);

	return "" if $out eq $title;
 
	return $out;
}

sub getlog_env($$)
{
	my ($self, $envvars) = @_;
	my $ret = "";

	$ret .= $self->getlog_env_app($envvars, "SMBD");
	$ret .= $self->getlog_env_app($envvars, "NMBD");
	$ret .= $self->getlog_env_app($envvars, "WINBINDD");

	return $ret;
}

sub check_env($$)
{
	my ($self, $envvars) = @_;

	# TODO ...
	return 1;
}

sub setup_env($$$)
{
	my ($self, $envname, $path) = @_;
	
	if ($envname eq "dc") {
		return $self->setup_dc("$path/dc");
	} elsif ($envname eq "member") {
		if (not defined($self->{vars}->{dc})) {
			$self->setup_dc("$path/dc");
		}
		return $self->setup_member("$path/member", $self->{vars}->{dc});
	} else {
		return undef;
	}
}

sub setup_dc($$)
{
	my ($self, $path) = @_;

	print "PROVISIONING DC...";

	my $dc_options = "
	domain master = yes
	domain logons = yes
	lanman auth = yes
";

	my $vars = $self->provision($path,
				    "LOCALDC2",
				    2,
				    "localdc2pass",
				    $dc_options);

	$self->check_or_start($vars,
			      ($ENV{SMBD_MAXTIME} or 2700),
			       "yes", "yes", "yes");

	$self->wait_for_start($vars);

	$self->{vars}->{dc} = $vars;

	return $vars;
}

sub setup_member($$$)
{
	my ($self, $prefix, $dcvars) = @_;

	print "PROVISIONING MEMBER...";

	my $member_options = "
	security = domain
	server signing = on
";
	my $ret = $self->provision($prefix,
				   "LOCALMEMBER3",
				   3,
				   "localmember3pass",
				   $member_options);

	$ret or die("Unable to provision");

	my $net = $self->binpath("net");
	my $cmd = "";
	$cmd .= "SOCKET_WRAPPER_DEFAULT_IFACE=\"$ret->{SOCKET_WRAPPER_DEFAULT_IFACE}\" ";
	$cmd .= "$net join $ret->{CONFIGURATION} $dcvars->{DOMAIN} member";
	$cmd .= " -U$dcvars->{USERNAME}\%$dcvars->{PASSWORD}";

	system($cmd) == 0 or die("Join failed\n$cmd");

	$self->check_or_start($ret,
			      ($ENV{SMBD_MAXTIME} or 2700),
			       "yes", "yes", "yes");

	$self->wait_for_start($ret);

	$ret->{DC_SERVER} = $dcvars->{SERVER};
	$ret->{DC_SERVER_IP} = $dcvars->{SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $dcvars->{NETBIOSNAME};
	$ret->{DC_USERNAME} = $dcvars->{USERNAME};
	$ret->{DC_PASSWORD} = $dcvars->{PASSWORD};

	return $ret;
}

sub stop($)
{
	my ($self) = @_;
}

sub stop_sig_term($$) {
	my ($self, $pid) = @_;
	kill("USR1", $pid) or kill("ALRM", $pid) or warn("Unable to kill $pid: $!");
}

sub stop_sig_kill($$) {
	my ($self, $pid) = @_;
	kill("ALRM", $pid) or warn("Unable to kill $pid: $!");
}

sub write_pid($$$)
{
	my ($env_vars, $app, $pid) = @_;

	open(PID, ">$env_vars->{PIDDIR}/timelimit.$app.pid");
	print PID $pid;
	close(PID);
}

sub read_pid($$)
{
	my ($env_vars, $app) = @_;

	open(PID, "<$env_vars->{PIDDIR}/timelimit.$app.pid");
	my $pid = <PID>;
	close(PID);
	return $pid;
}

sub check_or_start($$$$$) {
	my ($self, $env_vars, $maxtime, $nmbd, $winbindd, $smbd) = @_;

	unlink($env_vars->{NMBD_TEST_LOG});
	print "STARTING NMBD...";
	my $pid = fork();
	if ($pid == 0) {
		open STDOUT, ">$env_vars->{NMBD_TEST_LOG}";
		open STDERR, '>&STDOUT';

		SocketWrapper::set_default_iface($env_vars->{SOCKET_WRAPPER_DEFAULT_IFACE});

		$ENV{WINBINDD_SOCKET_DIR} = $env_vars->{WINBINDD_SOCKET_DIR};

		$ENV{NSS_WRAPPER_PASSWD} = $env_vars->{NSS_WRAPPER_PASSWD};
		$ENV{NSS_WRAPPER_GROUP} = $env_vars->{NSS_WRAPPER_GROUP};
		$ENV{NSS_WRAPPER_WINBIND_SO_PATH} = $env_vars->{NSS_WRAPPER_WINBIND_SO_PATH};

		if ($nmbd ne "yes") {
			$SIG{USR1} = $SIG{ALRM} = $SIG{INT} = $SIG{QUIT} = $SIG{TERM} = sub {
				my $signame = shift;
				print("Skip nmbd received signal $signame");
				exit 0;
			};
			sleep($maxtime);
			exit 0;
		}

		my @optargs = ("-d0");
		if (defined($ENV{NMBD_OPTIONS})) {
			@optargs = split(/ /, $ENV{NMBD_OPTIONS});
		}

		$ENV{MAKE_TEST_BINARY} = $self->binpath("nmbd");

		my @preargs = ($self->binpath("timelimit"), $maxtime);
		if(defined($ENV{NMBD_VALGRIND})) { 
			@preargs = split(/ /, $ENV{NMBD_VALGRIND});
		}

		exec(@preargs, $self->binpath("nmbd"), "-F", "-S", "--no-process-group", "-s", $env_vars->{SERVERCONFFILE}, @optargs) or die("Unable to start nmbd: $!");
	}
	write_pid($env_vars, "nmbd", $pid);
	print "DONE\n";

	unlink($env_vars->{WINBINDD_TEST_LOG});
	print "STARTING WINBINDD...";
	$pid = fork();
	if ($pid == 0) {
		open STDOUT, ">$env_vars->{WINBINDD_TEST_LOG}";
		open STDERR, '>&STDOUT';

		SocketWrapper::set_default_iface($env_vars->{SOCKET_WRAPPER_DEFAULT_IFACE});

		$ENV{WINBINDD_SOCKET_DIR} = $env_vars->{WINBINDD_SOCKET_DIR};

		$ENV{NSS_WRAPPER_PASSWD} = $env_vars->{NSS_WRAPPER_PASSWD};
		$ENV{NSS_WRAPPER_GROUP} = $env_vars->{NSS_WRAPPER_GROUP};
		$ENV{NSS_WRAPPER_WINBIND_SO_PATH} = $env_vars->{NSS_WRAPPER_WINBIND_SO_PATH};

		if ($winbindd ne "yes") {
			$SIG{USR1} = $SIG{ALRM} = $SIG{INT} = $SIG{QUIT} = $SIG{TERM} = sub {
				my $signame = shift;
				print("Skip winbindd received signal $signame");
				exit 0;
			};
			sleep($maxtime);
			exit 0;
		}

		my @optargs = ("-d0");
		if (defined($ENV{WINBINDD_OPTIONS})) {
			@optargs = split(/ /, $ENV{WINBINDD_OPTIONS});
		}

		$ENV{MAKE_TEST_BINARY} = $self->binpath("winbindd");

		my @preargs = ($self->binpath("timelimit"), $maxtime);
		if(defined($ENV{WINBINDD_VALGRIND})) {
			@preargs = split(/ /, $ENV{WINBINDD_VALGRIND});
		}

		exec(@preargs, $self->binpath("winbindd"), "-F", "-S", "--no-process-group", "-s", $env_vars->{SERVERCONFFILE}, @optargs) or die("Unable to start winbindd: $!");
	}
	write_pid($env_vars, "winbindd", $pid);
	print "DONE\n";

	unlink($env_vars->{SMBD_TEST_LOG});
	print "STARTING SMBD...";
	$pid = fork();
	if ($pid == 0) {
		open STDOUT, ">$env_vars->{SMBD_TEST_LOG}";
		open STDERR, '>&STDOUT';

		SocketWrapper::set_default_iface($env_vars->{SOCKET_WRAPPER_DEFAULT_IFACE});

		$ENV{WINBINDD_SOCKET_DIR} = $env_vars->{WINBINDD_SOCKET_DIR};

		$ENV{NSS_WRAPPER_PASSWD} = $env_vars->{NSS_WRAPPER_PASSWD};
		$ENV{NSS_WRAPPER_GROUP} = $env_vars->{NSS_WRAPPER_GROUP};
		$ENV{NSS_WRAPPER_WINBIND_SO_PATH} = $env_vars->{NSS_WRAPPER_WINBIND_SO_PATH};

		if ($smbd ne "yes") {
			$SIG{USR1} = $SIG{ALRM} = $SIG{INT} = $SIG{QUIT} = $SIG{TERM} = sub {
				my $signame = shift;
				print("Skip smbd received signal $signame");
				exit 0;
			};
			sleep($maxtime);
			exit 0;
		}

		$ENV{MAKE_TEST_BINARY} = $self->binpath("smbd");
		my @optargs = ("-d0");
		if (defined($ENV{SMBD_OPTIONS})) {
			@optargs = split(/ /, $ENV{SMBD_OPTIONS});
		}
		my @preargs = ($self->binpath("timelimit"), $maxtime);
		if(defined($ENV{SMBD_VALGRIND})) {
			@preargs = split(/ /,$ENV{SMBD_VALGRIND});
		}
		exec(@preargs, $self->binpath("smbd"), "-F", "-S", "--no-process-group", "-s", $env_vars->{SERVERCONFFILE}, @optargs) or die("Unable to start smbd: $!");
	}
	write_pid($env_vars, "smbd", $pid);
	print "DONE\n";

	return 0;
}

sub create_clientconf($$$)
{
	my ($self, $prefix, $domain) = @_;

	my $lockdir = "$prefix/locks";
	my $logdir = "$prefix/logs";
	my $piddir = "$prefix/pid";
	my $privatedir = "$prefix/private";
	my $conffile = "$prefix/smb.conf";

	my $torture_interfaces='127.0.0.6/8,127.0.0.7/8,127.0.0.8/8,127.0.0.9/8,127.0.0.10/8,127.0.0.11/8';
	open(CONF, ">$conffile");
	print CONF "
[global]
	workgroup = $domain

	private dir = $privatedir
	pid directory = $piddir
	lock directory = $lockdir
	log file = $logdir/log.\%m
	log level = 0

	name resolve order = bcast

	netbios name = TORTURE_6
	interfaces = $torture_interfaces
	panic action = $RealBin/gdb_backtrace \%d %\$(MAKE_TEST_BINARY)

	passdb backend = tdbsam
	";
	close(CONF);
}

sub provision($$$$$$)
{
	my ($self, $prefix, $server, $swiface, $password, $extra_options) = @_;

	##
	## setup the various environment variables we need
	##

	my %ret = ();
	my $server_ip = "127.0.0.$swiface";
	my $domain = "SAMBA-TEST";

	my $unix_name = ($ENV{USER} or $ENV{LOGNAME} or `PATH=/usr/ucb:$ENV{PATH} whoami`);
	chomp $unix_name;
	my $unix_uid = $>;
	my $unix_gids_str = $);
	my @unix_gids = split(" ", $unix_gids_str);

	my $prefix_abs = abs_path($prefix);
	my $bindir_abs = abs_path($self->{bindir});

	my @dirs = ();

	my $shrdir="$prefix_abs/share";
	push(@dirs,$shrdir);

	my $libdir="$prefix_abs/lib";
	push(@dirs,$libdir);

	my $piddir="$prefix_abs/pid";
	push(@dirs,$piddir);

	my $privatedir="$prefix_abs/private";
	push(@dirs,$privatedir);

	my $lockdir="$prefix_abs/lockdir";
	push(@dirs,$lockdir);

	my $logdir="$prefix_abs/logs";
	push(@dirs,$logdir);

	# this gets autocreated by winbindd
	my $wbsockdir="$prefix_abs/winbindd";
	my $wbsockprivdir="$lockdir/winbindd_privileged";

	## 
	## create the test directory layout
	##
	die ("prefix_abs = ''") if $prefix_abs eq "";
	die ("prefix_abs = '/'") if $prefix_abs eq "/";

	mkdir($prefix_abs, 0777);
	print "CREATE TEST ENVIRONMENT IN '$prefix'...";
	system("rm -rf $prefix_abs/*");
	mkdir($_, 0777) foreach(@dirs);

	my $conffile="$libdir/server.conf";

	my $nss_wrapper_pl = "$ENV{PERL} $RealBin/../lib/nss_wrapper/nss_wrapper.pl";
	my $nss_wrapper_passwd = "$privatedir/passwd";
	my $nss_wrapper_group = "$privatedir/group";

	open(CONF, ">$conffile") or die("Unable to open $conffile");
	print CONF "
[global]
	netbios name = $server
	interfaces = $server_ip/8
	bind interfaces only = yes
	panic action = $RealBin/gdb_backtrace %d %\$(MAKE_TEST_BINARY)

	workgroup = $domain

	private dir = $privatedir
	pid directory = $piddir
	lock directory = $lockdir
	log file = $logdir/log.\%m
	log level = 0

	name resolve order = bcast

	state directory = $lockdir
	cache directory = $lockdir

	passdb backend = tdbsam

	time server = yes

	add user script =		$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type passwd --action add --name %u
	add group script =		$nss_wrapper_pl --group_path  $nss_wrapper_group  --type group  --action add --name %g
	add machine script =		$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type passwd --action add --name %u
	add user to group script =	$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type member --action add --member %u --name %g --group_path $nss_wrapper_group
	delete user script =		$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type passwd --action delete --name %u
	delete group script =		$nss_wrapper_pl --group_path  $nss_wrapper_group  --type group  --action delete --name %g
	delete user from group script = $nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type member --action delete --member %u --name %g --group_path $nss_wrapper_group

	kernel oplocks = no
	kernel change notify = no

	syslog = no
	printing = bsd
	printcap name = /dev/null

	winbindd:socket dir = $wbsockdir
	idmap uid = 100000-200000
	idmap gid = 100000-200000

#	min receivefile size = 4000

	read only = no
	smbd:sharedelay = 100000
	smbd:writetimeupdatedelay = 500000
	map hidden = yes
	map system = yes
	create mask = 755
	vfs objects = $bindir_abs/xattr_tdb.so $bindir_abs/streams_depot.so

	# Begin extra options
	$extra_options
	# End extra options

	#Include user defined custom parameters if set
";

	if (defined($ENV{INCLUDE_CUSTOM_CONF})) {
		print CONF "\t$ENV{INCLUDE_CUSTOM_CONF}\n";
	}

	print CONF "
[tmp]
	path = $shrdir
[hideunread]
	copy = tmp
	hide unreadable = yes
[hideunwrite]
	copy = tmp
	hide unwriteable files = yes
[print1]
	copy = tmp
	printable = yes
	printing = vlp
	print command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb print %p %s
	lpq command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lpq %p
	lp rm command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lprm %p %j
	lp pause command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lppause %p %j
	lp resume command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lpresume %p %j
	queue pause command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb queuepause %p
	queue resume command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb queueresume %p

[print2]
	copy = print1
[print3]
	copy = print1
[print4]
	copy = print1
	";
	close(CONF);

	##
	## create a test account
	##

	open(PASSWD, ">$nss_wrapper_passwd") or die("Unable to open $nss_wrapper_passwd");
	print PASSWD "nobody:x:65534:65533:nobody gecos:$prefix_abs:/bin/false
root:x:65533:65532:root gecos:$prefix_abs:/bin/false
$unix_name:x:$unix_uid:$unix_gids[0]:$unix_name gecos:$prefix_abs:/bin/false
";
	close(PASSWD);

	open(GROUP, ">$nss_wrapper_group") or die("Unable to open $nss_wrapper_group");
	print GROUP "nobody:x:65533:
nogroup:x:65534:nobody
root:x:65532:
$unix_name-group:x:$unix_gids[0]:
";
	close(GROUP);

	$ENV{NSS_WRAPPER_PASSWD} = $nss_wrapper_passwd;
	$ENV{NSS_WRAPPER_GROUP} = $nss_wrapper_group;

	open(PWD, "|".$self->binpath("smbpasswd")." -c $conffile -L -s -a $unix_name >/dev/null");
	print PWD "$password\n$password\n";
	close(PWD) or die("Unable to set password for test account");

	delete $ENV{NSS_WRAPPER_PASSWD};
	delete $ENV{NSS_WRAPPER_GROUP};

	print "DONE\n";

	$ret{SERVER_IP} = $server_ip;
	$ret{NMBD_TEST_LOG} = "$prefix/nmbd_test.log";
	$ret{NMBD_TEST_LOG_POS} = 0;
	$ret{WINBINDD_TEST_LOG} = "$prefix/winbindd_test.log";
	$ret{WINBINDD_TEST_LOG_POS} = 0;
	$ret{SMBD_TEST_LOG} = "$prefix/smbd_test.log";
	$ret{SMBD_TEST_LOG_POS} = 0;
	$ret{SERVERCONFFILE} = $conffile;
	$ret{CONFIGURATION} ="-s $conffile";
	$ret{SERVER} = $server;
	$ret{USERNAME} = $unix_name;
	$ret{DOMAIN} = $domain;
	$ret{NETBIOSNAME} = $server;
	$ret{PASSWORD} = $password;
	$ret{PIDDIR} = $piddir;
	$ret{WINBINDD_SOCKET_DIR} = $wbsockdir;
	$ret{WINBINDD_PRIV_PIPE_DIR} = $wbsockprivdir;
	$ret{SOCKET_WRAPPER_DEFAULT_IFACE} = $swiface;
	$ret{NSS_WRAPPER_PASSWD} = $nss_wrapper_passwd;
	$ret{NSS_WRAPPER_GROUP} = $nss_wrapper_group;
	$ret{NSS_WRAPPER_WINBIND_SO_PATH} = $ENV{NSS_WRAPPER_WINBIND_SO_PATH};

	return \%ret;
}

sub wait_for_start($$)
{
	my ($self, $envvars) = @_;

	# give time for nbt server to register its names
	print "delaying for nbt name registration\n";
	sleep(10);
	# This will return quickly when things are up, but be slow if we need to wait for (eg) SSL init 
	system($self->binpath("nmblookup") ." $envvars->{CONFIGURATION} -U $envvars->{SERVER_IP} __SAMBA__");
	system($self->binpath("nmblookup") ." $envvars->{CONFIGURATION} __SAMBA__");
	system($self->binpath("nmblookup") ." $envvars->{CONFIGURATION} -U 127.255.255.255 __SAMBA__");
	system($self->binpath("nmblookup") ." $envvars->{CONFIGURATION} -U $envvars->{SERVER_IP} $envvars->{SERVER}");
	system($self->binpath("nmblookup") ." $envvars->{CONFIGURATION} $envvars->{SERVER}");
	# make sure smbd is also up set
	print "wait for smbd\n";
	system($self->binpath("smbclient") ." $envvars->{CONFIGURATION} -L $envvars->{SERVER_IP} -U% -p 139 | head -2");
	system($self->binpath("smbclient") ." $envvars->{CONFIGURATION} -L $envvars->{SERVER_IP} -U% -p 139 | head -2");

	print $self->getlog_env($envvars);
}

1;
