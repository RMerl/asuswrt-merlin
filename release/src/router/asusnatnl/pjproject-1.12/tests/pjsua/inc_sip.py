# $Id: inc_sip.py 3283 2010-08-18 07:37:29Z bennylp $
#
from socket import *
import re
import random
import time
import sys
import inc_cfg as cfg
from select import *

# SIP request template
req_templ = \
"""$METHOD $TARGET_URI SIP/2.0\r
Via: SIP/2.0/UDP $LOCAL_IP:$LOCAL_PORT;rport;branch=z9hG4bK$BRANCH\r
Max-Forwards: 70\r
From: <sip:caller@pjsip.org>$FROM_TAG\r
To: <$TARGET_URI>$TO_TAG\r
Contact: <sip:$LOCAL_IP:$LOCAL_PORT;transport=udp>\r
Call-ID: $CALL_ID@pjsip.org\r
CSeq: $CSEQ $METHOD\r
Allow: PRACK, INVITE, ACK, BYE, CANCEL, UPDATE, REFER\r
Supported: replaces, 100rel, norefersub\r
User-Agent: pjsip.org Python tester\r
Content-Length: $CONTENT_LENGTH\r
$SIP_HEADERS"""


def is_request(msg):
	return msg.split(" ", 1)[0] != "SIP/2.0"
	
def is_response(msg):
	return msg.split(" ", 1)[0] == "SIP/2.0"

def get_code(msg):
	if msg=="":
		return 0
	return int(msg.split(" ", 2)[1])

def get_tag(msg, hdr="To"):
	pat = "^" + hdr + ":.*"
	result = re.search(pat, msg, re.M | re.I)
	if result==None:
		return ""
	line = result.group()
	#print "line=", line
	tags = line.split(";tag=")
	if len(tags)>1:
		return tags[1]
	return ""
	#return re.split("[;& ]", s)

def get_header(msg, hname):
	headers = msg.splitlines()
	for hdr in headers:
		hfields = hdr.split(": ", 2)
		if hfields[0]==hname:
			return hfields[1]
	return None

