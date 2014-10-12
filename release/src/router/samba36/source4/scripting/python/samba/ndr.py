# -*- coding: utf-8 -*-

# Unix SMB/CIFS implementation.
# Copyright © Jelmer Vernooij <jelmer@samba.org> 2008
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


"""Network Data Representation (NDR) marshalling and unmarshalling."""


def ndr_pack(object):
    """Pack a NDR object.
    
    :param object: Object to pack
    :return: String object with marshalled object.
    """
    ndr_pack = getattr(object, "__ndr_pack__", None)
    if ndr_pack is None:
        raise TypeError("%r is not a NDR object" % object)
    return ndr_pack()


def ndr_unpack(cls, data):
    """NDR unpack an object.

    :param cls: Class of the object to unpack
    :param data: Buffer to unpack
    :return: Unpacked object
    """
    object = cls()
    object.__ndr_unpack__(data)
    return object


def ndr_print(object):
    return object.__ndr_print__()
