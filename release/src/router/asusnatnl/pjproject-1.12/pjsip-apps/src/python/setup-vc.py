# $Id: setup-vc.py 4121 2012-05-14 10:42:56Z bennylp $
#
# pjsua Setup script for Visual Studio
#
# Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
from distutils.core import setup, Extension
import os
import sys

# Find version
pj_version=""
pj_version_major=""
pj_version_minor=""
pj_version_rev=""
pj_version_suffix=""
f = open('../../../version.mak', 'r')
for line in f:
    if line.find("export PJ_VERSION_MAJOR") != -1:
    	tokens=line.split("=")
	if len(tokens)>1:
		pj_version_major= tokens[1].strip()
    elif line.find("export PJ_VERSION_MINOR") != -1:
    	tokens=line.split("=")
	if len(tokens)>1:
		pj_version_minor= line.split("=")[1].strip()
    elif line.find("export PJ_VERSION_REV") != -1:
    	tokens=line.split("=")
	if len(tokens)>1:
		pj_version_rev= line.split("=")[1].strip()
    elif line.find("export PJ_VERSION_SUFFIX") != -1:
    	tokens=line.split("=")
	if len(tokens)>1:
		pj_version_suffix= line.split("=")[1].strip()

f.close()
if not pj_version_major:
    print 'Unable to get PJ_VERSION_MAJOR'
    sys.exit(1)

pj_version = pj_version_major + "." + pj_version_minor
if pj_version_rev:
	pj_version += "." + pj_version_rev
if pj_version_suffix:
	pj_version += "-" + pj_version_suffix

#print 'PJ_VERSION = "'+ pj_version + '"'


# Check that extension has been built
if not os.access('../../lib/_pjsua.pyd', os.R_OK):
    print 'Error: file "../../lib/_pjsua.pyd" does not exist!'
    print ''
    print 'Please build the extension with Visual Studio first'
    print 'For more info, see http://trac.pjsip.org/repos/wiki/Python_SIP_Tutorial'
    sys.exit(1)

setup(name="pjsua",
      version=pj_version,
      description='SIP User Agent Library based on PJSIP',
      url='http://trac.pjsip.org/repos/wiki/Python_SIP_Tutorial',
      data_files=[('lib/site-packages', ['../../lib/_pjsua.pyd'])],
      py_modules=["pjsua"]
     )

