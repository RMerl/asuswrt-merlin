#!/usr/bin/perl
##   Add printer script for samba, APW, and cups
##   Copyright (C) Jeff Hardy <hardyjm@potsdam.edu> 2004
##
##   This program is free software; you can redistribute it
##   and/or modify it under the terms of the GNU General
##   Public License as published by the Free Software Foundation;
##   Either version 3 of the License, or (at your option) any
##   later version.
##
##   This program is distributed in the hope that it will be useful,
##   but WITHOUT ANY WARRANTY; without even the implied warranty of
##   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##   GNU General Public License for more details.
##
##   You should have received a copy of the GNU General Public
##   License along with this program; if not, see <http://www.gnu.org/licenses/>.

@argv = @ARGV;

# take in args
my $lpname=shift(@argv);	# printer name
my $shname=shift(@argv);	# share name	-> used for CUPS queue name
my $portname=shift(@argv);	# port name
my $drivername=shift(@argv);	# driver name	-> used for CUPS description
my $location=shift(@argv);	# location	-> used for CUPS device URI
my $win9x=shift(@argv);		# win9x location

#check for location syntax
#if no protocol specified...
if ($location !~ m#:/#){
	#assume an lpd printer
	$location = "lpd://".$location;
}
#else, simply pass the URI on to the lpadmin command

#run the cups lpadmin command to add the printer
system("/usr/sbin/lpadmin -p $shname -D \"$drivername\" -E -v $location");

