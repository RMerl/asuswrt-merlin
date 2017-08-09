#!/usr/bin/perl -w

# A perl script to connect to a VMware server and get the IP address of a VM.
# Copyright Brad Henry <brad@samba.org> 2006
# Released under the GNU GPL version 3 or later.

use VMHost;

sub check_error {
	my $vm = VMHost;
	my $custom_err_str = "";
	($vm, $custom_err_str) = @_;

	my ($err_code, $err_str) = $vm->error;
	if ($err_code != 0) {
		undef $vm;
		die $custom_err_str . "Returned $err_code: $err_str.\n";
	}
}

# Read in parameters from environment.
my $vm_cfg_path = $ENV{"$ARGV[0]"};
my $host_server_name = $ENV{'HOST_SERVER_NAME'};
my $host_server_port = $ENV{'HOST_SERVER_PORT'};
if (!defined($host_server_port)) {
	$host_server_port = 902;
}

my $host_username = $ENV{'HOST_USERNAME'};
my $host_password = $ENV{'HOST_PASSWORD'};
my $guest_admin_username = $ENV{'GUEST_ADMIN_USERNAME'};
my $guest_admin_password = $ENV{'GUEST_ADMIN_PASSWORD'};

my $vm = VMHost;

$vm->host_connect($host_server_name, $host_server_port, $host_username,
			$host_password, $vm_cfg_path, $guest_admin_username,
			$guest_admin_password);
check_error($vm, "Error in \$vm->host_connect().\n");

my $guest_ip = $vm->get_guest_ip();
check_error($vm, "Error in \$vm->get_guest_ip().\n");

print $guest_ip;

undef $vm;

exit 0;
