#!/usr/bin/env python

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

"""Local host configuration."""

from samdb import SamDB

class Hostconfig(object):
    """Aggregate object that contains all information about the configuration
    of a Samba host."""

    def __init__(self, lp):
        self.lp = lp

    def get_shares(self):
        return SharesContainer(self.lp)

    def get_samdb(self, session_info, credentials):
        """Access the SamDB host.

        :param session_info: Session info to use
        :param credentials: Credentials to access the SamDB with
        """
        return SamDB(url=self.lp.get("sam database"),
                     session_info=session_info, credentials=credentials,
                     lp=self.lp)


# TODO: Rather than accessing Loadparm directly here, we should really
# have bindings to the param/shares.c and use those.


class SharesContainer(object):
    """A shares container."""

    def __init__(self, lp):
        self._lp = lp

    def __getitem__(self, name):
        if name == "global":
            # [global] is not a share
            raise KeyError
        return Share(self._lp[name])

    def __len__(self):
        if "global" in self._lp.services():
            return len(self._lp)-1
        return len(self._lp)

    def keys(self):
        return [name for name in self._lp.services() if name != "global"]

    def __iter__(self):
        return iter(self.keys())


class Share(object):
    """A file share."""

    def __init__(self, service):
        self._service = service

    def __getitem__(self, name):
        return self._service[name]

    def __setitem__(self, name, value):
        self._service[name] = value
