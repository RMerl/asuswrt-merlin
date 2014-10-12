#!/usr/bin/env python
#
# Create DisplaySpecifiers LDIF (as a string) from the documents provided by
# Microsoft under the WSPP.
#
# Copyright (C) Andrew Kroeger <andrew@id10ts.net> 2009
#
# Based on ms_schema.py
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

import re

def __read_folded_line(f, buffer):
    """Read a line from an LDIF file, unfolding it"""
    line = buffer

    while True:
        l = f.readline()

        if l[:1] == " ":
            # continued line

            # cannot fold an empty line
            assert(line != "" and line != "\n")

            # preserves '\n '
            line = line + l
        else:
            # non-continued line
            if line == "":
                line = l

                if l == "":
                    # eof, definitely won't be folded
                    break
            else:
                # marks end of a folded line
                # line contains the now unfolded line
                # buffer contains the start of the next possibly folded line
                buffer = l
                break

    return (line, buffer)

# Only compile regexp once.
# Will not match options after the attribute type.
attr_type_re = re.compile("^([A-Za-z][A-Za-z0-9-]*):")

def __read_raw_entries(f):
    """Read an LDIF entry, only unfolding lines"""

    buffer = ""

    while True:
        entry = []

        while True:
            (l, buffer) = __read_folded_line(f, buffer)

            if l[:1] == "#":
                continue

            if l == "\n" or l == "":
                break

            m = attr_type_re.match(l)

            if m:
                if l[-1:] == "\n":
                    l = l[:-1]

                entry.append(l)
            else:
                print >>sys.stderr, "Invalid line: %s" % l,
                sys.exit(1)

        if len(entry):
            yield entry

        if l == "":
            break

def fix_dn(dn):
    """Fix a string DN to use ${CONFIGDN}"""

    if dn.find("<Configuration NC Distinguished Name>") != -1:
        dn = dn.replace("\n ", "")
        return dn.replace("<Configuration NC Distinguished Name>", "${CONFIGDN}")
    else:
        return dn

def __write_ldif_one(entry):
    """Write out entry as LDIF"""
    out = []

    for l in entry:
        if l[2] == 0:
            out.append("%s: %s" % (l[0], l[1]))
        else:
            # This is a base64-encoded value
            out.append("%s:: %s" % (l[0], l[1]))

    return "\n".join(out)

def __transform_entry(entry):
    """Perform required transformations to the Microsoft-provided LDIF"""

    temp_entry = []

    for l in entry:
        t = []

        if l.find("::") != -1:
            # This is a base64-encoded value
            t = l.split(":: ", 1)
            t.append(1)
        else:
            t = l.split(": ", 1)
            t.append(0)

        key = t[0].lower()

        if key == "changetype":
            continue

        if key == "distinguishedname":
            continue

        if key == "instancetype":
            continue

        if key == "name":
            continue

        if key == "cn":
            continue

        if key == "objectcategory":
            continue

        if key == "showinadvancedviewonly":
            value = t[1].upper().lstrip().rstrip()
            if value == "TRUE":
                # Remove showInAdvancedViewOnly attribute if it is set to the
                # default value of TRUE
                continue

        t[1] = fix_dn(t[1])

        temp_entry.append(t)

    entry = temp_entry

    return entry

def read_ms_ldif(filename):
    """Read and transform Microsoft-provided LDIF file."""

    out = []

    f = open(filename, "rU")
    for entry in __read_raw_entries(f):
        out.append(__write_ldif_one(__transform_entry(entry)))

    return "\n\n".join(out) + "\n\n"

if __name__ == '__main__':
    import sys

    try:
        display_specifiers_file = sys.argv[1]
    except IndexError:
        print >>sys.stderr, "Usage: %s display-specifiers-ldif-file.txt" % (sys.argv[0])
        sys.exit(1)

    print read_ms_ldif(display_specifiers_file)

