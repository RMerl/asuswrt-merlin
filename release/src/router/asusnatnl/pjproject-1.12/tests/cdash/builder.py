#
# builder.py - PJSIP test scenarios builder
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

import ccdash
import os
import platform
import re
import subprocess
import sys
import time

class Operation:
    """\
    The Operation class describes the individual ccdash operation to be 
    performed.

    """
    # Types:
    UPDATE = "update"           # Update operation
    CONFIGURE = "configure"     # Configure operation
    BUILD = "build"             # Build operation
    TEST = "test"               # Unit test operation

    def __init__(self, type, cmdline, name="", wdir=""):
        self.type = type
        self.cmdline = cmdline
        self.name = name
        self.wdir = wdir
        if self.type==self.TEST and not self.name:
            raise "name required for tests"

    def encode(self, base_dir):
        s = [self.type]
        if self.type == self.TEST:
            s.append(self.name)
        if self.type != self.UPDATE:
            s.append(self.cmdline)
        s.append("-w")
        if self.wdir:
            s.append(base_dir + "/" + self.wdir)
        else:
            s.append(base_dir)
        return s


#
# Update operation
#
update_ops = [Operation(Operation.UPDATE, "")]

#
# The standard library tests (e.g. pjlib-test, pjsip-test, etc.)
#
std_test_ops= [
    Operation(Operation.TEST, "./pjlib-test$SUFFIX", name="pjlib test",
              wdir="pjlib/bin"),
    Operation(Operation.TEST, "./pjlib-util-test$SUFFIX", 
              name="pjlib-util test", wdir="pjlib-util/bin"),
    Operation(Operation.TEST, "./pjnath-test$SUFFIX", name="pjnath test",
              wdir="pjnath/bin"),
    Operation(Operation.TEST, "./pjmedia-test$SUFFIX", name="pjmedia test",
              wdir="pjmedia/bin"),
    Operation(Operation.TEST, "./pjsip-test$SUFFIX", name="pjsip test",
              wdir="pjsip/bin")
]

#
# These are pjsua Python based unit test operations
#
def build_pjsua_test_ops(pjsua_exe=""):
    ops = []
    if pjsua_exe:
        exe = " -e ../../pjsip-apps/bin/" + pjsua_exe
    else:
        exe = ""
    cwd = os.getcwd()
    os.chdir("../pjsua")
    os.system("python runall.py --list > list")
    f = open("list", "r")
    for e in f:
        e = e.rstrip("\r\n ")
        (mod,param) = e.split(None,2)
        name = mod[4:mod.find(".py")] + "_" + \
               param[param.find("/")+1:param.find(".py")]
        ops.append(Operation(Operation.TEST, "python run.py" + exe + " " + \
                             e, name=name, wdir="tests/pjsua"))
    f.close()
    os.remove("list") 
    os.chdir(cwd)
    return ops

#
# Get gcc version
#
def gcc_version(gcc):
    proc = subprocess.Popen(gcc + " -v", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, shell=True)
    ver = ""
    while True:
        s = proc.stdout.readline()
        if not s:
            break
        if s.find("gcc version") >= 0:
            ver = s.split(None, 3)[2]
            break
    proc.wait()
    return "gcc-" + ver

