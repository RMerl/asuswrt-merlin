#!/usr/bin/python

# Unix SMB/CIFS implementation.
# Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2008
#   
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#   
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#   
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from samdb import SamDB

class Hostconfig(object):
    """Aggregate object that contains all information about the configuration 
    of a Samba host."""

    def __init__(self, lp):       
        self.lp = lp

    def get_samdb(self, session_info, credentials):
        return SamDB(url=self.lp.get("sam database"), 
                     session_info=session_info, credentials=credentials, 
                     lp=self.lp)

