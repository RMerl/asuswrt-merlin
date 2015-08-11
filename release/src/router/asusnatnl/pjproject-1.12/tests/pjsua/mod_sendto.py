# $Id: mod_sendto.py 3243 2010-08-01 09:48:51Z bennylp $
import imp
import sys
import inc_sip as sip
import inc_const as const
import re
from inc_cfg import *

# Read configuration
cfg_file = imp.load_source("cfg_file", ARGS[1])

# Test body function
def test_func(t):
	pjsua = t.process[0]
	# Create dialog
	dlg = sip.Dialog("127.0.0.1", pjsua.inst_param.sip_port, 
			  tcp=cfg_file.sendto_cfg.use_tcp)
	#dlg = sip.Dialog("127.0.0.1", 5060, tcp=cfg_file.sendto_cfg.use_tcp)
	cfg = cfg_file.sendto_cfg
	
	if len(cfg.complete_msg) != 0:
		req = dlg.update_fields(cfg.complete_msg)
	else:
		req = dlg.create_invite(cfg.sdp, cfg.extra_headers, cfg.body)
	resp = dlg.send_request_wait(req, 10)
	if resp=="":
		raise TestError("Timed-out waiting for response")
	# Check response code
	code = int(sip.get_code(resp))
	if code != cfg.resp_code:
		dlg.hangup(code)
		raise TestError("Expecting code " + str(cfg.resp_code) + 
				" got " + str(code))
	# Check for patterns that must exist
	for p in cfg.resp_include:
		if re.search(p, resp, re.M | re.I)==None:
			dlg.hangup(code)
			raise TestError("Pattern " + p + " not found")
	# Check for patterns that must not exist
	for p in cfg.resp_exclude:
		if re.search(p, resp, re.M | re.I)!=None:
			dlg.hangup(code)
			raise TestError("Excluded pattern " + p + " found")
	pjsua.sync_stdout()
	dlg.hangup(code)
	pjsua.sync_stdout()

# Here where it all comes together
test = TestParam(cfg_file.sendto_cfg.name, 
		 [cfg_file.sendto_cfg.inst_param], 
		 test_func)


