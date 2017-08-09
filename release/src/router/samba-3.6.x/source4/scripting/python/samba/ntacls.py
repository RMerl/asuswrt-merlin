#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Matthieu Patou <mat@matws.net> 2009-2010
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

"""NT Acls."""


import os
import samba.xattr_native, samba.xattr_tdb
from samba.dcerpc import security, xattr
from samba.ndr import ndr_pack, ndr_unpack

class XattrBackendError(Exception):
    """A generic xattr backend error."""


def checkset_backend(lp, backend, eadbfile):
    '''return the path to the eadb, or None'''
    if backend is None:
        return lp.get("posix:eadb")
    elif backend == "native":
        return None
    elif backend == "tdb":
        if eadbfile is not None:
            return eadbfile
        else:
            return os.path.abspath(os.path.join(lp.get("private dir"), "eadb.tdb"))
    else:
        raise XattrBackendError("Invalid xattr backend choice %s"%backend)


def getntacl(lp, file, backend=None, eadbfile=None):
    eadbname = checkset_backend(lp, backend, eadbfile)
    if eadbname is not None:
        try:
            attribute = samba.xattr_tdb.wrap_getxattr(eadbname, file, 
                xattr.XATTR_NTACL_NAME)
        except Exception:
            # FIXME: Don't catch all exceptions, just those related to opening 
            # xattrdb
            print "Fail to open %s" % eadbname
            attribute = samba.xattr_native.wrap_getxattr(file,
                xattr.XATTR_NTACL_NAME)
    else:
        attribute = samba.xattr_native.wrap_getxattr(file,
            xattr.XATTR_NTACL_NAME)
    ntacl = ndr_unpack(xattr.NTACL, attribute)
    return ntacl


def setntacl(lp, file, sddl, domsid, backend=None, eadbfile=None):
    eadbname = checkset_backend(lp, backend, eadbfile)
    ntacl = xattr.NTACL()
    ntacl.version = 1
    sid = security.dom_sid(domsid)
    sd = security.descriptor.from_sddl(sddl, sid)
    ntacl.info = sd
    if eadbname is not None:
        try:
            samba.xattr_tdb.wrap_setxattr(eadbname,
                file, xattr.XATTR_NTACL_NAME, ndr_pack(ntacl))
        except Exception:
            # FIXME: Don't catch all exceptions, just those related to opening 
            # xattrdb
            print "Fail to open %s" % eadbname
            samba.xattr_native.wrap_setxattr(file, xattr.XATTR_NTACL_NAME, 
                ndr_pack(ntacl))
    else:
        samba.xattr_native.wrap_setxattr(file, xattr.XATTR_NTACL_NAME,
                ndr_pack(ntacl))


def ldapmask2filemask(ldm):
    """Takes the access mask of a DS ACE and transform them in a File ACE mask.
    """
    RIGHT_DS_CREATE_CHILD     = 0x00000001
    RIGHT_DS_DELETE_CHILD     = 0x00000002
    RIGHT_DS_LIST_CONTENTS    = 0x00000004
    ACTRL_DS_SELF             = 0x00000008
    RIGHT_DS_READ_PROPERTY    = 0x00000010
    RIGHT_DS_WRITE_PROPERTY   = 0x00000020
    RIGHT_DS_DELETE_TREE      = 0x00000040
    RIGHT_DS_LIST_OBJECT      = 0x00000080
    RIGHT_DS_CONTROL_ACCESS   = 0x00000100
    FILE_READ_DATA            = 0x0001
    FILE_LIST_DIRECTORY       = 0x0001
    FILE_WRITE_DATA           = 0x0002
    FILE_ADD_FILE             = 0x0002
    FILE_APPEND_DATA          = 0x0004
    FILE_ADD_SUBDIRECTORY     = 0x0004
    FILE_CREATE_PIPE_INSTANCE = 0x0004
    FILE_READ_EA              = 0x0008
    FILE_WRITE_EA             = 0x0010
    FILE_EXECUTE              = 0x0020
    FILE_TRAVERSE             = 0x0020
    FILE_DELETE_CHILD         = 0x0040
    FILE_READ_ATTRIBUTES      = 0x0080
    FILE_WRITE_ATTRIBUTES     = 0x0100
    DELETE                    = 0x00010000
    READ_CONTROL              = 0x00020000
    WRITE_DAC                 = 0x00040000
    WRITE_OWNER               = 0x00080000
    SYNCHRONIZE               = 0x00100000
    STANDARD_RIGHTS_ALL       = 0x001F0000

    filemask = ldm & STANDARD_RIGHTS_ALL

    if (ldm & RIGHT_DS_READ_PROPERTY) and (ldm & RIGHT_DS_LIST_CONTENTS):
        filemask = filemask | (SYNCHRONIZE | FILE_LIST_DIRECTORY |\
                                FILE_READ_ATTRIBUTES | FILE_READ_EA |\
                                FILE_READ_DATA | FILE_EXECUTE)

    if ldm & RIGHT_DS_WRITE_PROPERTY:
        filemask = filemask | (SYNCHRONIZE | FILE_WRITE_DATA |\
                                FILE_APPEND_DATA | FILE_WRITE_EA |\
                                FILE_WRITE_ATTRIBUTES | FILE_ADD_FILE |\
                                FILE_ADD_SUBDIRECTORY)

    if ldm & RIGHT_DS_CREATE_CHILD:
        filemask = filemask | (FILE_ADD_SUBDIRECTORY | FILE_ADD_FILE)

    if ldm & RIGHT_DS_DELETE_CHILD:
        filemask = filemask | FILE_DELETE_CHILD

    return filemask


def dsacl2fsacl(dssddl, domsid):
    """
    
    This function takes an the SDDL representation of a DS
    ACL and return the SDDL representation of this ACL adapted
    for files. It's used for Policy object provision
    """
    sid = security.dom_sid(domsid)
    ref = security.descriptor.from_sddl(dssddl, sid)
    fdescr = security.descriptor()
    fdescr.owner_sid = ref.owner_sid
    fdescr.group_sid = ref.group_sid
    fdescr.type = ref.type
    fdescr.revision = ref.revision
    fdescr.sacl = ref.sacl
    aces = ref.dacl.aces
    for i in range(0, len(aces)):
        ace = aces[i]
        if not ace.type & security.SEC_ACE_TYPE_ACCESS_ALLOWED_OBJECT and str(ace.trustee) != security.SID_BUILTIN_PREW2K:
       #    if fdescr.type & security.SEC_DESC_DACL_AUTO_INHERITED:
            ace.flags = ace.flags | security.SEC_ACE_FLAG_OBJECT_INHERIT | security.SEC_ACE_FLAG_CONTAINER_INHERIT
            if str(ace.trustee) == security.SID_CREATOR_OWNER:
                # For Creator/Owner the IO flag is set as this ACE has only a sense for child objects
                ace.flags = ace.flags | security.SEC_ACE_FLAG_INHERIT_ONLY
            ace.access_mask =  ldapmask2filemask(ace.access_mask)
            fdescr.dacl_add(ace)

    return fdescr.as_sddl(sid)
