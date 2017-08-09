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
	my ($classname, $bindir, $srcdir) = @_;
	my $self = { bindir => $bindir,
		     srcdir => $srcdir
	};
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
	
	if ($envname eq "s3dc") {
		return $self->setup_dc("$path/s3dc");
	} elsif ($envname eq "secshare") {
		return $self->setup_secshare("$path/secshare");
	} elsif ($envname eq "secserver") {
		if (not defined($self->{vars}->{s3dc})) {
			$self->setup_dc("$path/s3dc");
		}
		return $self->setup_secserver("$path/secserver", $self->{vars}->{s3dc});
	} elsif ($envname eq "member") {
		if (not defined($self->{vars}->{s3dc})) {
			$self->setup_dc("$path/s3dc");
		}
		return $self->setup_member("$path/member", $self->{vars}->{s3dc});
	} else {
		return undef;
	}
}

sub setup_dc($$)
{
	my ($self, $path) = @_;

	print "PROVISIONING S3DC...";

	my $s3dc_options = "
	domain master = yes
	domain logons = yes
	lanman auth = yes
	raw NTLMv2 auth = yes
";

	my $vars = $self->provision($path,
				    "LOCALS3DC2",
				    2,
				    "locals3dc2pass",
				    $s3dc_options);

	$self->check_or_start($vars,
			      ($ENV{SMBD_MAXTIME} or 2700),
			       "yes", "yes", "yes");

	$self->wait_for_start($vars);

	$vars->{DC_SERVER} = $vars->{SERVER};
	$vars->{DC_SERVER_IP} = $vars->{SERVER_IP};
	$vars->{DC_NETBIOSNAME} = $vars->{NETBIOSNAME};
	$vars->{DC_USERNAME} = $vars->{USERNAME};
	$vars->{DC_PASSWORD} = $vars->{PASSWORD};

	$self->{vars}->{s3dc} = $vars;

	return $vars;
}

