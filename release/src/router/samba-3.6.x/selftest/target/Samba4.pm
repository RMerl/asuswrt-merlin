#!/usr/bin/perl
# Bootstrap Samba and run a number of tests against it.
# Copyright (C) 2005-2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later.

package Samba4;

use strict;
use Cwd qw(abs_path);
use FindBin qw($RealBin);
use POSIX;
use SocketWrapper;

sub new($$$$$) {
	my ($classname, $bindir, $ldap, $srcdir, $exeext) = @_;
	$exeext = "" unless defined($exeext);
	my $self = {
		vars => {},
		ldap => $ldap,
		bindir => $bindir,
		srcdir => $srcdir,
		exeext => $exeext
	};
	bless $self;
	return $self;
}

sub bindir_path($$) {
	my ($self, $path) = @_;

	my $valpath = "$self->{bindir}/$path$self->{exeext}";

	return $valpath if (-f $valpath);
	return $path;
}

sub scriptdir_path($$) {
	my ($self, $path) = @_;
	return "$self->{srcdir}/source4/scripting/$path";
}

sub openldap_start($$$) {
}

sub slapd_start($$)
{
	my $count = 0;
	my ($self, $env_vars) = @_;
	my $ldbsearch = $self->bindir_path("ldbsearch");

	my $uri = $env_vars->{LDAP_URI};

	if (system("$ldbsearch -H $uri -s base -b \"\" supportedLDAPVersion > /dev/null") == 0) {
	    print "A SLAPD is still listening to $uri before we started the LDAP backend.  Aborting!";
	    return 1;
	}
	# running slapd in the background means it stays in the same process group, so it can be
	# killed by timelimit
	if ($self->{ldap} eq "fedora-ds") {
	        system("$ENV{FEDORA_DS_ROOT}/sbin/ns-slapd -D $env_vars->{FEDORA_DS_DIR} -d0 -i $env_vars->{FEDORA_DS_PIDFILE}> $env_vars->{LDAPDIR}/logs 2>&1 &");
	} elsif ($self->{ldap} eq "openldap") {
	        system("$ENV{OPENLDAP_SLAPD} -d0 -F $env_vars->{SLAPD_CONF_D} -h $uri > $env_vars->{LDAPDIR}/logs 2>&1 &");
	}
	while (system("$ldbsearch -H $uri -s base -b \"\" supportedLDAPVersion > /dev/null") != 0) {
		$count++;
		if ($count > 40) {
			$self->slapd_stop($env_vars);
			return 0;
		}
		sleep(1);
	}
	return 1;
}

sub slapd_stop($$)
{
	my ($self, $envvars) = @_;
	if ($self->{ldap} eq "fedora-ds") {
		system("$envvars->{LDAPDIR}/slapd-$envvars->{LDAP_INSTANCE}/stop-slapd");
	} elsif ($self->{ldap} eq "openldap") {
		unless (open(IN, "<$envvars->{OPENLDAP_PIDFILE}")) {
			warn("unable to open slapd pid file: $envvars->{OPENLDAP_PIDFILE}");
			return 0;
		}
		kill 9, <IN>;
		close(IN);
	}
	return 1;
}

sub check_or_start($$$)
{
	my ($self, $env_vars, $max_time) = @_;
	return 0 if ( -p $env_vars->{SAMBA_TEST_FIFO});

	unlink($env_vars->{SAMBA_TEST_FIFO});
	POSIX::mkfifo($env_vars->{SAMBA_TEST_FIFO}, 0700);
	unlink($env_vars->{SAMBA_TEST_LOG});
	
	my $pwd = `pwd`;
	print "STARTING SAMBA for $ENV{ENVNAME}\n";
	my $pid = fork();
	if ($pid == 0) {
		open STDIN, $env_vars->{SAMBA_TEST_FIFO};
		# we want out from samba to go to the log file, but also
		# to the users terminal when running 'make test' on the command
		# line. This puts it on stderr on the terminal
		open STDOUT, "| tee $env_vars->{SAMBA_TEST_LOG} 1>&2";
		open STDERR, '>&STDOUT';

		SocketWrapper::set_default_iface($env_vars->{SOCKET_WRAPPER_DEFAULT_IFACE});

		my $valgrind = "";
		if (defined($ENV{SAMBA_VALGRIND})) {
		    $valgrind = $ENV{SAMBA_VALGRIND};
		}

		$ENV{KRB5_CONFIG} = $env_vars->{KRB5_CONFIG};
		$ENV{WINBINDD_SOCKET_DIR} = $env_vars->{WINBINDD_SOCKET_DIR};

		$ENV{NSS_WRAPPER_PASSWD} = $env_vars->{NSS_WRAPPER_PASSWD};
		$ENV{NSS_WRAPPER_GROUP} = $env_vars->{NSS_WRAPPER_GROUP};

		$ENV{UID_WRAPPER} = "1";

		# Start slapd before samba, but with the fifo on stdin
		if (defined($self->{ldap})) {
		 	unless($self->slapd_start($env_vars)) {
				warn("couldn't start slapd (main run)");
				return undef;
			}
		}

		my $optarg = "";
		if (defined($max_time)) {
			$optarg = "--maximum-runtime=$max_time ";
		}
		if (defined($ENV{SAMBA_OPTIONS})) {
			$optarg.= " $ENV{SAMBA_OPTIONS}";
		}
		my $samba = $self->bindir_path("samba");

		# allow selection of the process model using
		# the environment varibale SAMBA_PROCESS_MODEL
		# that allows us to change the process model for
		# individual machines in the build farm
		my $model = "single";
		if (defined($ENV{SAMBA_PROCESS_MODEL})) {
			$model = $ENV{SAMBA_PROCESS_MODEL};
		}
		chomp($pwd);
		my $cmdline = "$valgrind ${pwd}/$samba $optarg $env_vars->{CONFIGURATION} -M $model -i";
		my $ret = system("$cmdline");
		if ($ret == -1) {
			print "Unable to start $cmdline: $ret: $!\n";
			exit 1;
		}
		my $exit = ($ret >> 8);
		unlink($env_vars->{SAMBA_TEST_FIFO});
		if ($ret == 0) {
			print "$samba exited with no error\n";
			exit 0;
		} elsif ( $ret & 127 ) {
			print "$samba got signal ".($ret & 127)." and exits with $exit!\n";
		} else {
			print "$samba failed with status $exit!\n";
		}
		if ($exit == 0) {
			$exit = -1;
		}
		exit $exit;
	}
	print "DONE\n";

	open(DATA, ">$env_vars->{SAMBA_TEST_FIFO}");

	return $pid;
}

sub wait_for_start($$)
{
	my ($self, $testenv_vars) = @_;
	# give time for nbt server to register its names
	print "delaying for nbt name registration\n";
	sleep 2;

	# This will return quickly when things are up, but be slow if we
	# need to wait for (eg) SSL init
	my $nmblookup = $self->bindir_path("nmblookup");
	system("$nmblookup $testenv_vars->{CONFIGURATION} $testenv_vars->{SERVER}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} -U $testenv_vars->{SERVER_IP} $testenv_vars->{SERVER}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} $testenv_vars->{NETBIOSNAME}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} -U $testenv_vars->{SERVER_IP} $testenv_vars->{NETBIOSNAME}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} $testenv_vars->{NETBIOSALIAS}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} -U $testenv_vars->{SERVER_IP} $testenv_vars->{NETBIOSALIAS}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} $testenv_vars->{SERVER}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} -U $testenv_vars->{SERVER_IP} $testenv_vars->{SERVER}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} $testenv_vars->{NETBIOSNAME}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} -U $testenv_vars->{SERVER_IP} $testenv_vars->{NETBIOSNAME}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} $testenv_vars->{NETBIOSALIAS}");
	system("$nmblookup $testenv_vars->{CONFIGURATION} -U $testenv_vars->{SERVER_IP} $testenv_vars->{NETBIOSALIAS}");

	print $self->getlog_env($testenv_vars);
}

sub write_ldb_file($$$)
{
	my ($self, $file, $ldif) = @_;

	my $ldbadd = $self->bindir_path("ldbadd");
	open(LDIF, "|$ldbadd -H $file >/dev/null");
	print LDIF $ldif;
	return(close(LDIF));
}

