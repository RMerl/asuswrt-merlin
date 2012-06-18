#!/usr/bin/perl
# Start a KVM machine and run a number of tests against it.
# Copyright (C) 2005-2008 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later.

package Kvm;

use strict;
use Cwd qw(abs_path);
use FindBin qw($RealBin);
use POSIX;

sub new($$$$) {
	my ($classname, $dc_image, $vdesocket) = @_;
	my $self = { 
		dc_image => $dc_image,
		vdesocket => $vdesocket,
	};
	bless $self;
	return $self;
}

sub write_kvm_ifup($$$)
{
	my ($self, $path, $ip_prefix) = @_;
	open(SCRIPT, ">$path/kvm-ifup");

	print SCRIPT <<__EOF__;
#!/bin/sh

PREFIX=$ip_prefix

/sbin/ifconfig \$1 \$PREFIX.1 up

cat <<EOF>$path/udhcpd.conf
interface \$1
start 		\$PREFIX.20
end		\$PREFIX.20
max_leases 1
lease_file	$path/udhcpd.leases
pidfile	$path/udhcpd.pid
EOF

touch $path/udhcpd.leases

/usr/sbin/udhcpd $path/udhcpd.conf

exit 0
__EOF__
	close(SCRIPT);
	chmod(0755, "$path/kvm-ifup");

	return ("$path/kvm-ifup", "$path/udhcpd.pid", "$ip_prefix.20");
}

sub teardown_env($$)
{
	my ($self, $envvars) = @_;

	print "Killing kvm instance $envvars->{KVM_PID}\n";

	kill 9, $envvars->{KVM_PID};

	if (defined($envvars->{DHCPD_PID})) {
		print "Killing dhcpd instance $envvars->{DHCPD_PID}\n";
		kill 9, $envvars->{DHCPD_PID};
	}

	return 0;
}

sub getlog_env($$)
{
	my ($self, $envvars) = @_;

	return "";
}

sub check_env($$)
{
	my ($self, $envvars) = @_;

	# FIXME: Check whether $self->{pid} is still running

	return 1;
}

sub read_pidfile($)
{
	my ($path) = @_;

	open(PID, $path);
	<PID> =~ /([0-9]+)/;
	my ($pid) = $1;
	close(PID);
	return $pid;
}

sub start($$$)
{
	my ($self, $path, $image) = @_;

	my $pidfile = "$path/kvm.pid";

	my $opts = (defined($ENV{KVM_OPTIONS})?$ENV{KVM_OPTIONS}:"-nographic");

	if (defined($ENV{KVM_SNAPSHOT})) {
		$opts .= " -loadvm $ENV{KVM_SNAPSHOT}";
	}

	my $netopts;
	my $dhcp_pid;
	my $ip_address;

	if ($self->{vdesocket}) {
		$netopts = "vde,socket=$self->{vdesocket}";
	} else {
		my $ifup_script, $dhcpd_pidfile;
		($ifup_script, $dhcpd_pidfile, $ip_address) = $self->write_kvm_ifup($path, "192.168.9");
		$netopts = "tap,script=$ifup_script";
		$dhcp_pid = read_pidfile($dhcpd_pidfile);
	}

	system("kvm -name \"Samba 4 Test Subject\" $opts -monitor unix:$path/kvm.monitor,server,nowait -daemonize -pidfile $pidfile -snapshot $image -net nic -net $netopts");

	return (read_pidfile($pidfile), $dhcp_pid, $ip_address);
}

sub setup_env($$$)
{
	my ($self, $envname, $path) = @_;

	if ($envname eq "dc") {
		($self->{dc_pid}, $self->{dc_dhcpd_pid}, $self->{dc_ip}) = $self->start($path, $self->{dc_image});

		sub choose_var($$) { 
			my ($name, $default) = @_; 
			return defined($ENV{"KVM_DC_$name"})?$ENV{"KVM_DC_$name"}:$default; 
		}

		if ($envname eq "dc") {
			return {
				KVM_PID => $self->{dc_pid},
				DHCPD_PID => $self->{dc_dhcpd_pid},
				USERNAME => choose_var("USERNAME", "Administrator"),
				PASSWORD => choose_var("PASSWORD", "penguin"),
				DOMAIN => choose_var("DOMAIN", "SAMBA"), 
				REALM => choose_var("REALM", "SAMBA"), 
				SERVER => choose_var("SERVER", "DC"), 
				SERVER_IP => $self->{dc_ip},
				NETBIOSNAME => choose_var("NETBIOSNAME", "DC"), 
				NETBIOSALIAS => choose_var("NETBIOSALIAS", "DC"), 
			};
		} else {
			return undef;
		}
	} else {
		return undef;
	}
}

sub stop($)
{
	my ($self) = @_;
}

1;