class Dialog:
	sock = None
	dst_addr = ""
	dst_port = 5060
	local_ip = ""
	local_port = 0
	tcp = False
	call_id = str(random.random())
	cseq = 0
	local_tag = ";tag=" + str(random.random())
	rem_tag = ""
	last_resp_code = 0
	inv_branch = ""
	trace_enabled = True
	last_request = ""
	def __init__(self, dst_addr, dst_port=5060, tcp=False, trace=True, local_port=0):
		self.dst_addr = dst_addr
		self.dst_port = dst_port
		self.tcp = tcp
		self.trace_enabled = trace
		if tcp==True:
			self.sock = socket(AF_INET, SOCK_STREAM)
			self.sock.connect(dst_addr, dst_port)
		else:
			self.sock = socket(AF_INET, SOCK_DGRAM)
			self.sock.bind(("127.0.0.1", local_port))
		
		self.local_ip, self.local_port = self.sock.getsockname()
		self.trace("Dialog socket bound to " + self.local_ip + ":" + str(self.local_port))

	def trace(self, txt):
		if self.trace_enabled:
			print str(time.strftime("%H:%M:%S ")) + txt

	def update_fields(self, msg):
		if self.tcp:
			transport_param = ";transport=tcp"
		else:
			transport_param = ""
		msg = msg.replace("$TARGET_URI", "sip:"+self.dst_addr+":"+str(self.dst_port) + transport_param)
		msg = msg.replace("$LOCAL_IP", self.local_ip)
		msg = msg.replace("$LOCAL_PORT", str(self.local_port))
		msg = msg.replace("$FROM_TAG", self.local_tag)
		msg = msg.replace("$TO_TAG", self.rem_tag)
		msg = msg.replace("$CALL_ID", self.call_id)
		msg = msg.replace("$CSEQ", str(self.cseq))
		branch=str(random.random())
		msg = msg.replace("$BRANCH", branch)
		return msg

	def create_req(self, method, sdp, branch="", extra_headers="", body=""):
		if branch=="":
			self.cseq = self.cseq + 1
		msg = req_templ
		msg = msg.replace("$METHOD", method)
		msg = msg.replace("$SIP_HEADERS", extra_headers)
		if branch=="":
			branch=str(random.random())
		msg = msg.replace("$BRANCH", branch)
		if sdp!="":
			msg = msg.replace("$CONTENT_LENGTH", str(len(sdp)))
			msg = msg + "Content-Type: application/sdp\r\n"
			msg = msg + "\r\n"
			msg = msg + sdp
		elif body!="":
			msg = msg.replace("$CONTENT_LENGTH", str(len(body)))
			msg = msg + "\r\n"
			msg = msg + body
		else:
			msg = msg.replace("$CONTENT_LENGTH", "0")
		return self.update_fields(msg)

	def create_response(self, request, code, reason, to_tag=""):
		response = "SIP/2.0 " + str(code) + " " + reason + "\r\n"
		lines = request.splitlines()
		for line in lines:
			hdr = line.split(":", 1)[0]
			if hdr in ["Via", "From", "To", "CSeq", "Call-ID"]:
				if hdr=="To" and to_tag!="":
					line = line + ";tag=" + to_tag
				elif hdr=="Via":
					line = line + ";received=127.0.0.1"
				response = response + line + "\r\n"
		return response

	def create_invite(self, sdp, extra_headers="", body=""):
		self.inv_branch = str(random.random())
		return self.create_req("INVITE", sdp, branch=self.inv_branch, extra_headers=extra_headers, body=body)

	def create_ack(self, sdp="", extra_headers=""):
		return self.create_req("ACK", sdp, extra_headers=extra_headers, branch=self.inv_branch)

	def create_bye(self, extra_headers=""):
		return self.create_req("BYE", "", extra_headers)

	def send_msg(self, msg, dst_addr=None):
		if (is_request(msg)):
			self.last_request = msg.split(" ", 1)[0]
		if not dst_addr:
			dst_addr = (self.dst_addr, self.dst_port)
		self.trace("============== TX MSG to " + str(dst_addr) + " ============= \n" + msg)
		self.sock.sendto(msg, 0, dst_addr)

	def wait_msg_from(self, timeout):
		endtime = time.time() + timeout
		msg = ""
		src_addr = None
		while time.time() < endtime:
			readset = select([self.sock], [], [], 1)
			if len(readset[0]) < 1 or not self.sock in readset[0]:
				if len(readset[0]) < 1:
					print "select() timeout (will wait for " + str(int(endtime - time.time())) + "more secs)"
				elif not self.sock in readset[0]:
					print "select() alien socket"
				else:
					print "select other error"
				continue
			try:
				msg, src_addr = self.sock.recvfrom(4096)
				break
			except:
				print "recv() exception: ", sys.exc_info()[0]
				continue

		if msg=="":
			return "", None
		if self.last_request=="INVITE" and self.rem_tag=="":
			self.rem_tag = get_tag(msg, "To")
			self.rem_tag = self.rem_tag.rstrip("\r\n;")
			if self.rem_tag != "":
				self.rem_tag = ";tag=" + self.rem_tag
			self.trace("=== rem_tag:" + self.rem_tag)
		self.trace("=========== RX MSG from " + str(src_addr) +  " ===========\n" + msg)
		return (msg, src_addr)
	
	def wait_msg(self, timeout):
		return self.wait_msg_from(timeout)[0]
		
	# Send request and wait for final response
	def send_request_wait(self, msg, timeout):
		t1 = 1.0
		endtime = time.time() + timeout
		resp = ""
		code = 0
		for i in range(0,5):
			self.send_msg(msg)
			resp = self.wait_msg(t1)
			if resp!="" and is_response(resp):
				code = get_code(resp)
				break
		last_resp = resp
		while code < 200 and time.time() < endtime:
			resp = self.wait_msg(endtime - time.time())
			if resp != "" and is_response(resp):
				code = get_code(resp)
				last_resp = resp
			elif resp=="":
				break
		return last_resp
	
	def hangup(self, last_code=0):
		self.trace("====== hangup =====")
		if last_code!=0:
			self.last_resp_code = last_code
		if self.last_resp_code>0 and self.last_resp_code<200:
			msg = self.create_req("CANCEL", "", branch=self.inv_branch, extra_headers="")
			self.send_request_wait(msg, 5)
			msg = self.create_ack()
			self.send_msg(msg)
		elif self.last_resp_code>=200 and self.last_resp_code<300:
			msg = self.create_ack()
			self.send_msg(msg)
			msg = self.create_bye()
			self.send_request_wait(msg, 5)
		else:
			msg = self.create_ack()
			self.send_msg(msg)


