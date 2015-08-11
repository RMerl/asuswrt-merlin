#!/usr/bin/python
import os
import sys
import time
import datetime
import ccdash

INTERVAL = 300
DELAY = 0
ONCE = False
SUFFIX = ""
FORCE = False

def run_scenarios(scenarios, group):
	# Run each scenario
	rc = 0
	for scenario in scenarios:
		argv = []
		argv.append("ccdash.py")
		argv.append("scenario")
		argv.append(scenario)
		argv.append("--group")
		argv.append(group)
		thisrc = ccdash.main(argv)
		if rc==0 and thisrc:
			rc = thisrc
	return rc


def usage():
	print """Periodically monitor working directory for Continuous and Nightly builds

Usage:
  run_continuous.py [options] scenario1.xml [scenario2.xml ...]

options:
  These are options which will be processed by run_continuous.py:

  --delay MIN   Delay both Continuous and Nightly builds by MIN minutes. 
  		This is useful to coordinate the build with other build 
		machines. By default, Continuous build will be done right
		after changes are detected, and Nightly build will be done
		at 00:00 GMT. MIN is a float number.

  --once        Just run one loop to see if anything needs to be done and
                if so just do it once. Quit after that.

  --suffix SFX  Set group suffix to SFX. For example, if SFX is "-2.x", then
                tests will be submitted to "Nightly-2.x", "Continuous-2.x",
		and "Experimental-2.x"

  --force	Force running the test even when nothing has changed.
"""
	sys.exit(1)

if __name__ == "__main__":
	if len(sys.argv)<=1 or sys.argv[1]=="-h" or sys.argv[1]=="--h" or sys.argv[1]=="--help" or sys.argv[1]=="/h":
		usage()

	# Splice list
	scenarios = []
	i = 1
	while i < len(sys.argv):
		if sys.argv[i]=="--delay":
			i = i + 1
			if i >= len(sys.argv):
				print "Error: missing argument"
				sys.exit(1)
			DELAY = float(sys.argv[i]) * 60
			print "Delay is set to %f minute(s)" % (DELAY / 60)
		elif sys.argv[i]=="--suffix":
			i = i + 1
			if i >= len(sys.argv):
				print "Error: missing argument"
				sys.exit(1)
			SUFFIX = sys.argv[i]
			print "Suffix is set to %s" % (SUFFIX)
		elif sys.argv[i]=="--once":
			ONCE = True
		elif sys.argv[i]=="--force":
			FORCE = True
		else:
			# Check if scenario exists
			scenario = sys.argv[i]
			if not os.path.exists(scenario):
				print "Error: file " + scenario + " does not exist"
				sys.exit(1)
			scenarios.append(scenario)
			print "Scenario %s added" % (scenario)
		i = i + 1

	if len(scenarios) < 1:
		print "Error: scenario is required"
		sys.exit(1)

	# Current date
	utc = time.gmtime(None)
	day = utc.tm_mday

	# Loop foreva
	while True:
		argv = []

		# Anything changed recently?
		argv.append("ccdash.py")
		argv.append("status")
		argv.append("-w")
		argv.append("../..")
		rc = ccdash.main(argv)

		utc = time.gmtime(None)

		if utc.tm_mday != day or rc != 0 or FORCE:
			group = ""
			if utc.tm_mday != day:
				day = utc.tm_mday
				group = "Nightly" + SUFFIX
			elif rc != 0:
				group = "Continuous" + SUFFIX
			else:
				group = "Experimental" + SUFFIX
			if DELAY > 0:
				print "Will run %s after %f s.." % (group, DELAY)
				time.sleep(DELAY)
			rc = run_scenarios(scenarios, group)
			msg = str(datetime.datetime.now()) + \
				  ": done running " + group + \
				  "tests, will check again in " + str(INTERVAL) + "s.."
			if ONCE:
				sys.exit(0)
		else:
			# Nothing changed
			msg = str(datetime.datetime.now()) + \
				  ": No update, will check again in " + str(INTERVAL) + "s.."
			if ONCE:
				sys.exit(1)

		print msg
		time.sleep(INTERVAL)

