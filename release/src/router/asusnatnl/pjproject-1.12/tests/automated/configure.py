#!/usr/bin/python

import optparse
import os
import platform
import socket
import subprocess
import sys

PROG = "r" + "$Rev: 17 $".strip("$ ").replace("Rev: ", "")
PYTHON = os.path.basename(sys.executable)
build_type = ""
vs_target = ""
s60_target = ""
no_test = False
no_pjsua_test = False

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
# Get Visual Studio info
#
class VSVersion:
    def __init__(self):
	    self.version = "8"
	    self.release = "2005"

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
			self.version = "6"
			self.release = "98"
			break
		    elif major=="13":
			self.version = "7"
			self.release = "2003"
			break
		    elif major=="14":
			self.version = "8"
			self.release = "2005"
			break
		    elif major=="15":
			self.version = "9"
			self.release = "2008"
			break
		    elif major=="16":
			self.version = "10"
			self.release = "2010"
			break
		    else:
			self.version = "11"
			self.release = "2012"
			break
	    proc.wait()
	    self.vs_version = "vs" + self.version
	    self.vs_release = "vs" + self.release
    

#
# Get S60 SDK info
#
class S60SDK:
	def __init__(self):
		self.epocroot = ""
		self.sdk = ""
		self.device = ""

		# Check that EPOCROOT is set
		if not "EPOCROOT" in os.environ:
		    sys.stderr.write("Error: EPOCROOT environment variable is not set\n")
		    sys.exit(1)
		epocroot = os.environ["EPOCROOT"]
		# EPOCROOT must have trailing backslash
		if epocroot[-1] != "\\":
		    epocroot = epocroot + "\\"
		    os.environ["EPOCROOT"] = epocroot
		self.epocroot = epocroot
		self.sdk = sdk1 = epocroot.split("\\")[-2]
		self.device = "@" + self.sdk + ":com.nokia.s60"

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
		    sys.stderr.write("Error: default SDK in device doesn't match EPOCROOT\n")
		    sys.stderr.write("Default device SDK = '" + sdk2 + "'\n")
		    sys.stderr.write("EPOCROOT SDK = '" + sdk1 + "'\n")
		    sys.exit(1)

		self.name = sdk2.replace("_", "-")



def replace_vars(text):
	global vs_target, s60_target, build_type, no_test, no_pjsua_test
	suffix = ""

        os_info = platform.system() + platform.release() + "-" + platform.machine()

	# osinfo
	s60sdk_var = None
	if build_type == "s60":
		s60sdk_var = S60SDK()
		os_info = s60sdk_var.name
	elif platform.system().lower() == "windows" or platform.system().lower() == "microsoft":
		if platform.system().lower() == "microsoft":
			os_info = platform.release() + "-" + platform.version() + "-" + platform.win32_ver()[2]
	elif platform.system().lower() == "linux":
                os_info =  "-" + "-".join(platform.linux_distribution()[0:2])

	# vs_target
	if not vs_target and text.find("$(VSTARGET)") >= 0:
		if build_type != "vs":
			sys.stderr.write("Warning: $(VSTARGET) only valid for Visual Studio\n")
		print "Enter Visual Studio vs_target name (e.g. Release, Debug) [Release]: ",
		vs_target = sys.stdin.readline().replace("\n", "").replace("\r", "")
		if not vs_target:
			vs_target = "Release"

	# s60_target
	if not s60_target and text.find("$(S60TARGET)") >= 0:
		if build_type != "s60":
			sys.stderr.write("Warning: $(S60TARGET) only valid for S60\n")
		print "Enter S60 target name (e.g. \"gcce urel\") [gcce urel]: ",
		s60_target = sys.stdin.readline().replace("\n", "").replace("\r", "")
		if not s60_target:
			s60_target = "gcce urel"
    
	# Suffix
	if build_type == "vs":
		suffix = "i386-Win32-vc8-" + vs_target
	elif build_type == "s60":
		suffix = s60sdk_var.name + "-" + s60_target.replace(" ", "-")
	elif build_type == "gnu":
		proc = subprocess.Popen("sh config.guess", cwd="../..",
					shell=True, stdout=subprocess.PIPE)
		suffix = proc.stdout.readline().rstrip(" \r\n")
	else:
		sys.stderr.write("Error: unsupported build type '" + build_type + "'\n")
		sys.exit(1)

        while True:
                if text.find("$(PJSUA-TESTS)") >= 0:
			if no_test==False and no_pjsua_test==False:
				# Determine pjsua exe to use
				exe = "../../pjsip-apps/bin/pjsua-" + suffix
				proc = subprocess.Popen(PYTHON + " runall.py --list-xml -e " + exe, 
							cwd="../pjsua",
							shell=True, stdout=subprocess.PIPE)
				content = proc.stdout.read()
			else:
				content = ""
                        text = text.replace("$(PJSUA-TESTS)", content)
                elif text.find("$(GCC)") >= 0:
                        text = text.replace("$(GCC)", gcc_version("gcc"))
                elif text.find("$(VS)") >= 0:
			vsver = VSVersion()
                        text = text.replace("$(VS)", VSVersion().vs_release)
                elif text.find("$(VSTARGET)") >= 0:
                        text = text.replace("$(VSTARGET)", vs_target)
                elif text.find("$(S60TARGET)") >= 0:
                        text = text.replace("$(S60TARGET)", s60_target)
                elif text.find("$(S60TARGETNAME)") >= 0:
                        text = text.replace("$(S60TARGETNAME)", s60_target.replace(" ", "-"))
                elif text.find("$(S60DEVICE)") >= 0:
                        text = text.replace("$(S60DEVICE)", s60sdk_var.device)
                elif text.find("$(EPOCROOT)") >= 0:
                        text = text.replace("$(EPOCROOT)", s60sdk_var.epocroot)
                elif text.find("$(DISABLED)") >= 0:
                        text = text.replace("$(DISABLED)", "0")
                elif text.find("$(IPPROOT)") >= 0:
                        if not os.environ.has_key("IPPROOT"):
                                sys.stderr.write("Error: environment variable IPPROOT is needed but not set\n")
                                sys.exit(1)
                        text = text.replace("$(IPPROOT)", os.environ["IPPROOT"])
                elif text.find("$(IPPSAMPLES)") >= 0:
                        if not os.environ.has_key("IPPSAMPLES"):
                                sys.stderr.write("Error: environment variable IPPSAMPLES is needed but not set\n")
                                sys.exit(1)
                        text = text.replace("$(IPPSAMPLES)", os.environ["IPPSAMPLES"])
                elif text.find("$(IPPARCH)") >= 0:
                        if not os.environ.has_key("IPPARCH"):
                                text = text.replace("$(IPPARCH)", "")
                        else:
                                text = text.replace("$(IPPARCH)", os.environ["IPPARCH"])
                elif text.find("$(OS)") >= 0:
                        text = text.replace("$(OS)", os_info)
                elif text.find("$(SUFFIX)") >= 0:
			text = text.replace("$(SUFFIX)", suffix)
                elif text.find("$(HOSTNAME)") >= 0:
                        text = text.replace("$(HOSTNAME)", socket.gethostname())
                elif text.find("$(PJDIR)") >= 0:
                        wdir = os.path.join(os.getcwd(), "../..")
                        wdir = os.path.normpath(wdir)
                        text = text.replace("$(PJDIR)", wdir)
                elif text.find("$(NOP)") >= 0:
			if platform.system().lower() == "windows" or platform.system().lower() == "microsoft":
				cmd = "CMD /C echo Success"
			else:
				cmd = "echo Success"
                        text = text.replace("$(NOP)", cmd)
                elif text.find("$(NOTEST)") >= 0:
			if no_test:
				str = '"1"'
			else:
				str = '"0"'
                        text = text.replace("$(NOTEST)", str)
                else:
                        break
        return text