class SendtoCfg:
	# Test name
	name = ""
	# pjsua InstanceParam
	inst_param = None
	# Complete INVITE message. If this is not empty, then this
	# message will be sent instead and the "sdp" and "extra_headers"
	# settings will be ignored.
	complete_msg = ""
	# Initial SDP
	sdp = ""
	# Extra headers to add to request
	extra_headers = ""
	# Expected code
	resp_code = 0
	# Use TCP?
	use_tcp = False
	# List of RE patterns that must exist in response
	resp_include = []
	# List of RE patterns that must NOT exist in response
	resp_exclude = []
	# Full (non-SDP) body
	body = ""
	# Constructor
	def __init__(self, name, pjsua_args, sdp, resp_code, 
		     resp_inc=[], resp_exc=[], use_tcp=False,
		     extra_headers="", body="", complete_msg="",
		     enable_buffer = False):
	 	self.complete_msg = complete_msg
		self.sdp = sdp
		self.resp_code = resp_code
		self.resp_include = resp_inc
		self.resp_exclude = resp_exc
		self.use_tcp = use_tcp
		self.extra_headers = extra_headers
		self.body = body
		self.inst_param = cfg.InstanceParam("pjsua", pjsua_args)
		self.inst_param.enable_buffer = enable_buffer 


class RecvfromTransaction:
	# The test title for this transaction
	title = ""
	# Optinal list of pjsua command and optional expect patterns 
	# to be invoked to make pjsua send a request
	# Sample:
	#	(to make call and wait for INVITE to be sent)
	#	cmds = [ ["m"], ["sip:127.0.0.1", "INVITE sip:"]  ]
	cmds = []
	# Check if the CSeq must be greater than last Cseq?
	check_cseq = True
	# List of RE patterns that must exists in incoming request
	include = []
	# List of RE patterns that MUST NOT exist in incoming request
	exclude = []
	# Response code to send
	resp_code = 0
	# Additional list of headers to be sent on the response
	# Note: no need to add CRLF on the header
	resp_hdr = []
	# Message body. This should include the Content-Type header too.
	# Sample:
	#	body = """Content-Type: application/sdp\r\n
	#	\r\n
	#	v=0\r\n
	#	...
	#	"""
	body = None
	# Pattern to be expected on pjsua when receiving the response
	expect = ""
	
	def __init__(self, title, resp_code, check_cseq=True,
			include=[], exclude=[], cmds=[], resp_hdr=[], resp_body=None, expect=""):
		self.title = title
		self.cmds = cmds
		self.include = include
		self.exclude = exclude
		self.resp_code = resp_code
		self.resp_hdr = resp_hdr
		self.body = resp_body
		self.expect = expect
			

class RecvfromCfg:
	# Test name
	name = ""
	# pjsua InstanceParam
	inst_param = None
	# List of RecvfromTransaction
	transaction = None
	# Use TCP?
	tcp = False

	# Note:
	#  Any "$PORT" string in the pjsua_args will be replaced
	#  by server port
	def __init__(self, name, pjsua_args, transaction, tcp=False):
		self.name = name
		self.inst_param = cfg.InstanceParam("pjsua", pjsua_args)
		self.transaction = transaction
		self.tcp=tcp




