#!/usr/bin/perl -w

# 05/01/2005 - 18:07:10
#
# mklogon.pl - Login Script Generator
# Copyright (C) 2005 Ricky Nance
# ricky.nance@gmail.com
# http://www.weaubleau.k12.mo.us/~rnance/samba/mklogon.txt
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#

# Version: 1.0 (Stable)
# Revised: 07/28/2005

# Comments...
# Working on logging to the system logs, Logs user activity, but not errors yet.

use strict;
use Getopt::Long;

eval { require Config::Simple; };
if ($@) {
    print("\n");
    print( "It appears as though you don't have the Config Simple perl module installed.\n" );
    print("The package is typically called 'Config::Simple' \n");
    print("and it needs to be installed, before you can use this utility\n");
    print("Most PERL installations will allow you to use a command like\n");
    print("\ncpan -i Config::Simple\n");
    print("from the command line while logged in as the root user.\n");
    print("\n");
    exit(1);
}

# use Data::Dumper; #Used for debugging purposes

# This variable should point to the external conf file, personally I would set
# it to /etc/samba/mklogon.conf
my $configfile;

foreach my $dir ( ( '/etc', '/etc/samba', '/usr/local/samba/lib' ) ) {
    if ( -e "$dir/mklogon.conf" ) {
        $configfile = "$dir/mklogon.conf";
        last;
    }
}

# This section will come directly from the samba server. Basically it just makes the script easier to read.
my $getopts = GetOptions(
    'u|username=s'   => \my $user,
    'm|machine=s'    => \my $machine,
    's|servername=s' => \my $server,
    'o|ostype=s'     => \my $os,
    'i|ip=s'         => \my $ip,
    'd|date=s'       => \my $smbdate,
    'h|help|?'       => \my $help
);

if ($help) {
    help();
    exit(0);
}

# We want the program to error out if its missing an argument.
if ( !defined($user) )    { error("username"); }
if ( !defined($machine) ) { error("machine name") }
if ( !defined($server) )  { error("server name") }
if ( !defined($os) )      { error("operating system") }
if ( !defined($ip) )      { error("ip address") }
if ( !defined($smbdate) ) { error("date") }

# This section will be read from the external config file
my $cfg = new Config::Simple($configfile) or die "Could not find $configfile";

# Read this part from the samba config
my ( $sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst ) = localtime(time);
my $sambaconf = $cfg->param("global.sambaconf") or die "Couldn't find your samba config! \n";
my $smbcfg = new Config::Simple( filename => $sambaconf, syntax => "ini" );
my $smbprof = $smbcfg->param("profiles.path");
my $smbnetlogdir = $smbcfg->param("netlogon.path");
my $logging      = lc( $cfg->param("global.logging") );
my $mkprofile    = lc( $cfg->param("global.mkprofile") );
my $logdir       = $cfg->param("global.logdir");
my $logfile      = $cfg->param("global.logfile");
my $logs         = "$logdir\/$logfile";
my $logtype	 = $cfg->param("global.logtype");
my $usermap      = "usermap.$user";
my $osmap	 = "os.$os";
my @ostype	 = $cfg->param($osmap);
my @username     = $cfg->param($usermap);
my $compname     = $cfg->param( -block => "machines" );
my $ipname       = $cfg->param( -block => "ip" );
my $timesync     = $cfg->param("global.timesync");
my $altserver    = $cfg->param("global.servername");
if ( defined($altserver) ) { $server = $altserver; }
$server = uc($server);

# Lets start logging stuff if it is turned on in the config
if ( $logging =~ m/on|yes|1/i ) {
    if ($logtype =~ m/file/i) {
	print "----- Logging is turned on in the config. -----\n";
	print "----- Location of the logfile is \"$logs\" -----\n";
	open LOG, ">>$logs";
	printf LOG "Date: $smbdate Time: ";
	printf LOG '%02d', $hour;
	print LOG ":";
	printf LOG '%02d', $min;
	print LOG ".";
	printf LOG '%02d', $sec;
	print LOG " -- User: $user - Machine: $machine - IP: $ip -- \n";
	close(LOG);
    } elsif ($logtype =~ m/syslog|system/i){
	use Sys::Syslog;
	my $alert = "User: $user Logged into $machine ($ip) at $hour:$min.$sec on $smbdate.";
	openlog($0, 'cons', 'user');
        syslog('alert', $alert);
	closelog();

    }
} else {
    print "----- Logging is turned off in the config. -----\n";
}