sub add_wins_config($$)
{
	my ($self, $privatedir) = @_;

	return $self->write_ldb_file("$privatedir/wins_config.ldb", "
dn: name=TORTURE_11,CN=PARTNERS
objectClass: wreplPartner
name: TORTURE_11
address: 127.0.0.11
pullInterval: 0
pushChangeCount: 0
type: 0x3
");
}

sub mk_fedora_ds($$)
{
	my ($self, $ctx) = @_;

	#Make the subdirectory be as fedora DS would expect
	my $fedora_ds_dir = "$ctx->{ldapdir}/slapd-$ctx->{ldap_instance}";

	my $pidfile = "$fedora_ds_dir/logs/slapd-$ctx->{ldap_instance}.pid";

	return ($fedora_ds_dir, $pidfile);
}

sub mk_openldap($$)
{
	my ($self, $ctx) = @_;

	my $slapd_conf_d = "$ctx->{ldapdir}/slapd.d";
	my $pidfile = "$ctx->{ldapdir}/slapd.pid";

	return ($slapd_conf_d, $pidfile);
}

sub mk_keyblobs($$)
{
	my ($self, $tlsdir) = @_;

	#TLS and PKINIT crypto blobs
	my $dhfile = "$tlsdir/dhparms.pem";
	my $cafile = "$tlsdir/ca.pem";
	my $certfile = "$tlsdir/cert.pem";
	my $reqkdc = "$tlsdir/req-kdc.der";
	my $kdccertfile = "$tlsdir/kdc.pem";
	my $keyfile = "$tlsdir/key.pem";
	my $adminkeyfile = "$tlsdir/adminkey.pem";
	my $reqadmin = "$tlsdir/req-admin.der";
	my $admincertfile = "$tlsdir/admincert.pem";
	my $admincertupnfile = "$tlsdir/admincertupn.pem";

	mkdir($tlsdir, 0777);

	#This is specified here to avoid draining entropy on every run
	open(DHFILE, ">$dhfile");
	print DHFILE <<EOF;
-----BEGIN DH PARAMETERS-----
MGYCYQC/eWD2xkb7uELmqLi+ygPMKyVcpHUo2yCluwnbPutEueuxrG/Cys8j8wLO
svCN/jYNyR2NszOmg7ZWcOC/4z/4pWDVPUZr8qrkhj5MRKJc52MncfaDglvEdJrv
YX70obsCAQI=
-----END DH PARAMETERS-----
EOF
	close(DHFILE);

	#Likewise, we pregenerate the key material.  This allows the
	#other certificates to be pre-generated
	open(KEYFILE, ">$keyfile");
	print KEYFILE <<EOF;
-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQDKg6pAwCHUMA1DfHDmWhZfd+F0C+9Jxcqvpw9ii9En3E1uflpc
ol3+S9/6I/uaTmJHZre+DF3dTzb/UOZo0Zem8N+IzzkgoGkFafjXuT3BL5UPY2/H
6H+pPqVIRLOmrWImai359YyoKhFyo37Y6HPeU8QcZ+u2rS9geapIWfeuowIDAQAB
AoGAAqDLzFRR/BF1kpsiUfL4WFvTarCe9duhwj7ORc6fs785qAXuwUYAJ0Uvzmy6
HqoGv3t3RfmeHDmjcpPHsbOKnsOQn2MgmthidQlPBMWtQMff5zdoYNUFiPS0XQBq
szNW4PRjaA9KkLQVTwnzdXGkBSkn/nGxkaVu7OR3vJOBoo0CQQDO4upypesnbe6p
9/xqfZ2uim8IwV1fLlFClV7WlCaER8tsQF4lEi0XSzRdXGUD/dilpY88Nb+xok/X
8Z8OvgAXAkEA+pcLsx1gN7kxnARxv54jdzQjC31uesJgMKQXjJ0h75aUZwTNHmZQ
vPxi6u62YiObrN5oivkixwFNncT9MxTxVQJBAMaWUm2SjlLe10UX4Zdm1MEB6OsC
kVoX37CGKO7YbtBzCfTzJGt5Mwc1DSLA2cYnGJqIfSFShptALlwedot0HikCQAJu
jNKEKnbf+TdGY8Q0SKvTebOW2Aeg80YFkaTvsXCdyXrmdQcifw4WdO9KucJiDhSz
Y9hVapz7ykEJtFtWjLECQQDIlfc63I5ZpXfg4/nN4IJXUW6AmPVOYIA5215itgki
cSlMYli1H9MEXH0pQMGv5Qyd0OYIx2DDg96mZ+aFvqSG
-----END RSA PRIVATE KEY-----
EOF
	close(KEYFILE);

	open(ADMINKEYFILE, ">$adminkeyfile");

	print ADMINKEYFILE <<EOF;
-----BEGIN RSA PRIVATE KEY-----
MIICXQIBAAKBgQD0+OL7TQBj0RejbIH1+g5GeRaWaM9xF43uE5y7jUHEsi5owhZF
5iIoHZeeL6cpDF5y1BZRs0JlA1VqMry1jjKlzFYVEMMFxB6esnXhl0Jpip1JkUMM
XLOP1m/0dqayuHBWozj9f/cdyCJr0wJIX1Z8Pr+EjYRGPn/MF0xdl3JRlwIDAQAB
AoGAP8mjCP628Ebc2eACQzOWjgEvwYCPK4qPmYOf1zJkArzG2t5XAGJ5WGrENRuB
cm3XFh1lpmaADl982UdW3gul4gXUy6w4XjKK4vVfhyHj0kZ/LgaXUK9BAGhroJ2L
osIOUsaC6jdx9EwSRctwdlF3wWJ8NK0g28AkvIk+FlolW4ECQQD7w5ouCDnf58CN
u4nARx4xv5XJXekBvOomkCQAmuOsdOb6b9wn3mm2E3au9fueITjb3soMR31AF6O4
eAY126rXAkEA+RgHzybzZEP8jCuznMqoN2fq/Vrs6+W3M8/G9mzGEMgLLpaf2Jiz
I9tLZ0+OFk9tkRaoCHPfUOCrVWJZ7Y53QQJBAMhoA6rw0WDyUcyApD5yXg6rusf4
ASpo/tqDkqUIpoL464Qe1tjFqtBM3gSXuhs9xsz+o0bzATirmJ+WqxrkKTECQHt2
OLCpKqwAspU7N+w32kaUADoRLisCEdrhWklbwpQgwsIVsCaoEOpt0CLloJRYTANE
yoZeAErTALjyZYZEPcECQQDlUi0N8DFxQ/lOwWyR3Hailft+mPqoPCa8QHlQZnlG
+cfgNl57YHMTZFwgUVFRdJNpjH/WdZ5QxDcIVli0q+Ko
-----END RSA PRIVATE KEY-----
EOF

	#generated with
	# hxtool issue-certificate --self-signed --issue-ca \
	# --ca-private-key="FILE:$KEYFILE" \
	# --subject="CN=CA,DC=samba,DC=example,DC=com" \
	# --certificate="FILE:$CAFILE" --lifetime="25 years"

	open(CAFILE, ">$cafile");
	print CAFILE <<EOF;
-----BEGIN CERTIFICATE-----
MIICcTCCAdqgAwIBAgIUaBPmjnPVqyFqR5foICmLmikJTzgwCwYJKoZIhvcNAQEFMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDgwMzAxMTIyMzEyWhgPMjAzMzAyMjQx
MjIzMTJaMFIxEzARBgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxl
MRUwEwYKCZImiZPyLGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMIGfMA0GCSqGSIb3DQEBAQUA
A4GNADCBiQKBgQDKg6pAwCHUMA1DfHDmWhZfd+F0C+9Jxcqvpw9ii9En3E1uflpcol3+S9/6
I/uaTmJHZre+DF3dTzb/UOZo0Zem8N+IzzkgoGkFafjXuT3BL5UPY2/H6H+pPqVIRLOmrWIm
ai359YyoKhFyo37Y6HPeU8QcZ+u2rS9geapIWfeuowIDAQABo0IwQDAOBgNVHQ8BAf8EBAMC
AaYwHQYDVR0OBBYEFMLZufegDKLZs0VOyFXYK1L6M8oyMA8GA1UdEwEB/wQFMAMBAf8wDQYJ
KoZIhvcNAQEFBQADgYEAAZJbCAAkaqgFJ0xgNovn8Ydd0KswQPjicwiODPgw9ZPoD2HiOUVO
yYDRg/dhFF9y656OpcHk4N7qZ2sl3RlHkzDu+dseETW+CnKvQIoXNyeARRJSsSlwrwcoD4JR
HTLk2sGigsWwrJ2N99sG/cqSJLJ1MFwLrs6koweBnYU0f/g=
-----END CERTIFICATE-----
EOF

	#generated with GNUTLS internally in Samba.

	open(CERTFILE, ">$certfile");
	print CERTFILE <<EOF;
-----BEGIN CERTIFICATE-----
MIICYTCCAcygAwIBAgIE5M7SRDALBgkqhkiG9w0BAQUwZTEdMBsGA1UEChMUU2Ft
YmEgQWRtaW5pc3RyYXRpb24xNDAyBgNVBAsTK1NhbWJhIC0gdGVtcG9yYXJ5IGF1
dG9nZW5lcmF0ZWQgY2VydGlmaWNhdGUxDjAMBgNVBAMTBVNhbWJhMB4XDTA2MDgw
NDA0MzY1MloXDTA4MDcwNDA0MzY1MlowZTEdMBsGA1UEChMUU2FtYmEgQWRtaW5p
c3RyYXRpb24xNDAyBgNVBAsTK1NhbWJhIC0gdGVtcG9yYXJ5IGF1dG9nZW5lcmF0
ZWQgY2VydGlmaWNhdGUxDjAMBgNVBAMTBVNhbWJhMIGcMAsGCSqGSIb3DQEBAQOB
jAAwgYgCgYDKg6pAwCHUMA1DfHDmWhZfd+F0C+9Jxcqvpw9ii9En3E1uflpcol3+
S9/6I/uaTmJHZre+DF3dTzb/UOZo0Zem8N+IzzkgoGkFafjXuT3BL5UPY2/H6H+p
PqVIRLOmrWImai359YyoKhFyo37Y6HPeU8QcZ+u2rS9geapIWfeuowIDAQABoyUw
IzAMBgNVHRMBAf8EAjAAMBMGA1UdJQQMMAoGCCsGAQUFBwMBMAsGCSqGSIb3DQEB
BQOBgQAmkN6XxvDnoMkGcWLCTwzxGfNNSVcYr7TtL2aJh285Xw9zaxcm/SAZBFyG
LYOChvh6hPU7joMdDwGfbiLrBnMag+BtGlmPLWwp/Kt1wNmrRhduyTQFhN3PP6fz
nBr9vVny2FewB2gHmelaPS//tXdxivSXKz3NFqqXLDJjq7P8wA==
-----END CERTIFICATE-----
EOF
	close(CERTFILE);

	#KDC certificate
	# hxtool request-create \
	# --subject="CN=krbtgt,CN=users,DC=samba,DC=example,DC=com" \
	# --key="FILE:$KEYFILE" $KDCREQ

	# hxtool issue-certificate --ca-certificate=FILE:$CAFILE,$KEYFILE \
	# --type="pkinit-kdc" \
	# --pk-init-principal="krbtgt/SAMBA.EXAMPLE.COM@SAMBA.EXAMPLE.COM" \
	# --req="PKCS10:$KDCREQ" --certificate="FILE:$KDCCERTFILE" \
	# --lifetime="25 years"

	open(KDCCERTFILE, ">$kdccertfile");
	print KDCCERTFILE <<EOF;
-----BEGIN CERTIFICATE-----
MIIDDDCCAnWgAwIBAgIUI2Tzj+JnMzMcdeabcNo30rovzFAwCwYJKoZIhvcNAQEFMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDgwMzAxMTMxOTIzWhgPMjAzMzAyMjQx
MzE5MjNaMGYxEzARBgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxl
MRUwEwYKCZImiZPyLGQBGQwFc2FtYmExDjAMBgNVBAMMBXVzZXJzMQ8wDQYDVQQDDAZrcmJ0
Z3QwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMqDqkDAIdQwDUN8cOZaFl934XQL70nF
yq+nD2KL0SfcTW5+WlyiXf5L3/oj+5pOYkdmt74MXd1PNv9Q5mjRl6bw34jPOSCgaQVp+Ne5
PcEvlQ9jb8fof6k+pUhEs6atYiZqLfn1jKgqEXKjftjoc95TxBxn67atL2B5qkhZ966jAgMB
AAGjgcgwgcUwDgYDVR0PAQH/BAQDAgWgMBIGA1UdJQQLMAkGBysGAQUCAwUwVAYDVR0RBE0w
S6BJBgYrBgEFAgKgPzA9oBMbEVNBTUJBLkVYQU1QTEUuQ09NoSYwJKADAgEBoR0wGxsGa3Ji
dGd0GxFTQU1CQS5FWEFNUExFLkNPTTAfBgNVHSMEGDAWgBTC2bn3oAyi2bNFTshV2CtS+jPK
MjAdBgNVHQ4EFgQUwtm596AMotmzRU7IVdgrUvozyjIwCQYDVR0TBAIwADANBgkqhkiG9w0B
AQUFAAOBgQBmrVD5MCmZjfHp1nEnHqTIh8r7lSmVtDx4s9MMjxm9oNrzbKXynvdhwQYFVarc
ge4yRRDXtSebErOl71zVJI9CVeQQpwcH+tA85oGA7oeFtO/S7ls581RUU6tGgyxV4veD+lJv
KPH5LevUtgD+q9H4LU4Sq5N3iFwBaeryB0g2wg==
-----END CERTIFICATE-----
EOF

	# hxtool request-create \
	# --subject="CN=Administrator,CN=users,DC=samba,DC=example,DC=com" \
	# --key="FILE:$ADMINKEYFILE" $ADMINREQFILE

	# hxtool issue-certificate --ca-certificate=FILE:$CAFILE,$KEYFILE \
	# --type="pkinit-client" \
	# --pk-init-principal="administrator@SAMBA.EXAMPLE.COM" \
	# --req="PKCS10:$ADMINREQFILE" --certificate="FILE:$ADMINCERTFILE" \
	# --lifetime="25 years"
	
	open(ADMINCERTFILE, ">$admincertfile");
	print ADMINCERTFILE <<EOF;
-----BEGIN CERTIFICATE-----
MIIDHTCCAoagAwIBAgIUUggzW4lLRkMKe1DAR2NKatkMDYwwCwYJKoZIhvcNAQELMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDkwNzI3MDMzMjE1WhgPMjAzNDA3MjIw
MzMyMTVaMG0xEzARBgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxl
MRUwEwYKCZImiZPyLGQBGQwFc2FtYmExDjAMBgNVBAMMBXVzZXJzMRYwFAYDVQQDDA1BZG1p
bmlzdHJhdG9yMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQD0+OL7TQBj0RejbIH1+g5G
eRaWaM9xF43uE5y7jUHEsi5owhZF5iIoHZeeL6cpDF5y1BZRs0JlA1VqMry1jjKlzFYVEMMF
xB6esnXhl0Jpip1JkUMMXLOP1m/0dqayuHBWozj9f/cdyCJr0wJIX1Z8Pr+EjYRGPn/MF0xd
l3JRlwIDAQABo4HSMIHPMA4GA1UdDwEB/wQEAwIFoDAoBgNVHSUEITAfBgcrBgEFAgMEBggr
BgEFBQcDAgYKKwYBBAGCNxQCAjBIBgNVHREEQTA/oD0GBisGAQUCAqAzMDGgExsRU0FNQkEu
RVhBTVBMRS5DT02hGjAYoAMCAQGhETAPGw1BZG1pbmlzdHJhdG9yMB8GA1UdIwQYMBaAFMLZ
ufegDKLZs0VOyFXYK1L6M8oyMB0GA1UdDgQWBBQg81bLyfCA88C2B/BDjXlGuaFaxjAJBgNV
HRMEAjAAMA0GCSqGSIb3DQEBCwUAA4GBAEf/OSHUDJaGdtWGNuJeqcVYVMwrfBAc0OSwVhz1
7/xqKHWo8wIMPkYRtaRHKLNDsF8GkhQPCpVsa6mX/Nt7YQnNvwd+1SBP5E8GvwWw9ZzLJvma
nk2n89emuayLpVtp00PymrDLRBcNaRjFReQU8f0o509kiVPHduAp3jOiy13l
-----END CERTIFICATE-----
EOF
	close(ADMINCERTFILE);

	# hxtool issue-certificate --ca-certificate=FILE:$CAFILE,$KEYFILE \
	# --type="pkinit-client" \
	# --ms-upn="administrator@samba.example.com" \
	# --req="PKCS10:$ADMINREQFILE" --certificate="FILE:$ADMINCERTUPNFILE" \
	# --lifetime="25 years"
	
	open(ADMINCERTUPNFILE, ">$admincertupnfile");
	print ADMINCERTUPNFILE <<EOF;
-----BEGIN CERTIFICATE-----
MIIDDzCCAnigAwIBAgIUUp3CJMuNaEaAdPKp3QdNIwG7a4wwCwYJKoZIhvcNAQELMFIxEzAR
BgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxlMRUwEwYKCZImiZPy
LGQBGQwFc2FtYmExCzAJBgNVBAMMAkNBMCIYDzIwMDkwNzI3MDMzMzA1WhgPMjAzNDA3MjIw
MzMzMDVaMG0xEzARBgoJkiaJk/IsZAEZDANjb20xFzAVBgoJkiaJk/IsZAEZDAdleGFtcGxl
MRUwEwYKCZImiZPyLGQBGQwFc2FtYmExDjAMBgNVBAMMBXVzZXJzMRYwFAYDVQQDDA1BZG1p
bmlzdHJhdG9yMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQD0+OL7TQBj0RejbIH1+g5G
eRaWaM9xF43uE5y7jUHEsi5owhZF5iIoHZeeL6cpDF5y1BZRs0JlA1VqMry1jjKlzFYVEMMF
xB6esnXhl0Jpip1JkUMMXLOP1m/0dqayuHBWozj9f/cdyCJr0wJIX1Z8Pr+EjYRGPn/MF0xd
l3JRlwIDAQABo4HEMIHBMA4GA1UdDwEB/wQEAwIFoDAoBgNVHSUEITAfBgcrBgEFAgMEBggr
BgEFBQcDAgYKKwYBBAGCNxQCAjA6BgNVHREEMzAxoC8GCisGAQQBgjcUAgOgIQwfYWRtaW5p
c3RyYXRvckBzYW1iYS5leGFtcGxlLmNvbTAfBgNVHSMEGDAWgBTC2bn3oAyi2bNFTshV2CtS
+jPKMjAdBgNVHQ4EFgQUIPNWy8nwgPPAtgfwQ415RrmhWsYwCQYDVR0TBAIwADANBgkqhkiG
9w0BAQsFAAOBgQBk42+egeUB3Ji2PC55fbt3FNKxvmm2xUUFkV9POK/YR9rajKOwk5jtYSeS
Zd7J9s//rNFNa7waklFkDaY56+QWTFtdvxfE+KoHaqt6X8u6pqi7p3M4wDKQox+9Dx8yWFyq
Wfz/8alZ5aMezCQzXJyIaJsCLeKABosSwHcpAFmxlQ==
-----END CERTIFICATE-----
EOF
}

sub mk_krb5_conf($$)
{
	my ($self, $ctx) = @_;

	unless (open(KRB5CONF, ">$ctx->{krb5_conf}")) {
		warn("can't open $ctx->{krb5_conf}$?");
		return undef;
	}
	print KRB5CONF "
#Generated krb5.conf for $ctx->{realm}

[libdefaults]
 default_realm = $ctx->{realm}
 dns_lookup_realm = false
 dns_lookup_kdc = false
 ticket_lifetime = 24h
 forwardable = yes
 allow_weak_crypto = yes

[realms]
 $ctx->{realm} = {
  kdc = $ctx->{kdc_ipv4}:88
  admin_server = $ctx->{kdc_ipv4}:88
  default_domain = $ctx->{dnsname}
 }
 $ctx->{dnsname} = {
  kdc = $ctx->{kdc_ipv4}:88
  admin_server = $ctx->{kdc_ipv4}:88
  default_domain = $ctx->{dnsname}
 }
 $ctx->{domain} = {
  kdc = $ctx->{kdc_ipv4}:88
  admin_server = $ctx->{kdc_ipv4}:88
  default_domain = $ctx->{dnsname}
 }

[appdefaults]
	pkinit_anchors = FILE:$ctx->{tlsdir}/ca.pem

[kdc]
	enable-pkinit = true
	pkinit_identity = FILE:$ctx->{tlsdir}/kdc.pem,$ctx->{tlsdir}/key.pem
	pkinit_anchors = FILE:$ctx->{tlsdir}/ca.pem

[domain_realm]
 .$ctx->{dnsname} = $ctx->{realm}
";
	close(KRB5CONF);
}

sub provision_raw_prepare($$$$$$$$$$)
{
	my ($self, $prefix, $server_role, $netbiosname, $netbiosalias,
	    $domain, $realm, $functional_level,
	    $swiface, $password, $kdc_ipv4) = @_;
	my $ctx;

	unless(-d $prefix or mkdir($prefix, 0777)) {
		warn("Unable to create $prefix");
		return undef;
	}
	my $prefix_abs = abs_path($prefix);

	die ("prefix=''") if $prefix_abs eq "";
	die ("prefix='/'") if $prefix_abs eq "/";

	unless (system("rm -rf $prefix_abs/*") == 0) {
		warn("Unable to clean up");
	}

	$ctx->{prefix} = $prefix;
	$ctx->{prefix_abs} = $prefix_abs;
	
	$ctx->{dns_host_file} = "$ENV{SELFTEST_PREFIX}/dns_host_file";

	$ctx->{server_role} = $server_role;
	$ctx->{netbiosname} = $netbiosname;
	$ctx->{netbiosalias} = $netbiosalias;
	$ctx->{swiface} = $swiface;
	$ctx->{password} = $password;
	$ctx->{kdc_ipv4} = $kdc_ipv4;

	$ctx->{server_loglevel} =$ENV{SERVER_LOG_LEVEL} || 1;
	$ctx->{username} = "Administrator";
	$ctx->{domain} = $domain;
	$ctx->{realm} = uc($realm);
	$ctx->{dnsname} = lc($realm);
	$ctx->{sid_generator} = "internal";

	$ctx->{functional_level} = $functional_level;

	my $unix_name = ($ENV{USER} or $ENV{LOGNAME} or `whoami`);
	chomp $unix_name;
	$ctx->{unix_name} = $unix_name;
	$ctx->{unix_uid} = $>;
	$ctx->{unix_gids_str} = $);
	@{$ctx->{unix_gids}} = split(" ", $ctx->{unix_gids_str});

	$ctx->{etcdir} = "$prefix_abs/etc";
	$ctx->{piddir} = "$prefix_abs/pid";
	$ctx->{smb_conf} = "$ctx->{etcdir}/smb.conf";
	$ctx->{krb5_conf} = "$ctx->{etcdir}/krb5.conf";
	$ctx->{privatedir} = "$prefix_abs/private";
	$ctx->{ncalrpcdir} = "$prefix_abs/ncalrpc";
	$ctx->{lockdir} = "$prefix_abs/lockdir";
	$ctx->{winbindd_socket_dir} = "$prefix_abs/winbindd_socket";
	$ctx->{winbindd_privileged_socket_dir} = "$prefix_abs/winbindd_privileged_socket";
	$ctx->{ntp_signd_socket_dir} = "$prefix_abs/ntp_signd_socket";
	$ctx->{nsswrap_passwd} = "$ctx->{etcdir}/passwd";
	$ctx->{nsswrap_group} = "$ctx->{etcdir}/group";

	$ctx->{tlsdir} = "$ctx->{privatedir}/tls";

	$ctx->{ipv4} = "127.0.0.$swiface";
	$ctx->{interfaces} = "$ctx->{ipv4}/8";

	push(@{$ctx->{directories}}, $ctx->{privatedir});
	push(@{$ctx->{directories}}, $ctx->{etcdir});
	push(@{$ctx->{directories}}, $ctx->{piddir});
	push(@{$ctx->{directories}}, $ctx->{ncalrpcdir});
	push(@{$ctx->{directories}}, $ctx->{lockdir});

	$ctx->{smb_conf_extra_options} = "";

	my @provision_options = ();
	push (@provision_options, "NSS_WRAPPER_PASSWD=\"$ctx->{nsswrap_passwd}\"");
	push (@provision_options, "NSS_WRAPPER_GROUP=\"$ctx->{nsswrap_group}\"");
	if (defined($ENV{GDB_PROVISION})) {
		push (@provision_options, "gdb --args");
		if (!defined($ENV{PYTHON})) {
		    push (@provision_options, "env");
		    push (@provision_options, "python");
		}
	}
	if (defined($ENV{VALGRIND_PROVISION})) {
		push (@provision_options, "valgrind");
		if (!defined($ENV{PYTHON})) {
		    push (@provision_options, "env");
		    push (@provision_options, "python");
		}
	}
	if (defined($ENV{PYTHON})) {
		push (@provision_options, $ENV{PYTHON});
	}
	push (@provision_options, "$self->{srcdir}/source4/setup/provision");
	push (@provision_options, "--configfile=$ctx->{smb_conf}");
	push (@provision_options, "--host-name=$ctx->{netbiosname}");
	push (@provision_options, "--host-ip=$ctx->{ipv4}");
	push (@provision_options, "--quiet");
	push (@provision_options, "--domain=$ctx->{domain}");
	push (@provision_options, "--realm=$ctx->{realm}");
	push (@provision_options, "--adminpass=$ctx->{password}");
	push (@provision_options, "--krbtgtpass=krbtgt$ctx->{password}");
	push (@provision_options, "--machinepass=machine$ctx->{password}");
	push (@provision_options, "--root=$ctx->{unix_name}");
	push (@provision_options, "--server-role=\"$ctx->{server_role}\"");
	push (@provision_options, "--function-level=\"$ctx->{functional_level}\"");

	@{$ctx->{provision_options}} = @provision_options;

	return $ctx;
}