def main(args):
	global vs_target, s60_target, build_type, no_test, no_pjsua_test
        output = sys.stdout
        usage = """Usage: configure.py [OPTIONS] scenario_template_file

Where OPTIONS:
  -o FILE               Output to file, otherwise to stdout.
  -t TYPE		Specify build type. If not specified, it will be
			asked if necessary. Values are: 
			    vs:    Visual Studio
			    gnu:   Makefile based
			    s60:   Symbian S60
  -vstarget TARGETNAME	Specify Visual Studio target name if build type is set
			to vs. If not specified then it will be asked.
			Sample target names:
			    - Debug
			    - Release
			    - or any other target in the project file
  -s60target TARGETNAME Specify S60 target name if build type is set to s60.
                        If not specified then it will be asked. Sample target
			names:
			    - "gcce udeb"
			    - "gcce urel"
  -notest               Disable all tests in the scenario.
  -nopjsuatest          Disable pjsua tests in the scenario.
"""

        args.pop(0)
	while len(args):
                if args[0]=='-o':
                        args.pop(0)
                        if len(args):
                                output = open(args[0], "wt")
                                args.pop(0)
                        else:
                                sys.stderr.write("Error: needs value for -o\n")
                                sys.exit(1)
		elif args[0]=='-vstarget':
			args.pop(0)
			if len(args):
				vs_target = args[0]
				args.pop(0)
			else:
				sys.stderr.write("Error: needs value for -vstarget\n")
				sys.exit(1)
		elif args[0]=='-s60target':
			args.pop(0)
			if len(args):
				s60_target = args[0]
				args.pop(0)
			else:
				sys.stderr.write("Error: needs value for -s60target\n")
				sys.exit(1)
		elif args[0]=='-t':
			args.pop(0)
			if len(args):
				build_type = args[0].lower()
				args.pop(0)
			else:
				sys.stderr.write("Error: needs value for -t\n")
				sys.exit(1)
			if not ["vs", "gnu", "s60"].count(build_type):
				sys.stderr.write("Error: invalid -t argument value\n")
				sys.exit(1)
		elif args[0]=='-notest' or args[0]=='-notests':
			args.pop(0)
			no_test = True
		elif args[0]=='-nopjsuatest' or args[0]=='-nopjsuatests':
			args.pop(0)
			no_pjsua_test = True
		else:
			break

        if len(args) != 1:
                sys.stderr.write(usage + "\n")
                return 1
        
	if not build_type:
	    defval = "vs"
	    if "SHELL" in os.environ:
		shell = os.environ["SHELL"]
		if shell.find("sh") > -1:
		    defval = "gnu"
	    print "Enter the build type (values: vs, gnu, s60) [%s]: " % (defval),
	    build_type = sys.stdin.readline().replace("\n", "").replace("\r", "")
	    if not build_type:
		   build_type = defval
		

        tpl_file = args[len(args)-1]
        if not os.path.isfile(tpl_file):
                print "Error: unable to find template file '%s'" % (tpl_file)
                return 1
                
        f = open(tpl_file, "r")
        tpl = f.read()
        f.close()
        
        tpl = replace_vars(tpl)
        output.write(tpl)
        if output != sys.stdout:
                output.close()
        return 0


if __name__ == "__main__":
    rc = main(sys.argv)
    sys.exit(rc)