# If the user wants to make profiles with this script lets go
if ( defined($smbprof) ) {
    if ( $mkprofile =~ m/on|yes|1/i ) {
        print "----- Automatic making of user profiles is turned on in the config. ----- \n";
        ( my $login, my $pass, my $uid, my $gid ) = getpwnam($user)
          or die "$user not in passwd file \n";
        $smbprof =~ s/\%U/$user/g;
        my $dir2 = "$smbprof\/$user";
        print "$smbprof \n";
        print "$dir2 \n";
        if ( !-e $dir2 ) {
            print "Creating " . $user . "'s profile with a uid of $uid\n";
            mkdir $smbprof;
            mkdir $dir2;
            chomp($user);
#           chown $uid, $gid, $smbprof;
            chown $uid, $gid, $dir2;
        } else {
            print $user . "'s profile already exists \n";
        }
    } else {
        print "----- Automatic making of user profiles is turned off in the config. ----- \n";
    }
}

# Lets start making the batch files.
open LOGON, ">$smbnetlogdir\/$user.bat" or die "Unable to create userfile $smbnetlogdir\/$user.bat";
print LOGON "\@ECHO OFF \r\n";

if ( $timesync =~ m/on|yes|1/i ) {
    print LOGON "NET TIME /SET /YES \\\\$server \r\n";
} else {
    print "----- Time syncing to the client is turned off in the config. -----\n";
}

# Mapping from the common section
my $common = $cfg->param( -block => "common" );
for my $key ( keys %$common ) {
    drive_map( @{ $common->{$key} } );
}

my @perform_common = $cfg->param("performcommands.common");
if ( defined( $perform_common[0] ) ) {
    foreach (@perform_common) {
        print LOGON "$_ \r\n";
    }
}

# Map shares on a per user basis.
drive_map(@username);

# Map shares based on the Operating System.
drive_map(@ostype);

# Map shares only if they are in a group
# This line checks against the unix "groups" command, to see the secondary groups of a user.
my @usergroups = split( /\s/, do { open my $groups, "-|", groups => $user; <$groups> } );
foreach (@usergroups) {
    my $groupmap  = "groupmap.$_";
    my @groupname = $cfg->param($groupmap);
    drive_map(@groupname);
}

#Here is where we check the machine name against the config...
for my $key ( keys %$compname ) {
    my $test = $compname->{$key};
    if ( ref $test eq 'ARRAY' ) {
        foreach (@$test) {
            if ( $_ eq $machine ) {
                my $performit = $cfg->param("performcommands.$key");
                if ( defined($performit) ) {
                    if ( ref $performit ) {
                        foreach (@$performit) { print LOGON "$_ \r\n"; }
                    } else {
                        print LOGON "$performit \r\n";
                    }
                }
            }
        }
    }
    elsif ( $test eq $machine ) {
        my $performit = $cfg->param("performcommands.$key");
        if ( defined($performit) ) {
            if ( ref $performit ) {
                foreach (@$performit) { print LOGON "$_ \r\n"; }
            } else {
                print LOGON "$performit \r\n";
            }
        }
    }
}

# Here is where we test the ip address against the client to see if they have "Special Mapping"
# A huge portion of the ip matching code was made by  
# Carsten Schaub (rcsu in the #samba chan on freenode.net)

my $val;
for my $key ( sort keys %$ipname ) {
    if ( ref $ipname->{$key} eq 'ARRAY' ) {
        foreach ( @{ $ipname->{$key} } ) {
            getipval( $_, $key );
        }
    } else {
        getipval( $ipname->{$key}, $key );
    }
}

sub getipval {
    my ( $range, $rangename ) = @_;
    if ( parse( $ip, ipmap($range) ) ) {
        if ( $val eq 'true' ) {
            my $performit = $cfg->param("performcommands.$rangename");
            if ( defined($performit) ) {
                if ( ref $performit ) {
                    foreach (@$performit) { print LOGON "$_ \r\n"; }
                } else {
                    print LOGON "$performit \r\n";
                }
            }
        } elsif ( $val eq 'false' ) {
        }
    } else {
    }
}

