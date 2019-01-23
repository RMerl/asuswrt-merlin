#!/usr/bin/env python

# Unix SMB/CIFS implementation.
# Copyright (C) Andrew Tridgell 2009
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

"""Tests the possibleInferiors generation in the schema_fsmo ldb module"""

import optparse
import sys


# Find right directory when running from source tree
sys.path.insert(0, "bin/python")

from samba import getopt as options, Ldb
import ldb

parser = optparse.OptionParser("possibleinferiors.py <URL> [<CLASS>]")
sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)
credopts = options.CredentialsOptions(parser)
parser.add_option_group(credopts)
parser.add_option_group(options.VersionOptions(parser))
parser.add_option("--wspp", action="store_true")

opts, args = parser.parse_args()

if len(args) < 1:
    parser.print_usage()
    sys.exit(1)

url = args[0]
if (len(args) > 1):
    objectclass = args[1]
else:
    objectclass = None

def uniq_list(alist):
    """return a unique list"""
    set = {}
    return [set.setdefault(e,e) for e in alist if e not in set]


lp_ctx = sambaopts.get_loadparm()

creds = credopts.get_credentials(lp_ctx)

ldb_options = []
# use 'paged_search' module when connecting remotely
if url.lower().startswith("ldap://"):
    ldb_options = ["modules:paged_searches"]

db = Ldb(url, credentials=creds, lp=lp_ctx, options=ldb_options)

# get the rootDSE
res = db.search(base="", expression="",
                scope=ldb.SCOPE_BASE,
                attrs=["schemaNamingContext"])
rootDse = res[0]

schema_base = rootDse["schemaNamingContext"][0]

def possible_inferiors_search(db, oc):
    """return the possible inferiors via a search for the possibleInferiors attribute"""
    res = db.search(base=schema_base,
                    expression=("ldapDisplayName=%s" % oc),
                    attrs=["possibleInferiors"])

    poss=[]
    if len(res) == 0 or res[0].get("possibleInferiors") is None:
        return poss
    for item in res[0]["possibleInferiors"]:
        poss.append(str(item))
    poss = uniq_list(poss)
    poss.sort()
    return poss



# see [MS-ADTS] section 3.1.1.4.5.21
# and section 3.1.1.4.2 for this algorithm

# !systemOnly=TRUE
# !objectClassCategory=2
# !objectClassCategory=3

def supclasses(classinfo, oc):
    list = []
    if oc == "top":
        return list
    if classinfo[oc].get("SUPCLASSES") is not None:
        return classinfo[oc]["SUPCLASSES"]
    res = classinfo[oc]["subClassOf"]
    for r in res:
        list.append(r)
        list.extend(supclasses(classinfo,r))
    classinfo[oc]["SUPCLASSES"] = list
    return list

def auxclasses(classinfo, oclist):
    list = []
    if oclist == []:
        return list
    for oc in oclist:
        if classinfo[oc].get("AUXCLASSES") is not None:
            list.extend(classinfo[oc]["AUXCLASSES"])
        else:
            list2 = []
            list2.extend(classinfo[oc]["systemAuxiliaryClass"])
            list2.extend(auxclasses(classinfo, classinfo[oc]["systemAuxiliaryClass"]))
            list2.extend(classinfo[oc]["auxiliaryClass"])
            list2.extend(auxclasses(classinfo, classinfo[oc]["auxiliaryClass"]))
            list2.extend(auxclasses(classinfo, supclasses(classinfo, oc)))
            classinfo[oc]["AUXCLASSES"] = list2
            list.extend(list2)
    return list

def subclasses(classinfo, oclist):
    list = []
    for oc in oclist:
        list.extend(classinfo[oc]["SUBCLASSES"])
    return list

