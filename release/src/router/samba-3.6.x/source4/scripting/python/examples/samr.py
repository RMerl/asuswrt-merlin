#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Unix SMB/CIFS implementation.
# Copyright © Jelmer Vernooij <jelmer@samba.org> 2008
#
# Based on samr.js © Andrew Tridgell <tridge@samba.org>
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

import sys

sys.path.insert(0, "bin/python")

from samba.dcerpc import samr, security

def display_lsa_string(str):
    return str.string

def FillUserInfo(samr, dom_handle, users, level):
    """fill a user array with user information from samrQueryUserInfo"""
    for i in range(len(users)):
        user_handle = samr.OpenUser(handle, security.SEC_FLAG_MAXIMUM_ALLOWED, users[i].idx)
        info = samr.QueryUserInfo(user_handle, level)
        info.name = users[i].name
        info.idx  = users[i].idx
        users[i] = info
        samr.Close(user_handle)

def toArray((handle, array, num_entries)):
    ret = []
    for x in range(num_entries):
        ret.append((array.entries[x].idx, array.entries[x].name))
    return ret
    

def test_Connect(samr):
    """test the samr_Connect interface"""
    print "Testing samr_Connect"
    return samr.Connect2(None, security.SEC_FLAG_MAXIMUM_ALLOWED)

def test_LookupDomain(samr, handle, domain):
    """test the samr_LookupDomain interface"""
    print "Testing samr_LookupDomain"
    return samr.LookupDomain(handle, domain)

def test_OpenDomain(samr, handle, sid):
    """test the samr_OpenDomain interface"""
    print "Testing samr_OpenDomain"
    return samr.OpenDomain(handle, security.SEC_FLAG_MAXIMUM_ALLOWED, sid)

def test_EnumDomainUsers(samr, dom_handle):
    """test the samr_EnumDomainUsers interface"""
    print "Testing samr_EnumDomainUsers"
    users = toArray(samr.EnumDomainUsers(dom_handle, 0, 0, -1))
    print "Found %d users" % len(users)
    for idx, user in users:
        print "\t%s\t(%d)" % (user.string, idx)

def test_EnumDomainGroups(samr, dom_handle):
    """test the samr_EnumDomainGroups interface"""
    print "Testing samr_EnumDomainGroups"
    groups = toArray(samr.EnumDomainGroups(dom_handle, 0, 0))
    print "Found %d groups" % len(groups)
    for idx, group in groups:
        print "\t%s\t(%d)" % (group.string, idx)

def test_domain_ops(samr, dom_handle):
    """test domain specific ops"""
    test_EnumDomainUsers(samr, dom_handle)
    test_EnumDomainGroups(samr, dom_handle)

def test_EnumDomains(samr, handle):
    """test the samr_EnumDomains interface"""
    print "Testing samr_EnumDomains"

    domains = toArray(samr.EnumDomains(handle, 0, -1))
    print "Found %d domains" % len(domains)
    for idx, domain in domains:
        print "\t%s (%d)" % (display_lsa_string(domain), idx)
    for idx, domain in domains:
        print "Testing domain %s" % display_lsa_string(domain)
        sid = samr.LookupDomain(handle, domain)
        dom_handle = test_OpenDomain(samr, handle, sid)
        test_domain_ops(samr, dom_handle)
        samr.Close(dom_handle)

if len(sys.argv) != 2:
   print "Usage: samr.js <BINDING>"
   sys.exit(1)

binding = sys.argv[1]

print "Connecting to %s" % binding
try:
    samr = samr.samr(binding)
except Exception, e:
    print "Failed to connect to %s: %s" % (binding, e.message)
    sys.exit(1)

handle = test_Connect(samr)
test_EnumDomains(samr, handle)
samr.Close(handle)

print "All OK"