sub ipmap {
    my $pattern = shift;
    my ( $iprange, $iprange2, $ipmask );
    if ( $pattern =~ m/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\/(\d{1,2})$/ ) {
        # 1.1.1.1/3 notation
        $iprange = pack( "U4", $1, $2, $3, $4 );
        $ipmask = pack( "U4", 0, 0, 0, 0 );
        my $numbits = $5;
        for ( my $i = 0 ; $i < $numbits ; $i++ ) {
            vec( $ipmask, int( $i / 8 ) * 8 + ( 8 - ( $i % 8 ) ) - 1, 1 ) = 1;
        }
        $iprange &= "$ipmask";
    } elsif ( $pattern =~ m/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\/(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})/ ) {
        # 1.1.1.1/255.255.255.255 notation
        $iprange = pack( "U4", $1, $2, $3, $4 );
        $ipmask  = pack( "U4", $5, $6, $7, $8 );
        $iprange &= "$ipmask";
    } elsif ( $pattern =~ m/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/ ) {
        # 1.1.1.1 notation
        $iprange = pack( "U4", $1, $2, $3, $4 );
        $ipmask = pack( "U4", 255, 255, 255, 255 );
    } elsif ( $pattern =~ m/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\s*\-\s*(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/ ) {
        # 1.1.1.1 - 2.2.2.2 notation
        $iprange  = pack( "U4", $1,  $2,  $3,  $4 );
        $iprange2 = pack( "U4", $5,  $6,  $7,  $8 );
        $ipmask   = pack( "U4", 255, 255, 255, 255 );
    } else {
        return;
    }
	return $iprange, $ipmask, $iprange2;
}

sub parse {
    my ( $origip, $ipbase, $ipmask, $iprange2 ) = @_;
    $origip =~ m/^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/;
    $origip = pack( "U4", $1, $2, $3, $4 );
    if ( defined($iprange2) ) {
        if ( $ipbase le $origip && $origip le $iprange2 ) {
            return $val = 'true';
        } else {
            return $val = 'false';
        }
    } elsif ( ( "$origip" & "$ipmask" ) eq $ipbase ) {
        return $val = 'true';
    } else {
        return $val = 'false';
    }
}

# This sub will distinguish the drive mappings
sub drive_map {
    my @data = @_;
    for ( my $i = 0 ; $i < scalar(@data) ; ) {
        if ( $data[$i] =~ m/^[a-z]\:$/i ) {
            my $driveletter = $data[$i];
            $i++;
            my $sharename = $data[$i];
            $i++;
            if ( $sharename eq '/home' ) {
                print LOGON uc("NET USE $driveletter \\\\$server\\$user \/Y \r\n");
            } else {
                print LOGON
                  uc("NET USE $driveletter \\\\$server\\$sharename \/Y \r\n");
            }
        } else {
            print LOGON uc("$data[$i] \r\n");
            $i++;
        }
    }
}

close(LOGON);

sub error {
    my $var = shift(@_);
    help();
    print "\n\tCritical!!! \n\n\tNo $var specified\n\n\tYou must specify a $var.\n\n";
    exit(0);
}

sub help {

    print << "EOF" ;

	Usage:   $0 [options]

	Options:

	-h,--help		This help screen.

	-u,--username		The name of the user from the samba server.

	-m,--machinename	The name of the client connecting to the server.

	-s,--server		The name of the server this script is running in.

	-o,--os			The clients OS -- Windows 95/98/ME (Win95), Windows NT (WinNT),
				Windows 2000 (Win2K), Windows  XP  (WinXP), and Windows 2003
				(Win2K3). Anything else will be known as ``UNKNOWN''
				That snippet is directly from man smb.conf. 

	-i,--ip			The clients IP address.

	-d,--date		Time and Date returned from the samba server.



				--IMPORTANT--		
		

				All options MUST be specified.
	
				The mklogon.conf file MUST be located in /etc, /etc/samba, or 
				/usr/local/samba/lib.
	
	To use this file from the command line:
		$0 -u User -m machine -s servername -o ostype -i X.X.X.X -d MM/DD/YY

	To use this file from the samba server add these lines to your /etc/samba/smb.conf:


		This line goes in the [global] section
			login script = %U.bat

		This line should be at the end of the [netlogon] section.
			root preexec = /path/to/mklogon.pl -u %U -m %m -s %L -o %a -i %I -d %t
	
	
EOF

    print "\n\n";

}