def posssuperiors(classinfo, oclist):
    list = []
    for oc in oclist:
        if classinfo[oc].get("POSSSUPERIORS") is not None:
            list.extend(classinfo[oc]["POSSSUPERIORS"])
        else:
            list2 = []
            list2.extend(classinfo[oc]["systemPossSuperiors"])
            list2.extend(classinfo[oc]["possSuperiors"])
            list2.extend(posssuperiors(classinfo, supclasses(classinfo, oc)))
            if opts.wspp:
                # the WSPP docs suggest we should do this:
                list2.extend(posssuperiors(classinfo, auxclasses(classinfo, [oc])))
            else:
                # but testing against w2k3 and w2k8 shows that we need to do this instead
                list2.extend(subclasses(classinfo, list2))
            classinfo[oc]["POSSSUPERIORS"] = list2
            list.extend(list2)
    return list

def pull_classinfo(db):
    """At startup we build a classinfo[] dictionary that holds all the information needed to construct the possible inferiors"""
    classinfo = {}
    res = db.search(base=schema_base,
                    expression="objectclass=classSchema",
                    attrs=["ldapDisplayName", "systemOnly", "objectClassCategory",
                           "possSuperiors", "systemPossSuperiors",
                           "auxiliaryClass", "systemAuxiliaryClass", "subClassOf"])
    for r in res:
        name = str(r["ldapDisplayName"][0])
        classinfo[name] = {}
        if str(r["systemOnly"]) == "TRUE":
            classinfo[name]["systemOnly"] = True
        else:
            classinfo[name]["systemOnly"] = False
        if r.get("objectClassCategory"):
            classinfo[name]["objectClassCategory"] = int(r["objectClassCategory"][0])
        else:
            classinfo[name]["objectClassCategory"] = 0
        for a in [ "possSuperiors", "systemPossSuperiors",
                   "auxiliaryClass", "systemAuxiliaryClass",
                   "subClassOf" ]:
            classinfo[name][a] = []
            if r.get(a):
                for i in r[a]:
                    classinfo[name][a].append(str(i))

    # build a list of subclasses for each class
    def subclasses_recurse(subclasses, oc):
        list = subclasses[oc]
        for c in list:
            list.extend(subclasses_recurse(subclasses, c))
        return list

    subclasses = {}
    for oc in classinfo:
        subclasses[oc] = []
    for oc in classinfo:
        for c in classinfo[oc]["subClassOf"]:
            if not c == oc:
                subclasses[c].append(oc)
    for oc in classinfo:
        classinfo[oc]["SUBCLASSES"] = uniq_list(subclasses_recurse(subclasses, oc))

    return classinfo

def is_in_list(list, c):
    for a in list:
        if c == a:
            return True
    return False

def possible_inferiors_constructed(db, classinfo, c):
    list = []
    for oc in classinfo:
        superiors = posssuperiors(classinfo, [oc])
        if (is_in_list(superiors, c) and
            classinfo[oc]["systemOnly"] == False and
            classinfo[oc]["objectClassCategory"] != 2 and
            classinfo[oc]["objectClassCategory"] != 3):
            list.append(oc)
    list = uniq_list(list)
    list.sort()
    return list

def test_class(db, classinfo, oc):
    """test to see if one objectclass returns the correct possibleInferiors"""
    print "test: objectClass.%s" % oc
    poss1 = possible_inferiors_search(db, oc)
    poss2 = possible_inferiors_constructed(db, classinfo, oc)
    if poss1 != poss2:
        print "failure: objectClass.%s [" % oc
        print "Returned incorrect list for objectclass %s" % oc
        print "search:      %s" % poss1
        print "constructed: %s" % poss2
        for i in range(0,min(len(poss1),len(poss2))):
            print "%30s %30s" % (poss1[i], poss2[i])
        print "]"
        sys.exit(1)
    else:
        print "success: objectClass.%s" % oc

def get_object_classes(db):
    """return a list of all object classes"""
    list=[]
    for item in classinfo:
        list.append(item)
    return list

classinfo = pull_classinfo(db)

if objectclass is None:
    for oc in get_object_classes(db):
        test_class(db,classinfo,oc)
else:
    test_class(db,classinfo,objectclass)

print "Lists match OK"
