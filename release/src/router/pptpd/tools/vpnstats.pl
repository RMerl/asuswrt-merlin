#!/usr/bin/perl -w
use strict;
#
# vpnstats - generate list of VPN connections from PPTP+PPP log messages
# copyright (C) 2002 Scott Merrill (skippy@skippy.net)
#
# usage: vpnstats /var/log/messages
#
# version 1.4 09-09-2003
# - thanks to Masaya Miyamoto (miyamo@po.ntts.co.jp)
#   and David Fuzishima (david_f@zipmail.com.br) for fixing the
#   date/time regexes to catch single-digit days (9 instead of 09).
#
# version 1.3
# - thanks to Andy Behrens <andy.behrens@coat.com> for
#   fixing up the regex to catch extraneous whitespace, and
#   domain names that inlucde numbers and underscores.
# - I modified the output to report when a user is still connected
# - thanks to Wolfgang Powisch for fixing hostnames included a "-"
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
my @messages = ();
my %PID_USER = ();
my %PID_IP = ();
my %PID_LENGTH = ();
my %PID_SENT = ();
my %PID_RECEIVED = ();
my %PID_DATETIME = ();
my %USER_TOTAL_CONNECT = ();
my %USER_TOTAL_TIME = ();
my %USER_TOTAL_SENT = ();
my %USER_TOTAL_RECEIVED = ();
my %vpnstats = ();

@messages = <>;

# for each line of input
foreach my $x (@messages) {
	if ($x =~ /^(\w+\s+\d+\s\d+:\d+:\d+)\s        # $1 = date+time
                \S+\spppd\[(\d+)\]:\s                # $2 = PID
		MSCHAP-v2\speer\sauthentication\ssucceeded\sfor\s
		# I don't want the DOMAIN\\ prefix
		(.+\\)*(\w+)$			     # $4 = username
		/x) {
		$PID_USER{$2} = $4;
		$PID_DATETIME{$2} = $1;
		$USER_TOTAL_CONNECT{$4}++;
	} elsif ($x =~ /^(\w+\s+\d+\s\d+:\d+:\d+)\s   # $1 = date+time
		\S+\spppd\[(\d+)\]:\s		     # $2 = PID
		Connect\stime\s
		(\d*\.\d*)			     # $3 = minutes
		\sminutes\.$
		/x) {
		$PID_LENGTH{$2} = $3;
		$USER_TOTAL_TIME{$PID_USER{$2}} += $3;
	} elsif ($x =~ /^(\w+\s+\d+\s\d+:\d+:\d+)\s   # $1 = date+time
		\S+\spppd\[(\d+)\]:\s	     	     # $2 = PID
		Sent\s(\d+)\sbytes,\s		     # $3 = bytes sent
		received\s(\d+)\s		     # $4 = bytes received
		/x) {			
		$PID_SENT{$2} = $3;
		$PID_RECEIVED{$2} = $4;
		$USER_TOTAL_SENT{$PID_USER{$2}} += $3;
		$USER_TOTAL_RECEIVED{$PID_USER{$2}} += $4;
	} elsif ($x =~ /^(\w+\s+\d+\s\d+:\d+:\d+)\s   # $1 = date+time
		\S+\spptpd\[(\d+)\]:\s		     # $2 = PID
		CTRL:\sClient\s
		(\d+\.\d+\.\d+\.\d+)\s		     # $3 = IP
		control\sconnection\sfinished$
		/x) {
		$PID_IP{($2+1)} = $3;
		if (!defined ($PID_USER{($2+1)})) {
			$PID_DATETIME{($2+1)} = $1;
			$PID_USER{($2+1)} = "FAILED";
			$USER_TOTAL_CONNECT{"FAILED"}++;
		}
	}
}
foreach my $user (sort keys %USER_TOTAL_CONNECT) {
	if (! defined $user) { next };
	if ($user ne "FAILED") {
	print $user, ": ", $USER_TOTAL_CONNECT{$user}, " connections, ";
	print $USER_TOTAL_TIME{$user}, " minutes (";
	print $USER_TOTAL_SENT{$user}, " sent, ";
	print $USER_TOTAL_RECEIVED{$user}, " received).\n";
	foreach my $pid (sort keys %PID_DATETIME) {
		if ($user eq $PID_USER{$pid}) {
			print "     ";
			print $PID_DATETIME{$pid}, ": connected ";
			if ($PID_IP{$pid}) {
				print "from $PID_IP{$pid} ";
				print "for $PID_LENGTH{$pid} minutes.\n";
			} else {
				print "<still connected>\n";
			}
		}
	}
	}
}
if (defined $USER_TOTAL_CONNECT{"FAILED"}) {
	print "\n\n";
	print "FAILED CONNECTION ATTEMPTS: ";
	print $USER_TOTAL_CONNECT{"FAILED"}, "\n";
	foreach my $pid (sort keys %PID_DATETIME) {
		if ($PID_USER{$pid} eq "FAILED") {
			print "     ";
			print $PID_DATETIME{$pid}, ": attempt from ";
			print $PID_IP{$pid}, "\n";
		}
	}
}

