#!/usr/bin/python
#
#  tool to manipulate a remote registry
#  Copyright Andrew Tridgell 2005
#  Copyright Jelmer Vernooij 2007
#  Released under the GNU GPL v3 or later
#

import sys

# Find right directory when running from source tree
sys.path.insert(0, "bin/python")

from samba.dcerpc import winreg
import optparse
import samba.getopt as options

parser = optparse.OptionParser("%s <BINDING> [path]" % sys.argv[0])
sambaopts = options.SambaOptions(parser)
parser.add_option_group(sambaopts)
parser.add_option("--createkey", type="string", metavar="KEYNAME", 
        help="create a key")

opts, args = parser.parse_args()

if len(args) < 1:
    parser.print_usage()
    sys.exit(-1)

binding = args[0]

print "Connecting to " + binding
conn = winreg.winreg(binding, sambaopts.get_loadparm())

def list_values(key):
    (num_values, max_valnamelen, max_valbufsize) = conn.QueryInfoKey(key, winreg.String())[4:8]
    for i in range(num_values):
        name = winreg.StringBuf()
        name.size = max_valnamelen
        (name, type, data, _, data_len) = conn.EnumValue(key, i, name, 0, "", max_valbufsize, 0)
        print "\ttype=%-30s size=%4d  '%s'" % type, len, name
        if type in (winreg.REG_SZ, winreg.REG_EXPAND_SZ):
            print "\t\t'%s'" % data
#        if (v.type == reg.REG_MULTI_SZ) {
#            for (j in v.value) {
#                printf("\t\t'%s'\n", v.value[j])
#            }
#        }
#        if (v.type == reg.REG_DWORD || v.type == reg.REG_DWORD_BIG_ENDIAN) {
#            printf("\t\t0x%08x (%d)\n", v.value, v.value)
#        }
#        if (v.type == reg.REG_QWORD) {
#            printf("\t\t0x%llx (%lld)\n", v.value, v.value)
#        }

def list_path(key, path):
    count = 0
    (num_subkeys, max_subkeylen, max_subkeysize) = conn.QueryInfoKey(key, winreg.String())[1:4]
    for i in range(num_subkeys):
        name = winreg.StringBuf()
        name.size = max_subkeysize
        keyclass = winreg.StringBuf()
        keyclass.size = max_subkeysize
        (name, _, _) = conn.EnumKey(key, i, name, keyclass=keyclass, last_changed_time=None)[0]
        subkey = conn.OpenKey(key, name, 0, winreg.KEY_QUERY_VALUE | winreg.KEY_ENUMERATE_SUB_KEYS)
        count += list_path(subkey, "%s\\%s" % (path, name))
        list_values(subkey)
    return count

if len(args) > 1:
    root = args[1]
else:
    root = "HKLM"

if opts.createkey:
    reg.create_key("HKLM\\SOFTWARE", opt.createkey)
else:
    print "Listing registry tree '%s'" % root
    try:
        root_key = getattr(conn, "Open%s" % root)(None, winreg.KEY_QUERY_VALUE | winreg.KEY_ENUMERATE_SUB_KEYS)
    except AttributeError:
        print "Unknown root key name %s" % root
        sys.exit(1)
    count = list_path(root_key, root)
    if count == 0:
        print "No entries found"
        sys.exit(1)