sub setup_member($$$)
{
	my ($self, $prefix, $s3dcvars) = @_;

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
	$cmd .= "$net join $ret->{CONFIGURATION} $s3dcvars->{DOMAIN} member";
	$cmd .= " -U$s3dcvars->{USERNAME}\%$s3dcvars->{PASSWORD}";

	system($cmd) == 0 or die("Join failed\n$cmd");

	$self->check_or_start($ret,
			      ($ENV{SMBD_MAXTIME} or 2700),
			       "yes", "yes", "yes");

	$self->wait_for_start($ret);

	$ret->{DC_SERVER} = $s3dcvars->{SERVER};
	$ret->{DC_SERVER_IP} = $s3dcvars->{SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $s3dcvars->{NETBIOSNAME};
	$ret->{DC_USERNAME} = $s3dcvars->{USERNAME};
	$ret->{DC_PASSWORD} = $s3dcvars->{PASSWORD};

	return $ret;
}

sub setup_secshare($$)
{
	my ($self, $path) = @_;

	print "PROVISIONING server with security=share...";

	my $secshare_options = "
	security = share
	lanman auth = yes
";

	my $vars = $self->provision($path,
				    "LOCALSHARE4",
				    4,
				    "local4pass",
				    $secshare_options);

	$self->check_or_start($vars,
			      ($ENV{SMBD_MAXTIME} or 2700),
			       "yes", "no", "yes");

	$self->wait_for_start($vars);

	$self->{vars}->{secshare} = $vars;

	return $vars;
}

sub setup_secserver($$$)
{
	my ($self, $prefix, $s3dcvars) = @_;

	print "PROVISIONING server with security=server...";

	my $secserver_options = "
	security = server
        password server = $s3dcvars->{SERVER_IP}
	client ntlmv2 auth = no
";

	my $ret = $self->provision($prefix,
				   "LOCALSERVER5",
				   5,
				   "localserver5pass",
				   $secserver_options);

	$ret or die("Unable to provision");

	$self->check_or_start($ret,
			      ($ENV{SMBD_MAXTIME} or 2700),
			       "yes", "no", "yes");

	$self->wait_for_start($ret);

	$ret->{DC_SERVER} = $s3dcvars->{SERVER};
	$ret->{DC_SERVER_IP} = $s3dcvars->{SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $s3dcvars->{NETBIOSNAME};
	$ret->{DC_USERNAME} = $s3dcvars->{USERNAME};
	$ret->{DC_PASSWORD} = $s3dcvars->{PASSWORD};

	return $ret;
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
		$ENV{NMBD_SOCKET_DIR} = $env_vars->{NMBD_SOCKET_DIR};

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

		exec(@preargs, $self->binpath("nmbd"), "-F", "--no-process-group", "-S", "-s", $env_vars->{SERVERCONFFILE}, @optargs) or die("Unable to start nmbd: $!");
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
		$ENV{NMBD_SOCKET_DIR} = $env_vars->{NMBD_SOCKET_DIR};

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

		exec(@preargs, $self->binpath("winbindd"), "-F", "--no-process-group", "-s", $env_vars->{SERVERCONFFILE}, @optargs) or die("Unable to start winbindd: $!");
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
		$ENV{NMBD_SOCKET_DIR} = $env_vars->{NMBD_SOCKET_DIR};

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
		exec(@preargs, $self->binpath("smbd"), "-F", "--no-process-group", "-s", $env_vars->{SERVERCONFFILE}, @optargs) or die("Unable to start smbd: $!");
	}
	write_pid($env_vars, "smbd", $pid);
	print "DONE\n";

	return 0;
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
	my $vfs_modulesdir_abs = ($ENV{VFSLIBDIR} or $bindir_abs);

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

	my $eventlogdir="$prefix_abs/lockdir/eventlog";
	push(@dirs,$eventlogdir);

	my $logdir="$prefix_abs/logs";
	push(@dirs,$logdir);

	my $driver32dir="$shrdir/W32X86";
	push(@dirs,$driver32dir);

	my $driver64dir="$shrdir/x64";
	push(@dirs,$driver64dir);

	my $driver40dir="$shrdir/WIN40";
	push(@dirs,$driver40dir);

	my $ro_shrdir="$shrdir/root-tmp";
	push(@dirs,$ro_shrdir);

	my $msdfs_shrdir="$shrdir/msdfsshare";
	push(@dirs,$msdfs_shrdir);

	my $msdfs_deeppath="$msdfs_shrdir/deeppath";
	push(@dirs,$msdfs_deeppath);

	# this gets autocreated by winbindd
	my $wbsockdir="$prefix_abs/winbindd";
	my $wbsockprivdir="$lockdir/winbindd_privileged";

	my $nmbdsockdir="$prefix_abs/nmbd";
	unlink($nmbdsockdir);

	## 
	## create the test directory layout
	##
	die ("prefix_abs = ''") if $prefix_abs eq "";
	die ("prefix_abs = '/'") if $prefix_abs eq "/";

	mkdir($prefix_abs, 0777);
	print "CREATE TEST ENVIRONMENT IN '$prefix'...";
	system("rm -rf $prefix_abs/*");
	mkdir($_, 0777) foreach(@dirs);

	##
	## create ro and msdfs share layout
	##

	chmod 0755, $ro_shrdir;
	my $unreadable_file = "$ro_shrdir/unreadable_file";
	open(UNREADABLE_FILE, ">$unreadable_file") or die("Unable to open $unreadable_file");
	close(UNREADABLE_FILE);
	chmod 0600, $unreadable_file;

	my $msdfs_target = "$ro_shrdir/msdfs-target";
	open(MSDFS_TARGET, ">$msdfs_target") or die("Unable to open $msdfs_target");
	close(MSDFS_TARGET);
	chmod 0666, $msdfs_target;
	symlink "msdfs:$server_ip\\ro-tmp", "$msdfs_shrdir/msdfs-src1";
	symlink "msdfs:$server_ip\\ro-tmp", "$msdfs_shrdir/deeppath/msdfs-src2";

	my $conffile="$libdir/server.conf";

	my $nss_wrapper_pl = "$ENV{PERL} $self->{srcdir}/lib/nss_wrapper/nss_wrapper.pl";
	my $nss_wrapper_passwd = "$privatedir/passwd";
	my $nss_wrapper_group = "$privatedir/group";

	my $mod_printer_pl = "$ENV{PERL} $self->{srcdir}/source3/script/tests/printing/modprinter.pl";

	my @eventlog_list = ("dns server", "application");

	##
	## calculate uids and gids
	##

	my ($max_uid, $max_gid);
	my ($uid_nobody, $uid_root);
	my ($gid_nobody, $gid_nogroup, $gid_root, $gid_domusers);

	if ($unix_uid < 0xffff - 2) {
		$max_uid = 0xffff;
	} else {
		$max_uid = $unix_uid;
	}

	$uid_root = $max_uid - 1;
	$uid_nobody = $max_uid - 2;

	if ($unix_gids[0] < 0xffff - 3) {
		$max_gid = 0xffff;
	} else {
		$max_gid = $unix_gids[0];
	}

	$gid_nobody = $max_gid - 1;
	$gid_nogroup = $max_gid - 2;
	$gid_root = $max_gid - 3;
	$gid_domusers = $max_gid - 4;

	##
	## create conffile
	##

	open(CONF, ">$conffile") or die("Unable to open $conffile");
	print CONF "
[global]
	netbios name = $server
	interfaces = $server_ip/8
	bind interfaces only = yes
	panic action = $self->{srcdir}/selftest/gdb_backtrace %d %\$(MAKE_TEST_BINARY)

	workgroup = $domain

	private dir = $privatedir
	pid directory = $piddir
	lock directory = $lockdir
	log file = $logdir/log.\%m
	log level = 0
	debug pid = yes

	name resolve order = bcast

	state directory = $lockdir
	cache directory = $lockdir

	passdb backend = tdbsam

	time server = yes

	add user script =		$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type passwd --action add --name %u --gid $gid_nogroup
	add group script =		$nss_wrapper_pl --group_path  $nss_wrapper_group  --type group  --action add --name %g
	add machine script =		$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type passwd --action add --name %u --gid $gid_nogroup
	add user to group script =	$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type member --action add --member %u --name %g --group_path $nss_wrapper_group
	delete user script =		$nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type passwd --action delete --name %u
	delete group script =		$nss_wrapper_pl --group_path  $nss_wrapper_group  --type group  --action delete --name %g
	delete user from group script = $nss_wrapper_pl --passwd_path $nss_wrapper_passwd --type member --action delete --member %u --name %g --group_path $nss_wrapper_group

	addprinter command =		$mod_printer_pl -a -s $conffile --
	deleteprinter command =		$mod_printer_pl -d -s $conffile --

	eventlog list = application \"dns server\"

	kernel oplocks = no
	kernel change notify = no

	syslog = no
	printing = bsd
	printcap name = /dev/null

	winbindd:socket dir = $wbsockdir
	nmbd:socket dir = $nmbdsockdir
	idmap config * : range = 100000-200000
	winbind enum users = yes
	winbind enum groups = yes

#	min receivefile size = 4000

	max protocol = SMB2
	read only = no
	server signing = auto

	smbd:sharedelay = 100000
#	smbd:writetimeupdatedelay = 500000
	map hidden = no
	map system = no
	map readonly = no
	store dos attributes = yes
	create mask = 755
	vfs objects = $vfs_modulesdir_abs/xattr_tdb.so $vfs_modulesdir_abs/streams_depot.so

	printing = vlp
	print command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb print %p %s
	lpq command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lpq %p
	lp rm command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lprm %p %j
	lp pause command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lppause %p %j
	lp resume command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb lpresume %p %j
	queue pause command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb queuepause %p
	queue resume command = $bindir_abs/vlp tdbfile=$lockdir/vlp.tdb queueresume %p
	lpq cache time = 0

	ncalrpc dir = $lockdir/ncalrpc
	rpc_server:epmapper = embedded

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
	comment = smb username is [%U]
	vfs objects = $vfs_modulesdir_abs/dirsort.so
[tmpguest]
	path = $shrdir
        guest ok = yes
[guestonly]
	path = $shrdir
        guest only = yes
        guest ok = yes
[forceuser]
	path = $shrdir
        force user = $unix_name
        guest ok = yes
[forcegroup]
	path = $shrdir
        force group = nogroup
        guest ok = yes
[ro-tmp]
	path = $ro_shrdir
	guest ok = yes
[msdfs-share]
	path = $msdfs_shrdir
	msdfs root = yes
	guest ok = yes
[hideunread]
	copy = tmp
	hide unreadable = yes
[tmpcase]
	copy = tmp
	case sensitive = yes
[hideunwrite]
	copy = tmp
	hide unwriteable files = yes
[print1]
	copy = tmp
	printable = yes

[print2]
	copy = print1
[print3]
	copy = print1
[lp]
	copy = print1
[print\$]
	copy = tmp
	";
	close(CONF);

	##
	## create a test account
	##

	open(PASSWD, ">$nss_wrapper_passwd") or die("Unable to open $nss_wrapper_passwd");
	print PASSWD "nobody:x:$uid_nobody:$gid_nobody:nobody gecos:$prefix_abs:/bin/false
$unix_name:x:$unix_uid:$unix_gids[0]:$unix_name gecos:$prefix_abs:/bin/false
";
	if ($unix_uid != 0) {
		print PASSWD "root:x:$uid_root:$gid_root:root gecos:$prefix_abs:/bin/false";
	}
	close(PASSWD);

	open(GROUP, ">$nss_wrapper_group") or die("Unable to open $nss_wrapper_group");
	print GROUP "nobody:x:$gid_nobody:
nogroup:x:$gid_nogroup:nobody
$unix_name-group:x:$unix_gids[0]:
domusers:X:$gid_domusers:
";
	if ($unix_gids[0] != 0) {
		print GROUP "root:x:$gid_root:";
	}

	close(GROUP);

	foreach my $evlog (@eventlog_list) {
		my $evlogtdb = "$eventlogdir/$evlog.tdb";
		open(EVENTLOG, ">$evlogtdb") or die("Unable to open $evlogtdb");
		close(EVENTLOG);
	}

	$ENV{NSS_WRAPPER_PASSWD} = $nss_wrapper_passwd;
	$ENV{NSS_WRAPPER_GROUP} = $nss_wrapper_group;

	open(PWD, "|".$self->binpath("smbpasswd")." -c $conffile -L -s -a $unix_name >/dev/null");
	print PWD "$password\n$password\n";
	close(PWD) or die("Unable to set password for test account");

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
	$ret{USERID} = $unix_uid;
	$ret{DOMAIN} = $domain;
	$ret{NETBIOSNAME} = $server;
	$ret{PASSWORD} = $password;
	$ret{PIDDIR} = $piddir;
	$ret{WINBINDD_SOCKET_DIR} = $wbsockdir;
	$ret{WINBINDD_PRIV_PIPE_DIR} = $wbsockprivdir;
	$ret{NMBD_SOCKET_DIR} = $nmbdsockdir;
	$ret{SOCKET_WRAPPER_DEFAULT_IFACE} = $swiface;
	$ret{NSS_WRAPPER_PASSWD} = $nss_wrapper_passwd;
	$ret{NSS_WRAPPER_GROUP} = $nss_wrapper_group;
	$ret{NSS_WRAPPER_WINBIND_SO_PATH} = $ENV{NSS_WRAPPER_WINBIND_SO_PATH};
	$ret{LOCAL_PATH} = "$shrdir";

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

	# Ensure we have domain users mapped.
	system($self->binpath("net") ." $envvars->{CONFIGURATION} groupmap add rid=513 unixgroup=domusers type=domain");

	print $self->getlog_env($envvars);
}

1;
