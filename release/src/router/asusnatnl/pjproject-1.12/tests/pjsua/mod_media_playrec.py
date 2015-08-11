# $Id: mod_media_playrec.py 2884 2009-08-17 08:29:47Z bennylp $

# PLAYFILE -> RECFILE:
# Input file is played and is recorded to output, then compare them.
# Useful to tes clock rates compatibility and resample quality
#	null-audio
#	port 1: wav file input xxxxxx.clock_rate.wav, e.g: test1.8.wav
#	port 2: wav file ouput xxxxxx.clock_rate.wav, e.g: res1.8.wav
#	wav input must be more than 3 seconds long

import time
import imp
import sys
import re
import subprocess
import inc_const as const
from inc_cfg import *

# Load configuration
cfg_file = imp.load_source("cfg_file", ARGS[1])

# WAV similarity calculator
COMPARE_WAV_EXE = ""
if sys.platform.find("win32")!=-1:
    COMPARE_WAV_EXE = "tools/cmp_wav.exe"
    G_INUNIX = False
else:
    COMPARE_WAV_EXE = "tools/cmp_wav"
    G_INUNIX = True


# Threshold to declare degradation is too high when result is lower than this value
COMPARE_THRESHOLD = 2

# COMPARE params
input_filename	= ""			# Input filename
output_filename = ""			# Output filename

# Test body function
def test_func(t):
	global input_filename
	global output_filename

	endpt = t.process[0]
	
	# Get input file name
	input_filename = re.compile(const.MEDIA_PLAY_FILE).search(endpt.inst_param.arg).group(1)
	endpt.trace("Input file = " + input_filename)

	# Get output file name
	output_filename = re.compile(const.MEDIA_REC_FILE).search(endpt.inst_param.arg).group(1)
	endpt.trace("Output file = " + output_filename)

	# Find appropriate clock rate for the input file
	clock_rate = re.compile(".+(\.\d+\.wav)$").match(output_filename).group(1)
	if (clock_rate==None):
		endpt.trace("Cannot compare input & output, incorrect output filename format")
		return
	input_filename = re.sub("\.\d+\.wav$", clock_rate, input_filename)
	endpt.trace("WAV file to be compared with output = " + input_filename)

	# Connect input-output file
	endpt.sync_stdout()

	endpt.send("cc 1 2")
	endpt.expect(const.MEDIA_CONN_PORT_SUCCESS)

	# Wait
	time.sleep(3)

	endpt.sync_stdout()

	# Disconnect input-output file
	endpt.send("cd 1 2")
	endpt.expect(const.MEDIA_DISCONN_PORT_SUCCESS)


# Post body function
def post_func(t):
	global input_filename
	global output_filename

	endpt = t.process[0]

	# Check WAV similarity
	fullcmd = COMPARE_WAV_EXE + " " + input_filename + " " + output_filename + " " + "3000"
	endpt.trace("Popen " + fullcmd)
	cmp_proc = subprocess.Popen(fullcmd, shell=G_INUNIX, stdout=subprocess.PIPE, universal_newlines=True)

	# Parse similarity ouput
	line = cmp_proc.stdout.readline()
	mo_sim_val = re.match(".+=\s+(\d+)", line)
	if (mo_sim_val == None):
		raise TestError("Error comparing WAV files")
		return

	# Evaluate the similarity value
	sim_val = mo_sim_val.group(1)
	if (sim_val >= COMPARE_THRESHOLD):
		endpt.trace("WAV similarity = " + sim_val)
	else:
		raise TestError("WAV degraded heavily, similarity = " + sim_val)


# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func
test.post_func = post_func
