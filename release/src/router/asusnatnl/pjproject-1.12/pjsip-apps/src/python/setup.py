# $Id: setup.py 4387 2013-02-27 10:16:08Z ming $
#
# pjsua Setup script.
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
import platform

# find pjsip version
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


# Fill in pj_inc_dirs
pj_inc_dirs = []
f = os.popen("make -f helper.mak inc_dir")
for line in f:
    pj_inc_dirs.append(line.rstrip("\r\n"))
f.close()

# Fill in pj_lib_dirs
pj_lib_dirs = []
f = os.popen("make -f helper.mak lib_dir")
for line in f:
    pj_lib_dirs.append(line.rstrip("\r\n"))
f.close()

# Fill in pj_libs
pj_libs = []
f = os.popen("make -f helper.mak libs")
for line in f:
    pj_libs.append(line.rstrip("\r\n"))
f.close()

# Mac OS X depedencies
if platform.system() == 'Darwin':
    extra_link_args = ["-framework", "CoreFoundation", 
                       "-framework", "AudioToolbox"]
    version = platform.mac_ver()[0].split(".")    
    # OS X Lion (10.7.x) or above support
    if version[0] == '10' and int(version[1]) >= 7:
        extra_link_args += ["-framework", "AudioUnit"]
else:
    extra_link_args = []


setup(name="pjsua", 
      version=pj_version,
      description='SIP User Agent Library based on PJSIP',
      url='http://trac.pjsip.org/repos/wiki/Python_SIP_Tutorial',
      ext_modules = [Extension("_pjsua", 
                               ["_pjsua.c"], 
                               define_macros=[('PJ_AUTOCONF', '1'),],
                               include_dirs=pj_inc_dirs, 
                               library_dirs=pj_lib_dirs, 
                               libraries=pj_libs,
                               extra_link_args=extra_link_args
                              )
                    ],
      py_modules=["pjsua"]
     )

