#!/usr/bin/perl
# Bootstrap Samba and run a number of tests against it.
# Copyright (C) 2005-2007 Jelmer Vernooij <jelmer@samba.org>

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



package SocketWrapper;

use Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(setup_dir setup_pcap set_default_iface);

use strict;
use FindBin qw($RealBin);

sub setup_dir($$)
{
	my ($dir, $pcap) = @_;
	my $pcap_dir = undef;

	if (defined($dir)) {
		if ( -d $dir ) {
			unlink <$dir/*>;
		} else {
			mkdir($dir, 0777);
		}

		if ($pcap) {
			$pcap_dir = $dir."/pcap";

			if ( -d $pcap_dir ) {
				unlink <$pcap_dir/*>;
			} else {
				mkdir($pcap_dir, 0777);
			}
		}
	}

	if (defined($pcap_dir)) {
		$ENV{SOCKET_WRAPPER_PCAP_DIR} = $pcap_dir;
	} else {
		delete $ENV{SOCKET_WRAPPER_PCAP_DIR};
	}

	if (defined($dir)) {
		$ENV{SOCKET_WRAPPER_DIR} = $dir;
	} else {
		delete $ENV{SOCKET_WRAPPER_DIR};
	}

	return $dir;
}

sub setup_pcap($)
{
	my ($pcap_file) = @_;

	$ENV{SOCKET_WRAPPER_PCAP_FILE} = $pcap_file;
}

sub set_default_iface($)
{
	my ($i) = @_;
	$ENV{SOCKET_WRAPPER_DEFAULT_IFACE} = $i;
}

1;
