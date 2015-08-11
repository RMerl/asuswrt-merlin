# $Id: runall.py 3122 2010-03-27 02:35:06Z bennylp $
import os
import sys
import time
import re
import shutil

PYTHON = os.path.basename(sys.executable)

# Usage:
#  runall.py [test-to-resume]


# Initialize test list
tests = []

# Excluded tests (because they fail?)
excluded_tests = [ "svn",
		   "pyc",
		   "scripts-call/150_srtp_2_1",				# SRTP optional 'cannot' call SRTP mandatory
		   "scripts-call/301_ice_public_a.py",			# Unreliable, proxy returns 408 sometimes
		   "scripts-call/301_ice_public_b.py",			# Doesn't work because OpenSER modifies SDP
		   "scripts-pres/200_publish.py",			# Ok from cmdline, error from runall.py
		   "scripts-media-playrec/100_resample_lf_8_11.py",	# related to clock-rate 11 kHz problem
		   "scripts-media-playrec/100_resample_lf_8_22.py",	# related to clock-rate 22 kHz problem
		   "scripts-media-playrec/100_resample_lf_11"		# related to clock-rate 11 kHz problem
                   ]

# Add basic tests
for f in os.listdir("scripts-run"):
    tests.append("mod_run.py scripts-run/" + f)

# Add basic call tests
for f in os.listdir("scripts-call"):
    tests.append("mod_call.py scripts-call/" + f)

# Add presence tests
for f in os.listdir("scripts-pres"):
    tests.append("mod_pres.py scripts-pres/" + f)

# Add mod_sendto tests
for f in os.listdir("scripts-sendto"):
    tests.append("mod_sendto.py scripts-sendto/" + f)

# Add mod_media_playrec tests
for f in os.listdir("scripts-media-playrec"):
    tests.append("mod_media_playrec.py scripts-media-playrec/" + f)

# Add mod_pesq tests
for f in os.listdir("scripts-pesq"):
    tests.append("mod_pesq.py scripts-pesq/" + f)

# Add recvfrom tests
for f in os.listdir("scripts-recvfrom"):
    tests.append("mod_recvfrom.py scripts-recvfrom/" + f)

# Filter-out excluded tests
for pat in excluded_tests:
    tests = [t for t in tests if t.find(pat)==-1]


resume_script=""
shell_cmd=""

# Parse arguments
sys.argv.pop(0)
while len(sys.argv):
        if sys.argv[0]=='/h' or sys.argv[0]=='-h' or sys.argv[0]=='--help' or sys.argv[0]=='/help':
                sys.argv.pop(0)
                print "Usage:"
                print "  runall.py [OPTIONS] [run.py-OPTIONS]"
                print "OPTIONS:"
                print " --list"
                print "     List the tests"
                print "  --list-xml"
                print "     List the tests as XML format suitable for ccdash"
                print "  --resume,-r RESUME"
                print "      RESUME is string/substring to specify where to resume tests."
                print "      If this argument is omited, tests will start from the beginning."
                print "  --shell,-s SHELL"
                print "      Run the tests with the specified SHELL cmd. This can also be"
                print "      used to run the test with ccdash. Example:"
                print "        --shell '/bin/sh -c'"
                print "  run.py-OPTIONS are applicable here"
                sys.exit(0)
	elif sys.argv[0] == '-r' or sys.argv[0] == '--resume':
	        if len(sys.argv) > 1:
			resume_script=sys.argv[1]
			sys.argv.pop(0)
			sys.argv.pop(1)
	        else:
	                sys.argv.pop(0)
                        sys.stderr.write("Error: argument value required")
                        sys.exit(1)
	elif sys.argv[0] == '--list':
	        sys.argv.pop(0)
		for t in tests:
		      print t
		sys.exit(0)
        elif sys.argv[0] == '--list-xml':
                sys.argv.pop(0)
                for t in tests:
                        (mod,param) = t.split(None,2)
		        tname = mod[4:mod.find(".py")] + "_" + \
		                param[param.find("/")+1:param.find(".py")]
			c = ""
			if len(sys.argv):
				c = " ".join(sys.argv) + " "
                        tcmd = PYTHON + ' run.py ' + c + t
                        print '\t\t<Test name="%s" cmd="%s" wdir="tests/pjsua" />' % (tname, tcmd)
                sys.exit(0)
        elif sys.argv[0] == '-s' or sys.argv[0] == '--shell':
                if len(sys.argv) > 1:
			shell_cmd = sys.argv[1]
			sys.argv.pop(0)
			sys.argv.pop(1)
                else:
                        sys.argv.pop(0)
                        sys.stderr.write("Error: argument value required")
                        sys.exit(1)


# Generate arguments for run.py
argv_st = " ".join(sys.argv)

# Init vars
fails_cnt = 0
tests_cnt = 0

# Re-create "logs" directory
try:
    shutil.rmtree("logs")
except:
    print "Warning: failed in removing directory 'logs'"

try:
    os.mkdir("logs")
except:
    print "Warning: failed in creating directory 'logs'"

# Now run the tests
total_cnt = len(tests)
for t in tests:
	if resume_script!="" and t.find(resume_script)==-1:
	    print "Skipping " + t +".."
            total_cnt = total_cnt - 1
	    continue
	resume_script=""
	cmdline = "python run.py " + argv_st + t
        if shell_cmd:
                cmdline = "%s '%s'" % (shell_cmd, cmdline)
	t0 = time.time()
	msg = "Running %d/%d: %s..." % (tests_cnt+1, total_cnt, cmdline)
        sys.stdout.write(msg)
        sys.stdout.flush()
	ret = os.system(cmdline + " > output.log")
	t1 = time.time()
	if ret != 0:
		dur = int(t1 - t0)
		print " failed!! [" + str(dur) + "s]"
		logname = re.search(".*\s+(.*)", t).group(1)
		logname = re.sub("[\\\/]", "_", logname)
		logname = re.sub("\.py$", ".log", logname)
		logname = "logs/" + logname
		shutil.move("output.log", logname)
		print "Please see '" + logname + "' for the test log."
		fails_cnt += 1
	else:
		dur = int(t1 - t0)
		print " ok [" + str(dur) + "s]"
	tests_cnt += 1

if fails_cnt == 0:
	print "All " + str(tests_cnt) + " tests completed successfully"
else:
	print str(tests_cnt) + " tests completed, " +  str(fails_cnt) + " test(s) failed"