#
# Get Visual Studio version
#
def vs_get_version():
    proc = subprocess.Popen("cl", stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    while True:
        s = proc.stdout.readline()
        if s=="":
            break
        pos = s.find("Version")
        if pos > 0:
            proc.wait()
            s = s[pos+8:]
            ver = s.split(None, 1)[0]
            major = ver[0:2]
            if major=="12":
                return "vs6"
            elif major=="13":
                return "vs2003"
            elif major=="14":
                return "vs2005"
            elif major=="15":
                return "vs2008"
            else:
                return "vs-" + major
    proc.wait()
    return "vs-unknown"
    

#
# Test config
#
class BaseConfig:
    def __init__(self, base_dir, url, site, group, options=None):
        self.base_dir = base_dir
        self.url = url
        self.site = site
        self.group = group
        self.options = options

#
# Base class for test configurator
#
class TestBuilder:
    def __init__(self, config, build_config_name="",
                 user_mak="", config_site="", exclude=[], not_exclude=[]):
        self.config = config                        # BaseConfig instance
        self.build_config_name = build_config_name  # Optional build suffix
        self.user_mak = user_mak                    # To be put in user.mak
        self.config_site = config_site              # To be put in config_s..
        self.saved_user_mak = ""                    # To restore user.mak
        self.saved_config_site = ""                 # To restore config_s..
        self.exclude = exclude                      # List of exclude pattern
        self.not_exclude = not_exclude              # List of include pattern
        self.ccdash_args = []                       # ccdash cmd line

    def stamp(self):
        return time.strftime("%Y%m%d-%H%M", time.localtime())

    def pre_action(self):
        # Override user.mak
        name = self.config.base_dir + "/user.mak"
        if os.access(name, os.F_OK):
            f = open(name, "r")
            self.saved_user_mak = f.read()
            f.close()
        if True:
            f = open(name, "w")
            f.write(self.user_mak)
            f.close()
        # Override config_site.h
        name = self.config.base_dir + "/pjlib/include/pj/config_site.h"
        if os.access(name, os.F_OK):
            f = open(name, "r")
            self.saved_config_site= f.read()
            f.close()
        if True:
            f = open(name, "wt")
            f.write(self.config_site)
            f.close()


    def post_action(self):
        # Restore user.mak
        name = self.config.base_dir + "/user.mak"
        f = open(name, "wt")
        f.write(self.saved_user_mak)
        f.close()
        # Restore config_site.h
        name = self.config.base_dir + "/pjlib/include/pj/config_site.h"
        f = open(name, "wt")
        f.write(self.saved_config_site)
        f.close()

    def build_tests(self):
        # This should be overridden by subclasses
        pass

    def execute(self):
        if len(self.ccdash_args)==0:
            self.build_tests()
        self.pre_action()
	mandatory_op = ["update", "configure", "build"]
        counter = 0
        for a in self.ccdash_args:
            # Check if this test is in exclusion list
            fullcmd = " ".join(a)
            excluded = False
            included = False
            for pat in self.exclude:
                if pat and re.search(pat, fullcmd) != None:
                    excluded = True
                    break
            if excluded:
                for pat in self.not_exclude:
                    if pat and re.search(pat, fullcmd) != None:
                        included = True
                        break
            if excluded and not included:
                if len(fullcmd)>60:
                    fullcmd = fullcmd[0:60] + ".."
                print "Skipping '%s'" % (fullcmd)
                continue

            b = ["ccdash.py"]
            b.extend(a)
            a = b
            #print a
            try:
                rc = ccdash.main(a)
            except Exception, e:
                errmsg = str(e)
                print "**** Error: ccdash got exception %s ****" % errmsg
                rc = -1
            except:
                print "**** Error: ccdash got unknown exception ****"
                rc = -1
                
	    if rc!=0 and a[1] in mandatory_op:
		print "Stopping because of error.."
		break
            counter = counter + 1
        self.post_action()


#
# GNU test configurator
#
class GNUTestBuilder(TestBuilder):
    """\
    This class creates list of tests suitable for GNU targets.

    """
    def __init__(self, config, build_config_name="", user_mak="", \
                 config_site="", cross_compile="", exclude=[], not_exclude=[]):
        """\
        Parameters:
        config              - BaseConfig instance
        build_config_name   - Optional name to be added as suffix to the build
                              name. Sample: "min-size", "O4", "TLS", etc.
        user_mak            - Contents to be put on user.mak
        config_site         - Contents to be put on config_site.h
        cross_compile       - Optional cross-compile prefix. Must include the
                              trailing dash, e.g. "arm-unknown-linux-"
        exclude             - List of regular expression patterns for tests
                              that will be excluded from the run
        not_exclude         - List of regular expression patterns for tests
                              that will be run regardless of whether they
                              match the excluded pattern.

        """
        TestBuilder.__init__(self, config, build_config_name=build_config_name,
                             user_mak=user_mak, config_site=config_site,
                             exclude=exclude, not_exclude=not_exclude)
        self.cross_compile = cross_compile
        if self.cross_compile and self.cross_compile[-1] != '-':
            self.cross_compile.append("-")

    def build_tests(self):
        if self.cross_compile:
            suffix = "-" + self.cross_compile[0:-1]
            build_name =  self.cross_compile + \
                          gcc_version(self.cross_compile + "gcc")
        else:
            proc = subprocess.Popen("sh "+self.config.base_dir+"/config.guess",
                                    shell=True, stdout=subprocess.PIPE)
            plat = proc.stdout.readline().rstrip(" \r\n")
            build_name =  plat + "-"+gcc_version(self.cross_compile + "gcc")
            suffix = "-" + plat

        if self.build_config_name:
            build_name = build_name + "-" + self.build_config_name
        cmds = []
        cmds.extend(update_ops)
	cmds.append(Operation(Operation.CONFIGURE, "sh ./configure"))
	if sys.platform=="win32":
	    # Don't build python module on Mingw
	    cmds.append(Operation(Operation.BUILD, 
			    "sh -c 'make distclean && make dep && make'"))
	else:
	    cmds.append(Operation(Operation.BUILD, 
			    "sh -c 'make distclean && make dep && make" + \
			    " && cd pjsip-apps/src/python && " + \
			    "python setup.py clean build'"))

        cmds.extend(std_test_ops)
        cmds.extend(build_pjsua_test_ops())
        self.ccdash_args = []
        for c in cmds:
            c.cmdline = c.cmdline.replace("$SUFFIX", suffix)
            args = c.encode(self.config.base_dir)
            args.extend(["-U", self.config.url, 
                         "-S", self.config.site, 
                         "-T", self.stamp(), 
                         "-B", build_name, 
                         "-G", self.config.group])
            args.extend(self.config.options)
            self.ccdash_args.append(args)

#
# MSVC test configurator
#
class MSVCTestBuilder(TestBuilder):
    """\
    This class creates list of tests suitable for Visual Studio builds. 
    You need to set the MSVC environment variables (typically by calling
    vcvars32.bat) prior to running this class.
    
    """
    def __init__(self, config, target="Release|Win32", build_config_name="", 
                 config_site="", exclude=[], not_exclude=[]):
        """\
        Parameters:
        config              - BaseConfig instance
        target              - Visual Studio build configuration to build.
                              Sample: "Debug|Win32", "Release|Win32".
        build_config_name   - Optional name to be added as suffix to the build
                              name. Sample: "Debug", "Release", "IPv6", etc.
        config_site         - Contents to be put on config_site.h
        exclude             - List of regular expression patterns for tests
                              that will be excluded from the run
        not_exclude         - List of regular expression patterns for tests
                              that will be run regardless of whether they
                              match the excluded pattern.

        """
        TestBuilder.__init__(self, config, build_config_name=build_config_name,
                             config_site=config_site, exclude=exclude, 
                             not_exclude=not_exclude)
        self.target = target.lower()

    def build_tests(self):
       
        (vsbuild,sys) = self.target.split("|",2)
        
        build_name = sys + "-" + vs_get_version() + "-" + vsbuild

        if self.build_config_name:
            build_name = build_name + "-" + self.build_config_name

        vccmd = "vcbuild.exe /nologo /nohtmllog /nocolor /rebuild " + \
                "pjproject-vs8.sln " + " \"" + self.target + "\""
        
        suffix = "-i386-win32-vc8-" + vsbuild
        pjsua = "pjsua_vc8"
        if vsbuild=="debug":
            pjsua = pjsua + "d"
        
        cmds = []
        cmds.extend(update_ops)
        cmds.append(Operation(Operation.CONFIGURE, "CMD /C echo Nothing to do"))
        cmds.append(Operation(Operation.BUILD, vccmd))
        cmds.extend(std_test_ops)
        cmds.extend(build_pjsua_test_ops(pjsua))

        self.ccdash_args = []
        for c in cmds:
            c.cmdline = c.cmdline.replace("$SUFFIX", suffix)
            args = c.encode(self.config.base_dir)
            args.extend(["-U", self.config.url, 
                         "-S", self.config.site, 
                         "-T", self.stamp(), 
                         "-B", build_name, 
                         "-G", self.config.group])
            args.extend(self.config.options)
            self.ccdash_args.append(args)


#
# Symbian test configurator
#
class SymbianTestBuilder(TestBuilder):
    """\
    This class creates list of tests suitable for Symbian builds. You need to
    set the command line build settings prior to running this class (typically
    that involves setting the EPOCROOT variable and current device).
    
    """
    def __init__(self, config, target="gcce urel", build_config_name="", 
                 config_site="", exclude=[], not_exclude=[]):
        """\
        Parameters:
        config              - BaseConfig instance
        target              - Symbian target to build. Default is "gcce urel".
        build_config_name   - Optional name to be added as suffix to the build
                              name. Sample: "APS", "VAS", etc.
        config_site         - Contents to be put on config_site.h
        exclude             - List of regular expression patterns for tests
                              that will be excluded from the run
        not_exclude         - List of regular expression patterns for tests
                              that will be run regardless of whether they
                              match the excluded pattern.

        """
        TestBuilder.__init__(self, config, build_config_name=build_config_name,
                             config_site=config_site, exclude=exclude, 
                             not_exclude=not_exclude)
        self.target = target.lower()
        
    def build_tests(self):
       
        # Check that EPOCROOT is set
        if not "EPOCROOT" in os.environ:
            print "Error: EPOCROOT environment variable is not set"
            sys.exit(1)
        epocroot = os.environ["EPOCROOT"]
        # EPOCROOT must have trailing backslash
        if epocroot[-1] != "\\":
            epocroot = epocroot + "\\"
            os.environ["EPOCROOT"] = epocroot
        sdk1 = epocroot.split("\\")[-2]

        # Check that correct device is set
        proc = subprocess.Popen("devices", stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, shell=True)
        sdk2 = ""
        while True:
            line = proc.stdout.readline()
            if line.find("- default") > 0:
                sdk2 = line.split(":",1)[0]
                break
        proc.wait()

        if sdk1 != sdk2:
            print "Error: default SDK in device doesn't match EPOCROOT"
            print "Default device SDK =", sdk2
            print "EPOCROOT SDK =", sdk1
            sys.exit(1)

        build_name = sdk2.replace("_", "-") + "-" + \
                     self.target.replace(" ", "-")

        if self.build_config_name:
            build_name = build_name + "-" + self.build_config_name

        cmdline = "cmd /C \"cd build.symbian && bldmake bldfiles && abld build %s\"" % (self.target)
        
        cmds = []
        cmds.extend(update_ops)
        cmds.append(Operation(Operation.CONFIGURE, "CMD /C echo Nothing to do"))
        cmds.extend([Operation(Operation.BUILD, cmdline)])

        self.ccdash_args = []
        suffix = ""
        for c in cmds:
            c.cmdline = c.cmdline.replace("$SUFFIX", suffix)
            args = c.encode(self.config.base_dir)
            args.extend(["-U", self.config.url, 
                         "-S", self.config.site, 
                         "-T", self.stamp(), 
                         "-B", build_name, 
                         "-G", self.config.group])
            args.extend(self.config.options)
            self.ccdash_args.append(args)

