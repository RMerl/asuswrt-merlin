#!/usr/bin/perl -w

# A perl object to provide a simple, unified method of handling some
# VMware Server VM management functions using the perl and VIX API's.
# Copyright Brad Henry <brad@samba.org> 2006
# Released under the GNU GPL version 3 or later.

# VMware Perl API
use VMware::VmPerl;
use VMware::VmPerl::VM;
use VMware::VmPerl::ConnectParams;

# VMware C bindings
use VMware::Vix::Simple;
use VMware::Vix::API::Constants;

# Create a class to abstract from the Vix and VMPerl APIs.
{ package VMHost;
	my $perl_vm = VMware::VmPerl::VM::new();
	my $perl_vm_credentials;
	my $vix_vm;
	my $vix_vm_host;

	my $err_code = 0;
	my $err_str = "";

	my $hostname;
	my $port;
	my $username;
	my $password;
	my $vm_cfg_path;
	my $guest_admin_username;
	my $guest_admin_password;

	sub error {
		my $old_err_code = $err_code;
		my $old_err_str = $err_str;
		$err_code = 0;
		$err_str = "";
		return ($old_err_code, $old_err_str);
	}

	# Power on the guest if it isn't already running.
	# Returns 0 when the guest is already running, and
	# if not, it waits until it is started.
	sub start_guest {
		my $vm_power_state = $perl_vm->get_execution_state();
		if (!defined($vm_power_state)) {
			($err_code, $err_str) = $perl_vm->get_last_error();
			return ($err_code);
		}
		if ($vm_power_state == VMware::VmPerl::VM_EXECUTION_STATE_OFF
			|| $vm_power_state ==
				VMware::VmPerl::VM_EXECUTION_STATE_SUSPENDED)
		{
			if (!$perl_vm->start()) {
				($err_code, $err_str) =
					$perl_vm->get_last_error();
				return ($err_code);
			}
			while ($perl_vm->get_tools_last_active() == 0) {
				sleep(60);
			}
		}
		return ($err_code);
	}

	sub host_connect {
		# When called as a method, the first parameter passed is the
		# name of the method. Called locally, this function will lose
		# the first parameter.
		shift @_;
		($hostname, $port, $username, $password, $vm_cfg_path,
			$guest_admin_username, $guest_admin_password) = @_;

		# Connect to host using vmperl api.
		$perl_vm_credentials =
			VMware::VmPerl::ConnectParams::new($hostname, $port,
				$username, $password);
		if (!$perl_vm->connect($perl_vm_credentials, $vm_cfg_path)) {
			($err_code, $err_str) = $perl_vm->get_last_error();
			undef $perl_vm;
			return ($err_code);
		}

		# Connect to host using vix api.
		($err_code, $vix_vm_host) =
			VMware::Vix::Simple::HostConnect(
				VMware::Vix::Simple::VIX_API_VERSION,
				VMware::Vix::Simple::VIX_SERVICEPROVIDER_VMWARE_SERVER,
				$hostname, $port, $username, $password,
				0, VMware::Vix::Simple::VIX_INVALID_HANDLE);
		if ($err_code != VMware::Vix::Simple::VIX_OK) {
			$err_str =
				VMware::Vix::Simple::GetErrorText($err_code);
			undef $perl_vm;
			undef $vix_vm;
			undef $vix_vm_host;
			return ($err_code);
		}

		# Power on our guest os if it isn't already running.
		$err_code = start_guest();
		if ($err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Starting guest power after connect " .
					"failed: " . $old_err_str;
			undef $perl_vm;
			undef $vix_vm;
			undef $vix_vm_host;
			return ($err_code);
		}

		# Open VM.
		($err_code, $vix_vm) =
			VMware::Vix::Simple::VMOpen($vix_vm_host, $vm_cfg_path);
		if ($err_code != VMware::Vix::Simple::VIX_OK) {
			$err_str =
				VMware::Vix::Simple::GetErrorText($err_code);
			undef $perl_vm;
			undef $vix_vm;
			undef $vix_vm_host;
			return ($err_code);
		}

		# Login to $vix_vm guest OS.
		$err_code = VMware::Vix::Simple::VMLoginInGuest($vix_vm,
				$guest_admin_username, $guest_admin_password,
				0);
		if ($err_code != VMware::Vix::Simple::VIX_OK) {
			$err_str =
				VMware::Vix::Simple::GetErrorText($err_code);
			undef $perl_vm;
			undef $vix_vm;
			undef $vix_vm_host;
			return ($err_code);
		}
		return ($err_code);
	}

	sub host_disconnect {
		undef $perl_vm;

		$perl_vm = VMware::VmPerl::VM::new();
		if (!$perl_vm) {
			$err_code = 1;
			$err_str = "Error creating new VmPerl object";
		}

		undef $vix_vm;
		VMware::Vix::Simple::HostDisconnect($vix_vm_host);
		VMware::Vix::Simple::ReleaseHandle($vix_vm_host);
		return ($err_code);
	}

	sub host_reconnect {
		$err_code = host_disconnect();
		if ($err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Disconnecting from host failed: " .
					$old_err_str;
			return ($err_code);
		}

		$err_code = host_connect(NULL, $hostname, $port, $username,
				$password, $vm_cfg_path, $guest_admin_username,
				$guest_admin_password);
		if ($err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Re-connecting to host failed: " .
					$old_err_str;
			return ($err_code);
		}
		return ($err_code);
	}

	sub create_snapshot {
		my $snapshot;

		($err_code, $snapshot) =
			VMware::Vix::Simple::VMCreateSnapshot($vix_vm,
			"Snapshot", "Created by vm_setup.pl", 0,
			VMware::Vix::Simple::VIX_INVALID_HANDLE);

		VMware::Vix::Simple::ReleaseHandle($snapshot);

		if ($err_code != VMware::Vix::Simple::VIX_OK) {
			$err_str =
				VMware::Vix::Simple::GetErrorText($err_code);
			return $err_code;
		}

		$err_code = host_reconnect();
		if ($err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Reconnecting to host after creating " .
					"snapshot: " . $old_err_str;
			return ($err_code);
		}
		return ($err_code);
	}

	sub revert_snapshot {
		# Because of problems with VMRevertToSnapshot(), we have to
		# rely on the guest having set 'Revert to Snapshot' following
		# a power-off event.
		$err_code = VMware::Vix::Simple::VMPowerOff($vix_vm, 0);
		if ($err_code != VMware::Vix::Simple::VIX_OK) {
			$err_str =
				VMware::Vix::Simple::GetErrorText($err_code);
			return $err_code;
		}

		# host_reconnect() will power-on a guest in a non-running state.
		$err_code = host_reconnect();
		if ($err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Reconnecting to host after reverting " .
					"snapshot: " . $old_err_str;
			return ($err_code);
		}
		return ($err_code);
	}

	# $dest_path must exist. It doesn't get created.
	sub copy_files_to_guest {
		shift @_;
		my (%files) = @_;

		my $src_file;
		my $dest_file;

		foreach $src_file (keys(%files)) {
			$dest_file = $files{$src_file};
			$err_code =
				VMware::Vix::Simple::VMCopyFileFromHostToGuest(
					$vix_vm, $src_file, $dest_file, 0,
					VMware::Vix::Simple::VIX_INVALID_HANDLE);
			if ($err_code != VMware::Vix::Simple::VIX_OK) {
				$err_str = "Copying $src_file: " .
					VMware::Vix::Simple::GetErrorText(
						$err_code);
				return $err_code;
			}
		}
		return $err_code;
	}

	sub copy_to_guest {
		# Read parameters $src_path, $dest_path.
		shift @_;
		my ($src_path, $dest_dir) = @_;

		my $len = length($dest_dir);
		my $idx = rindex($dest_dir, '\\');
		if ($idx != ($len - 1)) {
			$err_code = -1;
			$err_str = "Destination $dest_dir must be a " .
					"directory path";
			return ($err_code);
		}

		# Create the directory $dest_path on the guest VM filesystem.
		my $cmd = "cmd.exe ";
		my $cmd_args = "/C MKDIR " . $dest_dir;
		$err_code = run_on_guest(NULL, $cmd, $cmd_args);
		if ( $err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Creating directory $dest_dir on host: " .
					$old_err_str;
			return ($err_code);
		}

		# If $src_filepath specifies a file, create it in $dest_path
		# and keep the same name.
		# If $src_path is a directory, create the files it contains in
		# $dest_path, keeping the same names.
		$len = length($src_path);
		my %files;
		$idx = rindex($src_path, '/');
		if ($idx == ($len - 1)) {
			# $src_path is a directory.
			if (!opendir (DIR_HANDLE, $src_path)) {
				$err_code = -1;
				$err_str = "Error opening directory $src_path";
				return $err_code;
			}

			foreach $file (readdir DIR_HANDLE) {
				my $src_file = $src_path . $file;

				if (!opendir(DIR_HANDLE2, $src_file)) {
					# We aren't interested in subdirs.
					my $dest_path = $dest_dir . $file;
					$files{$src_file} = $dest_path;
				} else {
					closedir(DIR_HANDLE2);
				}
			}
		} else {
			# Strip if preceeding path from $src_path.
			my $src_file = substr($src_path, ($idx + 1), $len);
			my $dest_path = $dest_dir . $src_file;

			# Add $src_path => $dest_path to %files.
			$files{$src_path} = $dest_path;
		}

		$err_code = copy_files_to_guest(NULL, %files);
		if ($err_code != 0) {
			my $old_err_str = $err_str;
			$err_str = "Copying files to host after " .
				"populating %files: " . $old_err_str;
			return ($err_code);
		}
		return ($err_code);
	}

	sub run_on_guest {
		# Read parameters $cmd, $cmd_args.
		shift @_;
		my ($cmd, $cmd_args) = @_;

		$err_code = VMware::Vix::Simple::VMRunProgramInGuest($vix_vm,
				$cmd, $cmd_args, 0,
				VMware::Vix::Simple::VIX_INVALID_HANDLE);
		if ($err_code != VMware::Vix::Simple::VIX_OK) {
			$err_str = VMware::Vix::Simple::GetErrorText(
					$err_code);
			return ($err_code);
		}

		return ($err_code);
	}

	sub get_guest_ip {
		my $guest_ip = $perl_vm->get_guest_info('ip');

		if (!defined($guest_ip)) {
			($err_code, $err_str) = $perl_vm->get_last_error();
			return NULL;
		}

		if (!($guest_ip)) {
			$err_code = 1;
			$err_str = "Guest did not set the 'ip' variable";
			return NULL;
		}
		return $guest_ip;
	}

	sub DESTROY {
		host_disconnect();
		undef $perl_vm;
		undef $vix_vm_host;
	}
}

return TRUE;
