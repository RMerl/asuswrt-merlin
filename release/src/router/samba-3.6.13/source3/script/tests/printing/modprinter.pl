#!/usr/bin/perl -w

use strict;

use Getopt::Long;
use Cwd qw(abs_path);

my $opt_help = 0;
my $opt_smb_conf = undef;
my $opt_add = 0;
my $opt_delete = 0;

my $result = GetOptions(
	'help|h|?'	=> \$opt_help,
	'smb_conf|s=s'	=> \$opt_smb_conf,
	'add|a'		=> \$opt_add,
	'delete|d'	=> \$opt_delete
);

sub usage($;$)
{
	my ($ret, $msg) = @_;

	print $msg."\n\n" if defined($msg);

	print "usage:

	--help|-h|-?		Show this help.

	--smb_conf|-s <path>	Path of the 'smb.conf' file.

	--add|-a		'add' a printer.
	--delete|-d		'delete' a printer.

	printer_name share_name port_name driver_name location XX remote_machine
";
	exit($ret);
}

usage(1) if (not $result);

usage(0) if ($opt_help);

if (!$opt_add && !$opt_delete) {
	usage(1, "invalid: neither --add|-a nor --delete|-d set");
}

if (!$opt_smb_conf) {
	usage(1, "invalid: no smb.conf file set");
}

my @argv = @ARGV;

my $printer_name = shift(@argv);
my $share_name = shift(@argv);
my $port_name = shift(@argv);
my $driver_name = shift(@argv);
my $location = shift(@argv);
my $win9x_driver_location = shift(@argv);
my $remote_machine = shift(@argv);

if (!defined($share_name) || length($share_name) == 0) {
	$share_name = $printer_name;
}

if (!defined($share_name)) {
	die "share name not defined";
}

my $tmp = $opt_smb_conf.$$;

my $section = undef;
my $within_section = 0;
my $found_section = 0;

open(CONFIGFILE_NEW, "+>$tmp") || die "Unable top open conf file $tmp";

open (CONFIGFILE, "+<$opt_smb_conf") || die "Unable to open config file $opt_smb_conf";
while (<CONFIGFILE>) {
	my $line = $_;
	chomp($_);
	$_ =~ s/^\s*//;
	$_ =~ s/\s*$//;
	if (($_ =~ /^#/) || ($_ =~ /^;/)) {
		print CONFIGFILE_NEW $line;
		next;
	}
	if ($_ =~ /^\[.*\]$/) {
		$_ = substr($_, 1, length($_)-2);
		if (length($_)) {
			$section = $_;
		} else {
			die "invalid section found";
		}
		if ($section eq $share_name) {
			$found_section = 1;
			if ($opt_add) {
				exit 0;
#				die("share $share_name already exists\n");
			}
			if ($opt_delete) {
				$within_section = 1;
				next;
			}
		} else {
			print CONFIGFILE_NEW $line;
			$within_section = 0;
		}
		next;
	} else {
		if ($within_section == 1) {
			next;
		}
		print CONFIGFILE_NEW $line;
	}
}
if ($opt_add) {
	print CONFIGFILE_NEW "[$share_name]\n\tprintable = yes\n\tpath = /tmp\n";
}
close (CONFIGFILE);
close (CONFIGFILE_NEW);

if ($opt_delete && ($found_section == 0)) {
	die "share $share_name not found";
}
system("cp", "$tmp", "$opt_smb_conf");
unlink $tmp;

exit 0;
