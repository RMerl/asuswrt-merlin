# $Id: run.py 3475 2011-03-23 03:48:13Z bennylp $
import sys
import imp
import re
import os
import subprocess
import random
import time
import getopt

import inc_const as const
import inc_cfg as inc

# Vars
G_EXE = ""		# pjsua executable path
G_INUNIX = False	# flags that test is running in Unix


# Usage string
usage = \
"""
run.py - Automated test driver

Usage:
	run.py [options] MODULE CONFIG
Options:
	--exe, -e		pjsua executable path
	--null-audio, -n	use null audio
Sample:
	run.py -n mod_run.py scripts-run/100_simple.py
"""

# Parse arguments
try:
    opts, args = getopt.getopt(sys.argv[1:], "hne:", ["help", "null-audio", "exe="])
except getopt.GetoptError, err:
    print str(err)
    print usage
    sys.exit(2)
for o, a in opts:
    if o in ("-h", "--help"):
	print usage
	sys.exit()
    elif o in ("-n", "--null-audio"):
        inc.HAS_SND_DEV = 0
    elif o in ("-e", "--exe"):
        G_EXE = a
    else:
        print "Unknown options"
	sys.exit(2)

if len(args) != 2:
	print "Invalid arguments"
	print usage
	sys.exit(2)

# Set global ARGS to be used by modules
inc.ARGS = args

# Get the pjsua executable name
if G_EXE == "":
	if sys.platform.find("win32")!=-1:
	    EXE_DIR = "../../pjsip-apps/bin/"
	    EXECUTABLES = [ "pjsua_vc6d.exe",
			    "pjsua_vc6.exe",
			    "pjsua-i386-Win32-vc8-Debug.exe",
			    "pjsua-i386-Win32-vc8-Debug-Dynamic.exe",
			    "pjsua-i386-Win32-vc8-Debug-Static.exe",
			    "pjsua-i386-Win32-vc8-Release.exe",
			    "pjsua-i386-Win32-vc8-Release-Dynamic.exe",
			    "pjsua-i386-Win32-vc8-Release-Static.exe"
			    ]
	    e_ts = 0
	    for e in EXECUTABLES:
		e = EXE_DIR + e
		if os.access(e, os.F_OK):
		    st = os.stat(e)
		    if e_ts==0 or e_ts<st.st_mtime:
			G_EXE = e
			e_ts = st.st_mtime

	    if G_EXE=="":
		print "Unable to find valid pjsua. Please build pjsip first"
		sys.exit(1)
		
	    G_INUNIX = False
	else:
	    f = open("../../build.mak", "r")
	    while True:
		line = f.readline()
		if not line:
		    break
		if line.find("TARGET_NAME")!=-1:
		    print line
		    G_EXE="../../pjsip-apps/bin/pjsua-" + line.split(":= ")[1]
		    break
	    if G_EXE=="":
		print "Unable to find ../../../build.mak. Please build pjsip first"
		sys.exit(1)
	    G_INUNIX = True
else:
	if sys.platform.lower().find("win32")!=-1 or sys.platform.lower().find("microsoft")!=-1:
		G_INUNIX = False
	else:
		G_INUNIX = True


G_EXE = G_EXE.rstrip("\n\r \t")

