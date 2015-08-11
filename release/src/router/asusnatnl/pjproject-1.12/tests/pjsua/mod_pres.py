# $Id: mod_pres.py 2078 2008-06-27 21:12:12Z nanang $
import time
import imp
import sys
import inc_const as const
from inc_cfg import *

# Load configuration
cfg_file = imp.load_source("cfg_file", ARGS[1])


# Test body function
def test_func(t):
	u1 = t.process[0]
	uri1 = cfg_file.test_param.inst_params[0].uri
	acc1 = "-1"
	u2 = t.process[1]
	uri2 = cfg_file.test_param.inst_params[1].uri
	acc2 = "-1"

	# if have_reg then wait for couple of seconds for PUBLISH
	# to complete (just in case pUBLISH is used)
	if u1.inst_param.have_reg:
		time.sleep(1)
	if u2.inst_param.have_reg:
		time.sleep(1)

	# U1 adds U2 as buddy
	u1.send("+b")
	u1.send(uri2)
	u1.expect("Subscription state changed NULL --> SENT")
	u1.expect("Presence subscription.*is ACCEPTED")
	if not u2.inst_param.have_publish:
		# Process incoming SUBSCRIBE in U2
		# Finds out which account gets the subscription in U2
		line = u2.expect("pjsua_pres.*subscription.*using account")
		acc2 = line.split("using account ")[1]
	# wait until we've got Online notification
	u1.expect(uri2 + ".*Online")

	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# U2 adds U1 as buddy
	u2.send("+b")
	u2.send(uri1)
	u2.expect("Subscription state changed NULL --> SENT")
	u2.expect("Presence subscription.*is ACCEPTED")
	if not u1.inst_param.have_publish:
		# Process incoming SUBSCRIBE in U1
		# Finds out which account gets the subscription in U1
		line = u1.expect("pjsua_pres.*subscription.*using account")
		acc1 = line.split("using account ")[1]
	# wait until we've got Online notification
	u2.expect(uri1 + ".*Online")

	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# Set current account in both U1 and U2
	if acc1!="-1":
		u1.send(">")
		u1.send(acc1)
		u1.expect("Current account changed")
	if acc2!="-1":
		u2.send(">")
		u2.send(acc2)
		u2.expect("Current account changed")

	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# u2 toggles online status
	u2.send("t")
	u1.expect(uri2 + ".*status.*Offline")
	u2.expect("offline")
	
	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# u1 toggles online status
	u1.send("t")
	u2.expect(uri1 + ".*status.*Offline")
	u1.expect("offline")

	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# u2 set online status to On the phone
	u2.send("T")
	u2.send("3")
	u1.expect(uri2 + ".*status.*On the phone")
	u2.expect("On the phone")
	
	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()

	# U1 send IM
	im_text = "Hello World from U1"
	u1.send("i")
	u1.send(uri2)
	u2.expect(" is typing")
	u1.send(im_text)
	u1.expect(im_text+".*delivered successfully")
	u2.expect("MESSAGE from.*"+im_text)
	
	# Synchronize stdout
	u1.sync_stdout()
	u2.sync_stdout()


# Here where it all comes together
test = cfg_file.test_param
test.test_func = test_func

