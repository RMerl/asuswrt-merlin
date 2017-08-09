#!/usr/bin/perl
######################################################################
##
##  Simple parselog script for Samba
##
##  Copyright (C) Brian Moran                2005.
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 3 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, see <http://www.gnu.org/licenses/>.
##
######################################################################

## usage: tail -f /var/log/syslog | parselog.pl | eventlogadm -o write "Application"

while(<>) {
    chomp();
    @le = split '\s+',$_,5;
    $ln = $le[4];
    $cname = $le[3]; 
    $outstr = sprintf "TMG: %d\nTMW: %d\nEID: 1000\nETP: INFO\nECT: 0\nRS2: 0\nCRN: 0\nUSL: 0\nSRC: Syslog\nSRN: $cname\nSTR: $ln\nDAT:\n\n",time(),time();
    print $outstr;
}