###################################
# Poor man's 'expect'-like class
class Expect:
	proc = None
	echo = False
	trace_enabled = False
	name = ""
	inst_param = None
	rh = re.compile(const.DESTROYED)
	ra = re.compile(const.ASSERT, re.I)
	rr = re.compile(const.STDOUT_REFRESH)
	t0 = time.time()
	def __init__(self, inst_param):
		self.inst_param = inst_param
		self.name = inst_param.name
		self.echo = inst_param.echo_enabled
		self.trace_enabled = inst_param.trace_enabled
		fullcmd = G_EXE + " " + inst_param.arg + " --stdout-refresh=5 --stdout-refresh-text=" + const.STDOUT_REFRESH
		if not inst_param.enable_buffer:
			fullcmd = fullcmd + " --stdout-no-buf"
		self.trace("Popen " + fullcmd)
		self.proc = subprocess.Popen(fullcmd, shell=G_INUNIX, bufsize=0, stdin=subprocess.PIPE, stdout=subprocess.PIPE, universal_newlines=False)
	def send(self, cmd):
		self.trace("send " + cmd)
		self.proc.stdin.writelines(cmd + "\n")
		self.proc.stdin.flush()
	def expect(self, pattern, raise_on_error=True, title=""):
		self.trace("expect " + pattern)
		r = re.compile(pattern, re.I)
		refresh_cnt = 0
		while True:
			line = self.proc.stdout.readline()
		  	if line == "":
				raise inc.TestError(self.name + ": Premature EOF")
			# Print the line if echo is ON
			if self.echo:
				print self.name + ": " + line,
			# Trap assertion error
			if self.ra.search(line) != None:
				if raise_on_error:
					raise inc.TestError(self.name + ": " + line)
				else:
					return None
			# Count stdout refresh text. 
			if self.rr.search(line) != None:
				refresh_cnt = refresh_cnt+1
				if refresh_cnt >= 6:
					self.trace("Timed-out!")
					if raise_on_error:
						raise inc.TestError(self.name + " " + title + ": Timeout expecting pattern: \"" + pattern + "\"")
					else:
						return None		# timeout
			# Search for expected text
			if r.search(line) != None:
				return line

	def sync_stdout(self):
		self.trace("sync_stdout")
		cmd = "echo 1" + str(random.randint(1000,9999))
		self.send(cmd)
		self.expect(cmd)

	def wait(self):
		self.trace("wait")
		self.proc.communicate()

	def trace(self, s):
		if self.trace_enabled:
			now = time.time()
			fmt = self.name + ": " + "================== " + s + " ==================" + " [at t=%(time)03d]"
			print fmt % {'time':int(now - self.t0)}

#########################
# Error handling
def handle_error(errmsg, t, close_processes = True):
	print "====== Caught error: " + errmsg + " ======"
	if (close_processes):
		time.sleep(1)
		for p in t.process:
			# Protect against 'Broken pipe' exception
			try:
				p.send("q")
				p.send("q")
			except:
				pass
			is_err = False
			try:
				ret = p.expect(const.DESTROYED, False)
				if not ret:
					is_err = True
			except:
				is_err = True
			if is_err:
				if sys.hexversion >= 0x02060000:
					p.proc.terminate()
				else:
					p.wait()
			else:
				p.wait()
	print "Test completed with error: " + errmsg
	sys.exit(1)


#########################
# MAIN	

# Import the test script
script = imp.load_source("script", inc.ARGS[0])  

# Init random seed
random.seed()

# Validate
if script.test == None:
	print "Error: no test defined"
	sys.exit(1)

if script.test.skip:
	print "Test " + script.test.title + " is skipped"
	sys.exit(0)

if len(script.test.inst_params) == 0:
	print "Error: test doesn't contain pjsua run descriptions"
	sys.exit(1)

# Instantiate pjsuas
print "====== Running " + script.test.title + " ======"
print "Using " + G_EXE + " as pjsua executable"

for inst_param in script.test.inst_params:
	try:
		# Create pjsua's Expect instance from the param
		p = Expect(inst_param)
		# Wait until registration completes
		if inst_param.have_reg:
			p.expect(inst_param.uri+".*registration success")
	 	# Synchronize stdout
		p.send("")
		p.expect(const.PROMPT)
		p.send("echo 1")
		p.send("echo 1")
		p.expect("echo 1")
		# add running instance
		script.test.process.append(p)

	except inc.TestError, e:
		handle_error(e.desc, script.test)

# Run the test function
if script.test.test_func != None:
	try:
		script.test.test_func(script.test)
	except inc.TestError, e:
		handle_error(e.desc, script.test)

# Shutdown all instances
time.sleep(2)
for p in script.test.process:
	# Unregister if we have_reg to make sure that next tests
	# won't wail
	if p.inst_param.have_reg:
		p.send("ru")
		p.expect(p.inst_param.uri+".*unregistration success")
	p.send("q")
	p.send("q")
	time.sleep(0.5)
	p.expect(const.DESTROYED, False)
	p.wait()

# Run the post test function
if script.test.post_func != None:
	try:
		script.test.post_func(script.test)
	except inc.TestError, e:
		handle_error(e.desc, script.test, False)

# Done
print "Test " + script.test.title + " completed successfully"
sys.exit(0)

