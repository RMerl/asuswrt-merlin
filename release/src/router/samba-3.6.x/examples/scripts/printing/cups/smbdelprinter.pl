#!/usr/bin/perl
##   Delete printer script for samba, APW, and cups
##   Copyright (C) Gerald (Jerry) Carter <jerry@samba.rog>    2004
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

system("/usr/sbin/lpadmin -x $lpname");

