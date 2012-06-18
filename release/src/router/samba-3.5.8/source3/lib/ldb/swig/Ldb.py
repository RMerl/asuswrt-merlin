"""Provide a more Pythonic and object-oriented interface to ldb."""

#
# Swig interface to Samba
#
# Copyright (C) Tim Potter 2006
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
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#

#
# Interface notes:
#
#   - should an empty dn be represented as None, or an empty string?
#
#   - should single-valued attributes be a string, or a list with one
#     element?
#

from ldb import *

# Global initialisation

result = ldb_global_init()

if result != 0:
    raise LdbError, (result, 'ldb_global_init failed')

# Ldb exceptions

class LdbError(Exception):
    """An exception raised when a ldb error occurs.
    The exception data is a tuple consisting of the ldb number and a
    string description of the error."""
    pass

# Ldb classes

class LdbMessage:
    """A class representing a ldb message as a Python dictionary."""
    
    def __init__(self):
        self.mem_ctx = talloc_init(None)
        self.msg = ldb_msg_new(self.mem_ctx)

    def __del__(self):
        if self.mem_ctx is not None:
            talloc_free(self.mem_ctx)
            self.mem_ctx = None
            self.msg = None

    # Make the dn attribute of the object dynamic

    def __getattr__(self, attr):
        if attr == 'dn':
            return ldb_dn_linearize(None, self.msg.dn)
        return self.__dict__[attr]

    def __setattr__(self, attr, value):
        if attr == 'dn':
            self.msg.dn = ldb_dn_explode(self.msg, value)
            if self.msg.dn == None:
                err = ldb.ERR_INVALID_DN_SYNTAX
                raise LdbError(err, ldb_strerror(err))
            return
        self.__dict__[attr] = value
        
    # Get and set individual elements

    def __getitem__(self, key):

        elt = ldb_msg_find_element(self.msg, key)

        if elt is None:
            raise KeyError, "No such attribute '%s'" % key

        return [ldb_val_array_getitem(elt.values, i)
                for i in range(elt.num_values)]

    def __setitem__(self, key, value):
        ldb_msg_remove_attr(self.msg, key)
        if type(value) in (list, tuple):
            [ldb_msg_add_value(self.msg, key, v) for v in value]
        else:
            ldb_msg_add_value(self.msg, key, value)

    # Dictionary interface
    # TODO: move to iterator based interface

    def len(self):
        return self.msg.num_elements

    def keys(self):
        return [ldb_message_element_array_getitem(self.msg.elements, i).name
                for i in range(self.msg.num_elements)]

    def values(self):
        return [self[k] for k in self.keys()]

    def items(self):
        return [(k, self[k]) for k in self.keys()]

    # Misc stuff

    def sanity_check(self):
        return ldb_msg_sanity_check(self.msg)

class Ldb:
    """A class representing a binding to a ldb file."""

    def __init__(self, url, flags = 0):
        """Initialise underlying ldb."""
    
        self.mem_ctx = talloc_init('mem_ctx for ldb 0x%x' % id(self))
        self.ldb_ctx = ldb_init(self.mem_ctx)

        result = ldb_connect(self.ldb_ctx, url, flags, None)

        if result != LDB_SUCCESS:
            raise LdbError, (result, ldb_strerror(result))
        
    def __del__(self):
        """Called when the object is to be garbage collected."""
        self.close()

    def close(self):
        """Close down a ldb."""
        if self.mem_ctx is not None:
            talloc_free(self.mem_ctx)
            self.mem_ctx = None
            self.ldb_ctx = None

    def _ldb_call(self, fn, *args):
        """Call a ldb function with args.  Raise a LdbError exception
        if the function returns a non-zero return value."""
        
        result = fn(*args)

        if result != LDB_SUCCESS:
            raise LdbError, (result, ldb_strerror(result))

    def search(self, expression):
        """Search a ldb for a given expression."""

        self._ldb_call(ldb_search, self.ldb_ctx, None, LDB_SCOPE_DEFAULT,
                       expression, None);

        return [LdbMessage(ldb_message_ptr_array_getitem(result.msgs, ndx))
                for ndx in range(result.count)]

    def delete(self, dn):
        """Delete a dn."""

        _dn = ldb_dn_explode(self.ldb_ctx, dn)

        self._ldb_call(ldb_delete, self.ldb_ctx, _dn)

    def rename(self, olddn, newdn):
        """Rename a dn."""
        
        _olddn = ldb_dn_explode(self.ldb_ctx, olddn)
        _newdn = ldb_dn_explode(self.ldb_ctx, newdn)
        
        self._ldb_call(ldb_rename, self.ldb_ctx, _olddn, _newdn)

    def add(self, m):
        self._ldb_call(ldb_add, self.ldb_ctx, m.msg)
