#!/bin/env python

#
# main.py - main entry for PJSIP's CDash tests
#
# Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

import sys

if len(sys.argv)==1:
    print "Usage: main.py cfg_file [cfg_site]"
    print "Example:"
    print "  main.py cfg_gnu"
    print "  main.py cfg_gnu custom_cfg_site"
    sys.exit(1)


args = []
args.extend(sys.argv)
args.remove(args[1])
args.remove(args[0])

cfg_file = __import__(sys.argv[1])
builders = cfg_file.create_builder(args)

for builder in builders:
    builder.execute()
