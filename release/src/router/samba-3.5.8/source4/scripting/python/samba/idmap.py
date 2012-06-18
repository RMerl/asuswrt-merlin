#!/usr/bin/python

# Unix SMB/CIFS implementation.
# Copyright (C) 2008 Kai Blin <kai@samba.org>
#
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

"""Convenience functions for using the idmap database."""

__docformat__ = "restructuredText"

import samba

class IDmapDB(samba.Ldb):
    """The IDmap database."""

    # Mappings for ID_TYPE_UID, ID_TYPE_GID and ID_TYPE_BOTH
    TYPE_UID = 1
    TYPE_GID = 2
    TYPE_BOTH = 3

    def __init__(self, url=None, lp=None, modules_dir=None, session_info=None,
                 credentials=None, flags=0, options=None):
        """Opens the IDMap Database
        For parameter meanings see the super class (samba.Ldb)
        """

        self.lp = lp
        if url is None:
                url = lp.get("idmap database")

        super(IDmapDB, self).__init__(url=url, lp=lp, modules_dir=modules_dir,
                session_info=session_info, credentials=credentials, flags=flags,
                options=options)

    def connect(self, url=None, flags=0, options=None):
        super(IDmapDB, self).connect(url=self.lp.private_path(url), flags=flags,
                options=options)

    def setup_name_mapping(self, sid, type, unixid):
        """Setup a mapping between a sam name and a unix name.

        :param sid: SID of the NT-side of the mapping.
        :param unixname: Unix name to map to.
        """
        type_string = ""
        if type == self.TYPE_UID:
            type_string = "ID_TYPE_UID"
        elif type == self.TYPE_GID:
            type_string = "ID_TYPE_GID"
        elif type == self.TYPE_BOTH:
            type_string = "ID_TYPE_BOTH"
        else:
            return

        mod = """
dn: CN=%s
xidNumber: %s
objectSid: %s
objectClass: sidMap
type: %s
cn: %s

""" % (sid, unixid, sid, type_string, sid)
        self.add(self.parse_ldif(mod).next()[1])