#
# Step1 creates the basic configuration
#
sub provision_raw_step1($$)
{
	my ($self, $ctx) = @_;

	mkdir($_, 0777) foreach (@{$ctx->{directories}});

	unless (open(CONFFILE, ">$ctx->{smb_conf}")) {
		warn("can't open $ctx->{smb_conf}$?");
		return undef;
	}
	print CONFFILE "
[global]
	netbios name = $ctx->{netbiosname}
	netbios aliases = $ctx->{netbiosalias}
	posix:eadb = $ctx->{lockdir}/eadb.tdb
	workgroup = $ctx->{domain}
	realm = $ctx->{realm}
	private dir = $ctx->{privatedir}
	pid directory = $ctx->{piddir}
	ncalrpc dir = $ctx->{ncalrpcdir}
	lock dir = $ctx->{lockdir}
	winbindd socket directory = $ctx->{winbindd_socket_dir}
	winbindd privileged socket directory = $ctx->{winbindd_privileged_socket_dir}
	ntp signd socket directory = $ctx->{ntp_signd_socket_dir}
	winbind separator = /
	name resolve order = file bcast
	interfaces = $ctx->{interfaces}
	tls dh params file = $ctx->{tlsdir}/dhparms.pem
	panic action = $RealBin/gdb_backtrace \%PID% \%PROG%
	wins support = yes
	server role = $ctx->{server_role}
	server services = +echo
	notify:inotify = false
	ldb:nosync = true
#We don't want to pass our self-tests if the PAC code is wrong
	gensec:require_pac = true
	log level = $ctx->{server_loglevel}
	lanman auth = Yes
	rndc command = true
        dns update command = $ENV{SRCDIR_ABS}/source4/scripting/bin/samba_dnsupdate --all-interfaces --use-file=$ctx->{dns_host_file}
        spn update command = $ENV{SRCDIR_ABS}/source4/scripting/bin/samba_spnupdate
        resolv:host file = $ctx->{dns_host_file}
	dreplsrv:periodic_startup_interval = 0
";

	if (defined($ctx->{sid_generator}) && $ctx->{sid_generator} ne "internal") {
		print CONFFILE "
	sid generator = $ctx->{sid_generator}";
	}

	print CONFFILE "

	# Begin extra options
	$ctx->{smb_conf_extra_options}
	# End extra options
";
	close(CONFFILE);

	$self->mk_keyblobs($ctx->{tlsdir});

	$self->mk_krb5_conf($ctx);

	open(PWD, ">$ctx->{nsswrap_passwd}");
	print PWD "
root:x:0:0:root gecos:$ctx->{prefix_abs}:/bin/false
$ctx->{unix_name}:x:$ctx->{unix_uid}:@{$ctx->{unix_gids}}[0]:$ctx->{unix_name} gecos:$ctx->{prefix_abs}:/bin/false
nobody:x:65534:65533:nobody gecos:$ctx->{prefix_abs}:/bin/false
";
	close(PWD);

	open(GRP, ">$ctx->{nsswrap_group}");
	print GRP "
root:x:0:
wheel:x:10:
users:x:100:
nobody:x:65533:
nogroup:x:65534:nobody
";
	close(GRP);

	my $configuration = "--configfile=$ctx->{smb_conf}";

#Ensure the config file is valid before we start
	my $testparm = $self->scriptdir_path("bin/testparm");
	if (system("$testparm $configuration -v --suppress-prompt >/dev/null 2>&1") != 0) {
		system("$testparm -v --suppress-prompt $configuration >&2");
		warn("Failed to create a valid smb.conf configuration $testparm!");
		return undef;
	}
	unless (system("($testparm $configuration -v --suppress-prompt --parameter-name=\"netbios name\" --section-name=global 2> /dev/null | grep -i \"^$ctx->{netbiosname}\" ) >/dev/null 2>&1") == 0) {
		warn("Failed to create a valid smb.conf configuration! $testparm $configuration -v --suppress-prompt --parameter-name=\"netbios name\" --section-name=global");
		return undef;
	}

	my $ret = {
		KRB5_CONFIG => $ctx->{krb5_conf},
		PIDDIR => $ctx->{piddir},
		SERVER => $ctx->{netbiosname},
		SERVER_IP => $ctx->{ipv4},
		NETBIOSNAME => $ctx->{netbiosname},
		NETBIOSALIAS => $ctx->{netbiosalias},
		DOMAIN => $ctx->{domain},
		USERNAME => $ctx->{username},
		REALM => $ctx->{realm},
		PASSWORD => $ctx->{password},
		LDAPDIR => $ctx->{ldapdir},
		LDAP_INSTANCE => $ctx->{ldap_instance},
		WINBINDD_SOCKET_DIR => $ctx->{winbindd_socket_dir},
		NCALRPCDIR => $ctx->{ncalrpcdir},
		LOCKDIR => $ctx->{lockdir},
		SERVERCONFFILE => $ctx->{smb_conf},
		CONFIGURATION => $configuration,
		SOCKET_WRAPPER_DEFAULT_IFACE => $ctx->{swiface},
		NSS_WRAPPER_PASSWD => $ctx->{nsswrap_passwd},
		NSS_WRAPPER_GROUP => $ctx->{nsswrap_group},
		SAMBA_TEST_FIFO => "$ctx->{prefix}/samba_test.fifo",
		SAMBA_TEST_LOG => "$ctx->{prefix}/samba_test.log",
		SAMBA_TEST_LOG_POS => 0,
	};

	return $ret;
}

