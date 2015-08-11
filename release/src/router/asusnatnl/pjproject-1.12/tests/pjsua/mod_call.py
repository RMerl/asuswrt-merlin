# $Id: mod_call.py 2078 2008-06-27 21:12:12Z nanang $
import time
import imp
import sys
import inc_const as const
from inc_cfg import *

# Load configuration
cfg_file = imp.load_source("cfg_file", ARGS[1])

# Check media flow between ua1 and ua2
def check_media(ua1, ua2):
	ua1.send("#")
	ua1.expect("#")
	ua1.send("1122")
	ua2.expect(const.RX_DTMF + "1")
	ua2.expect(const.RX_DTMF + "1")
	ua2.expect(const.RX_DTMF + "2")
	ua2.expect(const.RX_DTMF + "2")


# Test body function
def test_func(t):
	callee = t.process[0]
	caller = t.process[1]

	# if have_reg then wait for couple of seconds for PUBLISH
	# to complete (just in case pUBLISH is used)
	if callee.inst_param.have_reg:
		time.sleep(1)
	if caller.inst_param.have_reg:
		time.sleep(1)

	# Caller making call
	caller.send("m")
	caller.send(t.inst_params[0].uri)
	caller.expect(const.STATE_CALLING)
	
	# Callee waits for call and answers with 180/Ringing
	time.sleep(0.2)
	callee.expect(const.EVENT_INCOMING_CALL)
	callee.send("a")
	callee.send("180")
	callee.expect("SIP/2.0 180")
	caller.expect("SIP/2.0 180")

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Callee answers with 200/OK
	callee.send("a")
	callee.send("200")

	# Wait until call is connected in both endpoints
	time.sleep(0.2)
	caller.expect(const.STATE_CONFIRMED)
	callee.expect(const.STATE_CONFIRMED)

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()
	time.sleep(0.1)
	caller.sync_stdout()
	callee.sync_stdout()

	# Test that media is okay
	time.sleep(0.3)
	check_media(caller, callee)
	check_media(callee, caller)

	# Hold call by caller
	caller.send("H")
	caller.expect("INVITE sip:")
	callee.expect("INVITE sip:")
	caller.expect(const.MEDIA_HOLD)
	callee.expect(const.MEDIA_HOLD)
	
	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Release hold
	time.sleep(0.5)
	caller.send("v")
	caller.expect("INVITE sip:")
	callee.expect("INVITE sip:")
	caller.expect(const.MEDIA_ACTIVE, title="waiting for media active after call hold")
	callee.expect(const.MEDIA_ACTIVE, title="waiting for media active after call hold")

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Test that media is okay
	check_media(caller, callee)
	check_media(callee, caller)

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Hold call by callee
	callee.send("H")
	callee.expect("INVITE sip:")
	caller.expect("INVITE sip:")
	caller.expect(const.MEDIA_HOLD)
	callee.expect(const.MEDIA_HOLD)
	
	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Release hold
	time.sleep(0.1)
	callee.send("v")
	callee.expect("INVITE sip:")
	caller.expect("INVITE sip:")
	callee.expect(const.MEDIA_ACTIVE, title="waiting for media active after call hold")
	caller.expect(const.MEDIA_ACTIVE, title="waiting for media active after call hold")

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Test that media is okay
	# Wait for some time for ICE negotiation
	time.sleep(0.6)
	check_media(caller, callee)
	check_media(callee, caller)

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# UPDATE (by caller)
	caller.send("U")
	#caller.sync_stdout()
	callee.expect(const.MEDIA_ACTIVE, title="waiting for media active with UPDATE")
	caller.expect(const.MEDIA_ACTIVE, title="waiting for media active with UPDATE")
	
	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Test that media is okay
	time.sleep(0.1)
	check_media(caller, callee)
	check_media(callee, caller)

	# UPDATE (by callee)
	callee.send("U")
	callee.expect("UPDATE sip:")
	caller.expect("UPDATE sip:")
	caller.expect(const.MEDIA_ACTIVE, title="waiting for media active with UPDATE")
	callee.expect(const.MEDIA_ACTIVE, title="waiting for media active with UPDATE")
	
	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Test that media is okay
	time.sleep(0.1)
	check_media(caller, callee)
	check_media(callee, caller)

	# Synchronize stdout
	caller.sync_stdout()
	callee.sync_stdout()

	# Set codecs in both caller and callee so that there is
	# no common codec between them.
	# In caller we only enable PCMU, in callee we only enable PCMA
	caller.send("Cp")
	caller.expect("Enter codec")
	caller.send("* 0")
	caller.send("Cp")
	caller.expect("Enter codec")
	caller.send("pcmu 120")
	
	callee.send("Cp")
	callee.expect("Enter codec")
	callee.send("* 0")
	callee.send("Cp")
	callee.expect("Enter codec")
	callee.send("pcma 120")

	# Test when UPDATE fails (by callee)
	callee.send("U")
	caller.expect("SIP/2.0 488")
	callee.expect("SIP/2.0 488")
	callee.sync_stdout()
	caller.sync_stdout()
	
	# Test that media is still okay
	time.sleep(0.1)
	check_media(caller, callee)
	check_media(callee, caller)

	# Test when UPDATE fails (by caller)
	caller.send("U")
	caller.expect("UPDATE sip:")
	callee.expect("UPDATE sip:")
	callee.expect("SIP/2.0 488")
	caller.expect("SIP/2.0 488")
	caller.sync_stdout()
	callee.sync_stdout()
	
	# Test that media is still okay
	time.sleep(0.1)
	check_media(callee, caller)
	check_media(caller, callee)

	# Hangup call
	time.sleep(0.1)
	caller.send("h")

	# Wait until calls are cleared in both endpoints
	caller.expect(const.STATE_DISCONNECTED)
	callee.expect(const.STATE_DISCONNECTED)
	

# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func

