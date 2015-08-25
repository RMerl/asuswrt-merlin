# $Id: mod_recvfrom.py 3259 2010-08-09 07:31:34Z nanang $
import imp
import sys
import inc_sip as sip
import inc_const as const
import re
from inc_cfg import *

# Read configuration
cfg_file = imp.load_source("cfg_file", ARGS[1])

# Default server port (should we randomize?)
srv_port = 50070

def test_func(test):
	pjsua = test.process[0]
	dlg = sip.Dialog("127.0.0.1", pjsua.inst_param.sip_port, 
			 local_port=srv_port, 
			 tcp=cfg_file.recvfrom_cfg.tcp)

	last_cseq = 0
	last_method = ""
	last_call_id = ""
	for t in cfg_file.recvfrom_cfg.transaction:
		# Print transaction title
		if t.title != "":
			dlg.trace(t.title)
		# Run command and expect patterns
		for c in t.cmds:
			if c[0] and c[0] != "":
				pjsua.send(c[0])
			if len(c)>1 and c[1] and c[1] != "":
				pjsua.expect(c[1])
		# Wait for request
		if t.check_cseq:
			# Absorbs retransmissions
			cseq = 0
			method = last_method
			call_id = last_call_id
			while cseq <= last_cseq and method == last_method and call_id == last_call_id:
				request, src_addr = dlg.wait_msg_from(30)
				if request==None or request=="":
					raise TestError("Timeout waiting for request")
				method = request.split(" ", 1)[0]
				cseq_hval = sip.get_header(request, "CSeq")
				cseq_hval = cseq_hval.split(" ")[0]
				cseq = int(cseq_hval)
				call_id = sip.get_header(request, "Call-ID")
			last_cseq = cseq
			last_method = method
		else:
			request, src_addr = dlg.wait_msg_from(30)
			if request==None or request=="":
				raise TestError("Timeout waiting for request")

		# Check for include patterns
		for pat in t.include:
			if re.search(pat, request, re.M | re.I)==None:
				if t.title:
					tname = " in " + t.title + " transaction"
				else:
					tname = ""
				raise TestError("Pattern " + pat + " not found" + tname)
		# Check for exclude patterns
		for pat in t.exclude:
			if re.search(pat, request, re.M | re.I)!=None:
				if t.title:
					tname = " in " + t.title + " transaction"
				else:
					tname = ""
				raise TestError("Excluded pattern " + pat + " found" + tname)
		# Create response
		if t.resp_code!=0:
			response = dlg.create_response(request, t.resp_code, "Status reason")
			# Add headers to response
			for h in t.resp_hdr:
				response = response + h + "\r\n"
			# Add message body if required
			if t.body:
				response = response + t.body
			# Send response
			dlg.send_msg(response, src_addr)

		# Expect something to happen in pjsua
		if t.expect != "":
			pjsua.expect(t.expect)
		# Sync
		pjsua.sync_stdout()

# Replace "$PORT" with server port in pjsua args
cfg_file.recvfrom_cfg.inst_param.arg = cfg_file.recvfrom_cfg.inst_param.arg.replace("$PORT", str(srv_port))

# Here where it all comes together
test = TestParam(cfg_file.recvfrom_cfg.name, 
		 [cfg_file.recvfrom_cfg.inst_param], 
		 test_func)