#
# Step2 runs the provision script
#
sub provision_raw_step2($$$)
{
	my ($self, $ctx, $ret) = @_;

	my $provision_cmd = join(" ", @{$ctx->{provision_options}});
	unless (system($provision_cmd) == 0) {
		warn("Unable to provision: \n$provision_cmd\n");
		return undef;
	}

	return $ret;
}

sub provision($$$$$$$$$)
{
	my ($self, $prefix, $server_role, $netbiosname, $netbiosalias,
	    $domain, $realm, $functional_level,
	    $swiface, $password, $kdc_ipv4, $extra_smbconf_options) = @_;

	my $ctx = $self->provision_raw_prepare($prefix, $server_role,
					       $netbiosname, $netbiosalias,
					       $domain, $realm, $functional_level,
					       $swiface, $password, $kdc_ipv4);

	$ctx->{tmpdir} = "$ctx->{prefix_abs}/tmp";
	push(@{$ctx->{directories}}, "$ctx->{tmpdir}");
	push(@{$ctx->{directories}}, "$ctx->{tmpdir}/test1");
	push(@{$ctx->{directories}}, "$ctx->{tmpdir}/test2");
	my $msdfs = "no";
	$msdfs = "yes" if ($server_role eq "domain controller");
	$ctx->{smb_conf_extra_options} = "

	max xmit = 32K
	server max protocol = SMB2
        $extra_smbconf_options
	host msdfs = $msdfs
	lanman auth = yes

[tmp]
	path = $ctx->{tmpdir}
	read only = no
	posix:sharedelay = 10000
	posix:oplocktimeout = 3
	posix:writetimeupdatedelay = 500000

[test1]
	path = $ctx->{tmpdir}/test1
	read only = no
	posix:sharedelay = 10000
	posix:oplocktimeout = 3
	posix:writetimeupdatedelay = 50000

[test2]
	path = $ctx->{tmpdir}/test2
	read only = no
	posix:sharedelay = 10000
	posix:oplocktimeout = 3
	posix:writetimeupdatedelay = 50000

[cifs]
	read only = no
	ntvfs handler = cifs
	cifs:server = $ctx->{netbiosname}
	cifs:share = tmp
#There is no username specified here, instead the client is expected
#to log in with kerberos, and the serverwill use delegated credentials.

[simple]
	path = $ctx->{tmpdir}
	read only = no
	ntvfs handler = simple

[sysvol]
	path = $ctx->{lockdir}/sysvol
	read only = yes

[netlogon]
	path = $ctx->{lockdir}/sysvol/$ctx->{dnsname}/scripts
	read only = no

[cifsposix]
	copy = simple
	ntvfs handler = cifsposix
";

	if (defined($self->{ldap})) {
		$ctx->{ldapdir} = "$ctx->{privatedir}/ldap";
		push(@{$ctx->{directories}}, "$ctx->{ldapdir}");

		my $ldap_uri= "$ctx->{ldapdir}/ldapi";
		$ldap_uri =~ s|/|%2F|g;
		$ldap_uri = "ldapi://$ldap_uri";
		$ctx->{ldap_uri} = $ldap_uri;
		if ($self->{ldap} eq "fedora-ds") {
			$ctx->{sid_generator} = "backend";
		}

		$ctx->{ldap_instance} = lc($ctx->{netbiosname});
	}

	my $ret = $self->provision_raw_step1($ctx);
	unless (defined $ret) {
		return undef;
	}

	if (defined($self->{ldap})) {
		$ret->{LDAP_URI} = $ctx->{ldap_uri};
		push (@{$ctx->{provision_options}}, "--ldap-backend-type=" . $self->{ldap});
		push (@{$ctx->{provision_options}}, "--ldap-backend-nosync");
		if ($self->{ldap} eq "openldap") {
			push (@{$ctx->{provision_options}}, "--slapd-path=" . $ENV{OPENLDAP_SLAPD});
			($ret->{SLAPD_CONF_D}, $ret->{OPENLDAP_PIDFILE}) = $self->mk_openldap($ctx) or die("Unable to create openldap directories");

                } elsif ($self->{ldap} eq "fedora-ds") {
 		        push (@{$ctx->{provision_options}}, "--slapd-path=" . "$ENV{FEDORA_DS_ROOT}/sbin/ns-slapd");
 		        push (@{$ctx->{provision_options}}, "--setup-ds-path=" . "$ENV{FEDORA_DS_ROOT}/sbin/setup-ds.pl");
			($ret->{FEDORA_DS_DIR}, $ret->{FEDORA_DS_PIDFILE}) = $self->mk_fedora_ds($ctx) or die("Unable to create fedora ds directories");
		}

	}

	return $self->provision_raw_step2($ctx, $ret);
}

sub provision_member($$$)
{
	my ($self, $prefix, $dcvars) = @_;
	print "PROVISIONING MEMBER...";

	my $ret = $self->provision($prefix,
				   "member server",
				   "localmember",
				   "member3",
				   "SAMBADOMAIN",
				   "samba.example.com",
				   "2008",
				   3,
				   "locMEMpass3",
				   $dcvars->{SERVER_IP},
				   "");
	unless ($ret) {
		return undef;
	}

	my $samba_tool = $self->bindir_path("samba-tool");
	my $cmd = "";
	$cmd .= "SOCKET_WRAPPER_DEFAULT_IFACE=\"$ret->{SOCKET_WRAPPER_DEFAULT_IFACE}\" ";
	$cmd .= "KRB5_CONFIG=\"$ret->{KRB5_CONFIG}\" ";
	$cmd .= "$samba_tool join $ret->{CONFIGURATION} $dcvars->{REALM} member";
	$cmd .= " -U$dcvars->{DC_USERNAME}\%$dcvars->{DC_PASSWORD}";

	unless (system($cmd) == 0) {
		warn("Join failed\n$cmd");
		return undef;
	}

	$ret->{MEMBER_SERVER} = $ret->{SERVER};
	$ret->{MEMBER_SERVER_IP} = $ret->{SERVER_IP};
	$ret->{MEMBER_NETBIOSNAME} = $ret->{NETBIOSNAME};
	$ret->{MEMBER_NETBIOSALIAS} = $ret->{NETBIOSALIAS};
	$ret->{MEMBER_USERNAME} = $ret->{USERNAME};
	$ret->{MEMBER_PASSWORD} = $ret->{PASSWORD};

	$ret->{DC_SERVER} = $dcvars->{DC_SERVER};
	$ret->{DC_SERVER_IP} = $dcvars->{DC_SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $dcvars->{DC_NETBIOSNAME};
	$ret->{DC_NETBIOSALIAS} = $dcvars->{DC_NETBIOSALIAS};
	$ret->{DC_USERNAME} = $dcvars->{DC_USERNAME};
	$ret->{DC_PASSWORD} = $dcvars->{DC_PASSWORD};

	return $ret;
}

sub provision_rpc_proxy($$$)
{
	my ($self, $prefix, $dcvars) = @_;
	print "PROVISIONING RPC PROXY...";

	my $extra_smbconf_options = "dcerpc_remote:binding = ncacn_ip_tcp:localdc
       dcerpc endpoint servers = epmapper, remote
       dcerpc_remote:interfaces = rpcecho
";

	my $ret = $self->provision($prefix,
				   "member server",
				   "localrpcproxy",
				   "rpcproxy4",
				   "SAMBADOMAIN",
				   "samba.example.com",
				   "2008",
				   4,
				   "locRPCproxypass4",
				   $dcvars->{SERVER_IP},
				   $extra_smbconf_options);

	unless ($ret) {
		return undef;
	}

	my $samba_tool = $self->bindir_path("samba-tool");
	my $cmd = "";
	$cmd .= "SOCKET_WRAPPER_DEFAULT_IFACE=\"$ret->{SOCKET_WRAPPER_DEFAULT_IFACE}\" ";
	$cmd .= "KRB5_CONFIG=\"$ret->{KRB5_CONFIG}\" ";
	$cmd .= "$samba_tool join $ret->{CONFIGURATION} $dcvars->{REALM} member";
	$cmd .= " -U$dcvars->{DC_USERNAME}\%$dcvars->{DC_PASSWORD}";

	unless (system($cmd) == 0) {
		warn("Join failed\n$cmd");
		return undef;
	}

	$ret->{RPC_PROXY_SERVER} = $ret->{SERVER};
	$ret->{RPC_PROXY_SERVER_IP} = $ret->{SERVER_IP};
	$ret->{RPC_PROXY_NETBIOSNAME} = $ret->{NETBIOSNAME};
	$ret->{RPC_PROXY_NETBIOSALIAS} = $ret->{NETBIOSALIAS};
	$ret->{RPC_PROXY_USERNAME} = $ret->{USERNAME};
	$ret->{RPC_PROXY_PASSWORD} = $ret->{PASSWORD};

	return $ret;
}

sub provision_vampire_dc($$$)
{
	my ($self, $prefix, $dcvars) = @_;
	print "PROVISIONING VAMPIRE DC...";

	# We do this so that we don't run the provision.  That's the job of 'net vampire'.
	my $ctx = $self->provision_raw_prepare($prefix, "domain controller",
					       "localvampiredc",
					       "dc2",
					       "SAMBADOMAIN",
					       "samba.example.com",
					       "2008",
					       2, $dcvars->{PASSWORD},
					       $dcvars->{SERVER_IP});

	$ctx->{smb_conf_extra_options} = "
	max xmit = 32K
	server max protocol = SMB2

[sysvol]
	path = $ctx->{lockdir}/sysvol
	read only = yes

[netlogon]
	path = $ctx->{lockdir}/sysvol/$ctx->{dnsname}/scripts
	read only = no

";

	my $ret = $self->provision_raw_step1($ctx);
	unless ($ret) {
		return undef;
	}

	my $samba_tool = $self->bindir_path("samba-tool");
	my $cmd = "";
	$cmd .= "SOCKET_WRAPPER_DEFAULT_IFACE=\"$ret->{SOCKET_WRAPPER_DEFAULT_IFACE}\" ";
	$cmd .= "KRB5_CONFIG=\"$ret->{KRB5_CONFIG}\" ";
	$cmd .= "$samba_tool join $ret->{CONFIGURATION} $dcvars->{REALM} DC --realm=$dcvars->{REALM}";
	$cmd .= " -U$dcvars->{DC_USERNAME}\%$dcvars->{DC_PASSWORD}";

	unless (system($cmd) == 0) {
		warn("Join failed\n$cmd");
		return undef;
	}

	$ret->{VAMPIRE_DC_SERVER} = $ret->{SERVER};
	$ret->{VAMPIRE_DC_SERVER_IP} = $ret->{SERVER_IP};
	$ret->{VAMPIRE_DC_NETBIOSNAME} = $ret->{NETBIOSNAME};
	$ret->{VAMPIRE_DC_NETBIOSALIAS} = $ret->{NETBIOSALIAS};

	$ret->{DC_SERVER} = $dcvars->{DC_SERVER};
	$ret->{DC_SERVER_IP} = $dcvars->{DC_SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $dcvars->{DC_NETBIOSNAME};
	$ret->{DC_NETBIOSALIAS} = $dcvars->{DC_NETBIOSALIAS};
	$ret->{DC_USERNAME} = $dcvars->{DC_USERNAME};
	$ret->{DC_PASSWORD} = $dcvars->{DC_PASSWORD};

	return $ret;
}

sub provision_dc($$)
{
	my ($self, $prefix) = @_;

	print "PROVISIONING DC...";
	my $ret = $self->provision($prefix,
				   "domain controller",
				   "localdc",
				   "dc1",
				   "SAMBADOMAIN",
				   "samba.example.com",
				   "2008",
				   1,
				   "locDCpass1",
				   "127.0.0.1", "");

	return undef unless(defined $ret);
	unless($self->add_wins_config("$prefix/private")) {
		warn("Unable to add wins configuration");
		return undef;
	}

	$ret->{DC_SERVER} = $ret->{SERVER};
	$ret->{DC_SERVER_IP} = $ret->{SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $ret->{NETBIOSNAME};
	$ret->{DC_NETBIOSALIAS} = $ret->{NETBIOSALIAS};
	$ret->{DC_USERNAME} = $ret->{USERNAME};
	$ret->{DC_PASSWORD} = $ret->{PASSWORD};

	return $ret;
}

sub provision_fl2000dc($$)
{
	my ($self, $prefix) = @_;

	print "PROVISIONING DC...";
	my $ret = $self->provision($prefix,
				   "domain controller",
				   "dc5",
				   "localfl2000dc",
				   "SAMBA2000",
				   "samba2000.example.com",
				   "2000",
				   5,
				   "locDCpass5",
				   "127.0.0.5", "");

	unless($self->add_wins_config("$prefix/private")) {
		warn("Unable to add wins configuration");
		return undef;
	}

	return $ret;
}

sub provision_fl2003dc($$)
{
	my ($self, $prefix) = @_;

	print "PROVISIONING DC...";
	my $ret = $self->provision($prefix,
				   "domain controller",
				   "dc6",
				   "localfl2003dc",
				   "SAMBA2003",
				   "samba2003.example.com",
				   "2003",
				   6,
				   "locDCpass6",
				   "127.0.0.6", "");

	unless($self->add_wins_config("$prefix/private")) {
		warn("Unable to add wins configuration");
		return undef;
	}

	return $ret;
}

sub provision_fl2008r2dc($$)
{
	my ($self, $prefix) = @_;

	print "PROVISIONING DC...";
	my $ret = $self->provision($prefix,
				   "domain controller",
				   "dc7",
				   "localfl2000r2dc",
				   "SAMBA2008R2",
				   "samba2008R2.example.com",
				   "2008_R2",
				   7,
				   "locDCpass7",
				   "127.0.0.7", "");

	unless ($self->add_wins_config("$prefix/private")) {
		warn("Unable to add wins configuration");
		return undef;
	}

	return $ret;
}


sub provision_rodc($$$)
{
	my ($self, $prefix, $dcvars) = @_;
	print "PROVISIONING RODC...";

	# We do this so that we don't run the provision.  That's the job of 'net join RODC'.
	my $ctx = $self->provision_raw_prepare($prefix, "domain controller",
					       "rodc",
					       "dc8",
					       "SAMBADOMAIN",
					       "samba.example.com",
					       "2008",
					       8, $dcvars->{PASSWORD},
					       $dcvars->{SERVER_IP});
	unless ($ctx) {
		return undef;
	}

	$ctx->{tmpdir} = "$ctx->{prefix_abs}/tmp";
	push(@{$ctx->{directories}}, "$ctx->{tmpdir}");

	$ctx->{smb_conf_extra_options} = "
	max xmit = 32K
	server max protocol = SMB2

[sysvol]
	path = $ctx->{lockdir}/sysvol
	read only = yes

[netlogon]
	path = $ctx->{lockdir}/sysvol/$ctx->{dnsname}/scripts
	read only = yes

[tmp]
	path = $ctx->{tmpdir}
	read only = no
	posix:sharedelay = 10000
	posix:oplocktimeout = 3
	posix:writetimeupdatedelay = 500000

";

	my $ret = $self->provision_raw_step1($ctx);
	unless ($ret) {
		return undef;
	}

	my $samba_tool = $self->bindir_path("samba-tool");
	my $cmd = "";
	$cmd .= "SOCKET_WRAPPER_DEFAULT_IFACE=\"$ret->{SOCKET_WRAPPER_DEFAULT_IFACE}\" ";
	$cmd .= "KRB5_CONFIG=\"$ret->{KRB5_CONFIG}\" ";
	$cmd .= "$samba_tool join $ret->{CONFIGURATION} $dcvars->{REALM} RODC";
	$cmd .= " -U$dcvars->{DC_USERNAME}\%$dcvars->{DC_PASSWORD}";
	$cmd .= " --server=$dcvars->{DC_SERVER}";

	unless (system($cmd) == 0) {
		warn("RODC join failed\n$cmd");
		return undef;
	}

	# we overwrite the kdc after the RODC join
	# so that use the RODC as kdc and test
	# the proxy code
	$ctx->{kdc_ipv4} = $ret->{SERVER_IP};
	$self->mk_krb5_conf($ctx);

	$ret->{RODC_DC_SERVER} = $ret->{SERVER};
	$ret->{RODC_DC_SERVER_IP} = $ret->{SERVER_IP};
	$ret->{RODC_DC_NETBIOSNAME} = $ret->{NETBIOSNAME};
	$ret->{RODC_DC_NETBIOSALIAS} = $ret->{NETBIOSALIAS};

	$ret->{DC_SERVER} = $dcvars->{DC_SERVER};
	$ret->{DC_SERVER_IP} = $dcvars->{DC_SERVER_IP};
	$ret->{DC_NETBIOSNAME} = $dcvars->{DC_NETBIOSNAME};
	$ret->{DC_NETBIOSALIAS} = $dcvars->{DC_NETBIOSALIAS};
	$ret->{DC_USERNAME} = $dcvars->{DC_USERNAME};
	$ret->{DC_PASSWORD} = $dcvars->{DC_PASSWORD};

	return $ret;
}

sub teardown_env($$)
{
	my ($self, $envvars) = @_;
	my $pid;

	close(DATA);

	if (open(IN, "<$envvars->{PIDDIR}/samba.pid")) {
		$pid = <IN>;
		close(IN);

		# Give the process 20 seconds to exit.  gcov needs
		# this time to write out the covarge data
		my $count = 0;
		until (kill(0, $pid) == 0) {
			# if no process sucessfully signalled, then we are done
			sleep(1);
			$count++;
			last if $count > 20;
		}

		# If it is still around, kill it
		if ($count > 20) {
			print "server process $pid took more than $count seconds to exit, killing\n";
			kill 9, $pid;
		}
	}

	my $failed = $? >> 8;

	$self->slapd_stop($envvars) if ($self->{ldap});

	print $self->getlog_env($envvars);

	return $failed;
}

sub getlog_env($$)
{
	my ($self, $envvars) = @_;
	my $title = "SAMBA LOG of: $envvars->{NETBIOSNAME}\n";
	my $out = $title;

	open(LOG, "<$envvars->{SAMBA_TEST_LOG}");

	seek(LOG, $envvars->{SAMBA_TEST_LOG_POS}, SEEK_SET);
	while (<LOG>) {
		$out .= $_;
	}
	$envvars->{SAMBA_TEST_LOG_POS} = tell(LOG);
	close(LOG);

	return "" if $out eq $title;

	return $out;
}

sub check_env($$)
{
	my ($self, $envvars) = @_;

	return (-p $envvars->{SAMBA_TEST_FIFO});
}

sub setup_env($$$)
{
	my ($self, $envname, $path) = @_;

	$ENV{ENVNAME} = $envname;

	if ($envname eq "dc") {
		return $self->setup_dc("$path/dc");
	} elsif ($envname eq "fl2000dc") {
		return $self->setup_fl2000dc("$path/fl2000dc");
	} elsif ($envname eq "fl2003dc") {
		return $self->setup_fl2003dc("$path/fl2003dc");
	} elsif ($envname eq "fl2008r2dc") {
		return $self->setup_fl2008r2dc("$path/fl2008r2dc");
	} elsif ($envname eq "rpc_proxy") {
		if (not defined($self->{vars}->{dc})) {
			$self->setup_dc("$path/dc");
		}
		return $self->setup_rpc_proxy("$path/rpc_proxy", $self->{vars}->{dc});
	} elsif ($envname eq "vampire_dc") {
		if (not defined($self->{vars}->{dc})) {
			$self->setup_dc("$path/dc");
		}
		return $self->setup_vampire_dc("$path/vampire_dc", $self->{vars}->{dc});
	} elsif ($envname eq "member") {
		if (not defined($self->{vars}->{dc})) {
			$self->setup_dc("$path/dc");
		}
		return $self->setup_member("$path/member", $self->{vars}->{dc});
	} elsif ($envname eq "rodc") {
		if (not defined($self->{vars}->{dc})) {
			$self->setup_dc("$path/dc");
		}
		return $self->setup_rodc("$path/rodc", $self->{vars}->{dc});
	} elsif ($envname eq "all") {
		if (not defined($self->{vars}->{dc})) {
			$ENV{ENVNAME} = "dc";
			$self->setup_dc("$path/dc");
		}
		my $ret = $self->setup_member("$path/member", $self->{vars}->{dc});
		if (not defined($self->{vars}->{rpc_proxy})) {
			$ENV{ENVNAME} = "rpc_proxy";
			my $rpc_proxy_ret = $self->setup_rpc_proxy("$path/rpc_proxy", $self->{vars}->{dc});
			
			$ret->{RPC_PROXY_SERVER} = $rpc_proxy_ret->{SERVER};
			$ret->{RPC_PROXY_SERVER_IP} = $rpc_proxy_ret->{SERVER_IP};
			$ret->{RPC_PROXY_NETBIOSNAME} = $rpc_proxy_ret->{NETBIOSNAME};
			$ret->{RPC_PROXY_NETBIOSALIAS} = $rpc_proxy_ret->{NETBIOSALIAS};
			$ret->{RPC_PROXY_USERNAME} = $rpc_proxy_ret->{USERNAME};
			$ret->{RPC_PROXY_PASSWORD} = $rpc_proxy_ret->{PASSWORD};
		}
		if (not defined($self->{vars}->{fl2000dc})) {
			$ENV{ENVNAME} = "fl2000dc";
			my $fl2000dc_ret = $self->setup_fl2000dc("$path/fl2000dc", $self->{vars}->{dc});
			
			$ret->{FL2000DC_SERVER} = $fl2000dc_ret->{SERVER};
			$ret->{FL2000DC_SERVER_IP} = $fl2000dc_ret->{SERVER_IP};
			$ret->{FL2000DC_NETBIOSNAME} = $fl2000dc_ret->{NETBIOSNAME};
			$ret->{FL2000DC_NETBIOSALIAS} = $fl2000dc_ret->{NETBIOSALIAS};
			$ret->{FL2000DC_USERNAME} = $fl2000dc_ret->{USERNAME};
			$ret->{FL2000DC_PASSWORD} = $fl2000dc_ret->{PASSWORD};
		}
		if (not defined($self->{vars}->{fl2003dc})) {
			$ENV{ENVNAME} = "fl2003dc";
			my $fl2003dc_ret = $self->setup_fl2003dc("$path/fl2003dc", $self->{vars}->{dc});

			$ret->{FL2003DC_SERVER} = $fl2003dc_ret->{SERVER};
			$ret->{FL2003DC_SERVER_IP} = $fl2003dc_ret->{SERVER_IP};
			$ret->{FL2003DC_NETBIOSNAME} = $fl2003dc_ret->{NETBIOSNAME};
			$ret->{FL2003DC_NETBIOSALIAS} = $fl2003dc_ret->{NETBIOSALIAS};
			$ret->{FL2003DC_USERNAME} = $fl2003dc_ret->{USERNAME};
			$ret->{FL2003DC_PASSWORD} = $fl2003dc_ret->{PASSWORD};
		}
		if (not defined($self->{vars}->{fl2008r2dc})) {
			$ENV{ENVNAME} = "fl2008r2dc";
			my $fl2008r2dc_ret = $self->setup_fl2008r2dc("$path/fl2008r2dc", $self->{vars}->{dc});

			$ret->{FL2008R2DC_SERVER} = $fl2008r2dc_ret->{SERVER};
			$ret->{FL2008R2DC_SERVER_IP} = $fl2008r2dc_ret->{SERVER_IP};
			$ret->{FL2008R2DC_NETBIOSNAME} = $fl2008r2dc_ret->{NETBIOSNAME};
			$ret->{FL2008R2DC_NETBIOSALIAS} = $fl2008r2dc_ret->{NETBIOSALIAS};
			$ret->{FL2008R2DC_USERNAME} = $fl2008r2dc_ret->{USERNAME};
			$ret->{FL2008R2DC_PASSWORD} = $fl2008r2dc_ret->{PASSWORD};
		}
		return $ret;
	} else {
		warn("Samba4 can't provide environment '$envname'");
		return undef;
	}
}

sub setup_member($$$)
{
	my ($self, $path, $dc_vars) = @_;

	my $env = $self->provision_member($path, $dc_vars);

	if (defined $env) {
		$self->check_or_start($env, ($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{member} = $env;
	}

	return $env;
}

sub setup_rpc_proxy($$$)
{
	my ($self, $path, $dc_vars) = @_;

	my $env = $self->provision_rpc_proxy($path, $dc_vars);

	if (defined $env) {
	        $self->check_or_start($env, ($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{rpc_proxy} = $env;
	}
	return $env;
}

sub setup_dc($$)
{
	my ($self, $path) = @_;

	my $env = $self->provision_dc($path);
	if (defined $env) {
		$self->check_or_start($env,
			($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{dc} = $env;
	}
	return $env;
}

sub setup_fl2000dc($$)
{
	my ($self, $path) = @_;

	my $env = $self->provision_fl2000dc($path);
	if (defined $env) {
		$self->check_or_start($env,
			($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{fl2000dc} = $env;
	}

	return $env;
}

sub setup_fl2003dc($$)
{
	my ($self, $path) = @_;

	my $env = $self->provision_fl2003dc($path);

	if (defined $env) {
		$self->check_or_start($env,
			($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{fl2003dc} = $env;
	}
	return $env;
}

sub setup_fl2008r2dc($$)
{
	my ($self, $path) = @_;

	my $env = $self->provision_fl2008r2dc($path);

	if (defined $env) {
		$self->check_or_start($env,
			($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{fl2008r2dc} = $env;
	}

	return $env;
}

sub setup_vampire_dc($$$)
{
	my ($self, $path, $dc_vars) = @_;

	my $env = $self->provision_vampire_dc($path, $dc_vars);

	if (defined $env) {
		$self->check_or_start($env,
			($ENV{SMBD_MAXTIME} or 7500));

		$self->wait_for_start($env);

		$self->{vars}->{vampire_dc} = $env;

		# force replicated DC to update repsTo/repsFrom
		# for vampired partitions
		my $samba_tool = $self->bindir_path("samba-tool");
		my $cmd = "";
		$cmd .= "SOCKET_WRAPPER_DEFAULT_IFACE=\"$env->{SOCKET_WRAPPER_DEFAULT_IFACE}\"";
		$cmd .= " KRB5_CONFIG=\"$env->{KRB5_CONFIG}\"";
		$cmd .= " $samba_tool drs kcc $env->{DC_SERVER}";
		$cmd .= " -U$dc_vars->{DC_USERNAME}\%$dc_vars->{DC_PASSWORD}";
		unless (system($cmd) == 0) {
			warn("Failed to exec kcc\n$cmd");
			return undef;
		}

		# as 'vampired' dc may add data in its local replica
		# we need to synchronize data between DCs
		my $base_dn = "DC=".join(",DC=", split(/\./, $dc_vars->{REALM}));
		$cmd = "SOCKET_WRAPPER_DEFAULT_IFACE=\"$env->{SOCKET_WRAPPER_DEFAULT_IFACE}\"";
		$cmd .= " KRB5_CONFIG=\"$env->{KRB5_CONFIG}\"";
		$cmd .= " $samba_tool drs replicate $env->{DC_SERVER} $env->{VAMPIRE_DC_SERVER}";
		$cmd .= " -U$dc_vars->{DC_USERNAME}\%$dc_vars->{DC_PASSWORD}";
		# replicate Configuration NC
		my $cmd_repl = "$cmd \"CN=Configuration,$base_dn\"";
		unless(system($cmd_repl) == 0) {
			warn("Failed to replicate\n$cmd_repl");
			return undef;
		}
		# replicate Default NC
		$cmd_repl = "$cmd \"$base_dn\"";
		unless(system($cmd_repl) == 0) {
			warn("Failed to replicate\n$cmd_repl");
			return undef;
		}
	}

	return $env;
}

sub setup_rodc($$$)
{
	my ($self, $path, $dc_vars) = @_;

	my $env = $self->provision_rodc($path, $dc_vars);

	unless ($env) {
		return undef;
	}

	$self->check_or_start($env,
		($ENV{SMBD_MAXTIME} or 7500));

	$self->wait_for_start($env);

	$self->{vars}->{rodc} = $env;

	return $env;
}

1;
