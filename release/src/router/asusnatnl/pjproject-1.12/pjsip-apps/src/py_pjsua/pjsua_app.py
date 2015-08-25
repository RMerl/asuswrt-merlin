# $Id: pjsua_app.py 1438 2007-09-17 15:44:47Z bennylp $
#
# Sample and simple Python script to make and receive calls, and do
# presence and instant messaging/IM using PJSUA-API binding for Python.
#
# Copyright (C) 2003-2007 Benny Prijono <benny@prijono.org>
#
import py_pjsua
import sys
import thread

#
# Configurations
#
THIS_FILE = "pjsua_app.py"
C_QUIT = 0
C_LOG_LEVEL = 4

# STUN config.
# Set C_STUN_HOST to the address:port of the STUN server to enable STUN
#
C_STUN_HOST = ""
#C_STUN_HOST = "192.168.0.2"
#C_STUN_HOST = "stun.iptel.org:3478"

# SIP port
C_SIP_PORT = 5060


# Globals
#
g_ua_cfg = None
g_acc_id = py_pjsua.PJSUA_INVALID_ID
g_current_call = py_pjsua.PJSUA_INVALID_ID
g_wav_files = []
g_wav_id = 0
g_wav_port = 0
g_rec_file = ""
g_rec_id = 0
g_rec_port = 0

# Utility: display PJ error and exit
#
def err_exit(title, rc):
    py_pjsua.perror(THIS_FILE, title, rc)
    py_pjsua.destroy()
    exit(1)


# Logging function (also callback, called by pjsua-lib)
#
def log_cb(level, str, len):
    if level <= C_LOG_LEVEL:
        print str,

def write_log(level, str):
    log_cb(level, str + "\n", 0)


# Utility to get call info
#
def call_name(call_id):
	ci = py_pjsua.call_get_info(call_id)
	return "[Call " + `call_id` + " " + ci.remote_info + "]"

# Callback when call state has changed.
#
def on_call_state(call_id, e):	
	global g_current_call
	ci = py_pjsua.call_get_info(call_id)
	write_log(3, call_name(call_id) + " state = " + `ci.state_text`)
	if ci.state == py_pjsua.PJSIP_INV_STATE_DISCONNECTED:
		g_current_call = py_pjsua.PJSUA_INVALID_ID

# Callback for incoming call
#
def on_incoming_call(acc_id, call_id, rdata):
	global g_current_call
	
	if g_current_call != py_pjsua.PJSUA_INVALID_ID:
		# There's call in progress - answer Busy
		py_pjsua.call_answer(call_id, 486, None, None)
		return
	
	g_current_call = call_id
	ci = py_pjsua.call_get_info(call_id)
	write_log(3, "*** Incoming call: " + call_name(call_id) + "***")
	write_log(3, "*** Press a to answer or h to hangup  ***")
	
	
	
# Callback when media state has changed (e.g. established or terminated)
#
def on_call_media_state(call_id):
	ci = py_pjsua.call_get_info(call_id)
	if ci.media_status == py_pjsua.PJSUA_CALL_MEDIA_ACTIVE:
		py_pjsua.conf_connect(ci.conf_slot, 0)
		py_pjsua.conf_connect(0, ci.conf_slot)
		write_log(3, call_name(call_id) + ": media is active")
	else:
		write_log(3, call_name(call_id) + ": media is inactive")


# Callback when account registration state has changed
#
def on_reg_state(acc_id):
	acc_info = py_pjsua.acc_get_info(acc_id)
	if acc_info.has_registration != 0:
		cmd = "registration"
	else:
		cmd = "unregistration"
	if acc_info.status != 0 and acc_info.status != 200:
		write_log(3, "Account " + cmd + " failed: rc=" + `acc_info.status` + " " + acc_info.status_text)
	else:
		write_log(3, "Account " + cmd + " success")


# Callback when buddy's presence state has changed
#
def on_buddy_state(buddy_id):
	write_log(3, "On Buddy state called")
	buddy_info = py_pjsua.buddy_get_info(buddy_id)
	if buddy_info.status != 0 and buddy_info.status != 200:
		write_log(3, "Status of " + `buddy_info.uri` + " is " + `buddy_info.status_text`)
	else:
		write_log(3, "Status : " + `buddy_info.status`)

# Callback on incoming pager (MESSAGE)
#		
def on_pager(call_id, strfrom, strto, contact, mime_type, text):
	write_log(3, "MESSAGE from " + `strfrom` + " : " + `text`)


# Callback on the delivery status of outgoing pager (MESSAGE)
#	
def on_pager_status(call_id, strto, body, user_data, status, reason):
	write_log(3, "MESSAGE to " + `strto` + " status " + `status` + " reason " + `reason`)


# Received typing indication
#
def on_typing(call_id, strfrom, to, contact, is_typing):
	str_t = ""
	if is_typing:
		str_t = "is typing.."
	else:
		str_t = "has stopped typing"
	write_log(3, "IM indication: " + strfrom + " " + str_t)

# Received the status of previous call transfer request
#
def on_call_transfer_status(call_id,status_code,status_text,final,p_cont):
	strfinal = ""
	if final == 1:
		strfinal = "[final]"
	
	write_log(3, "Call " + `call_id` + ": transfer status= " + `status_code` + " " + status_text+ " " + strfinal)
	      
	if status_code/100 == 2:
		write_log(3, "Call " + `call_id` + " : call transfered successfully, disconnecting call")
		status = py_pjsua.call_hangup(call_id, 410, None, None)
		p_cont = 0

# Callback on incoming call transfer request
#		
def on_call_transfer_request(call_id, dst, code):
	write_log(3, "Call transfer request from " + `call_id` + " to " + dst + " with code " + `code`)

#
# Initialize pjsua.
#
def app_init():
	global g_acc_id, g_ua_cfg

	# Create pjsua before anything else
	status = py_pjsua.create()
	if status != 0:
		err_exit("pjsua create() error", status)

	# Create and initialize logging config
	log_cfg = py_pjsua.logging_config_default()
	log_cfg.level = C_LOG_LEVEL
	log_cfg.cb = log_cb

	# Create and initialize pjsua config
	# Note: for this Python module, thread_cnt must be 0 since Python
	#       doesn't like to be called from alien thread (pjsua's thread
	#       in this case)	    
	ua_cfg = py_pjsua.config_default()
	ua_cfg.thread_cnt = 0
	ua_cfg.user_agent = "PJSUA/Python 0.1"
	ua_cfg.cb.on_incoming_call = on_incoming_call
	ua_cfg.cb.on_call_media_state = on_call_media_state
	ua_cfg.cb.on_reg_state = on_reg_state
	ua_cfg.cb.on_call_state = on_call_state
	ua_cfg.cb.on_buddy_state = on_buddy_state
	ua_cfg.cb.on_pager = on_pager
	ua_cfg.cb.on_pager_status = on_pager_status
	ua_cfg.cb.on_typing = on_typing
	ua_cfg.cb.on_call_transfer_status = on_call_transfer_status
	ua_cfg.cb.on_call_transfer_request = on_call_transfer_request

	# Configure STUN setting
	if C_STUN_HOST != "":
	    ua_cfg.stun_host = C_STUN_HOST;

	# Create and initialize media config
	med_cfg = py_pjsua.media_config_default()
	med_cfg.ec_tail_len = 0

	#
	# Initialize pjsua!!
	#
	status = py_pjsua.init(ua_cfg, log_cfg, med_cfg)
	if status != 0:
		err_exit("pjsua init() error", status)

	# Configure UDP transport config
	transport_cfg = py_pjsua.transport_config_default()
	transport_cfg.port = C_SIP_PORT

	# Create UDP transport
	status, transport_id = \
	    py_pjsua.transport_create(py_pjsua.PJSIP_TRANSPORT_UDP, transport_cfg)
	if status != 0:
		err_exit("Error creating UDP transport", status)
		

	# Create initial default account
	status, acc_id = py_pjsua.acc_add_local(transport_id, 1)
	if status != 0:
		err_exit("Error creating account", status)

	g_acc_id = acc_id
	g_ua_cfg = ua_cfg

# Add SIP account interractively
#
def add_account():
	global g_acc_id

	acc_domain = ""
	acc_username = ""
	acc_passwd =""
	confirm = ""
	
	# Input account configs
	print "Your SIP domain (e.g. myprovider.com): ",
	acc_domain = sys.stdin.readline()
	if acc_domain == "\n": 
		return
	acc_domain = acc_domain.replace("\n", "")

	print "Your username (e.g. alice): ",
	acc_username = sys.stdin.readline()
	if acc_username == "\n":
		return
	acc_username = acc_username.replace("\n", "")

	print "Your password (e.g. secret): ",
	acc_passwd = sys.stdin.readline()
	if acc_passwd == "\n":
		return
	acc_passwd = acc_passwd.replace("\n", "")

	# Configure account configuration
	acc_cfg = py_pjsua.acc_config_default()
	acc_cfg.id = "sip:" + acc_username + "@" + acc_domain
	acc_cfg.reg_uri = "sip:" + acc_domain

	cred_info = py_pjsua.Pjsip_Cred_Info()
	cred_info.realm = "*"
	cred_info.scheme = "digest"
	cred_info.username = acc_username
	cred_info.data_type = 0
	cred_info.data = acc_passwd

	acc_cfg.cred_info.append(1)
	acc_cfg.cred_info[0] = cred_info

	# Add new SIP account
	status, acc_id = py_pjsua.acc_add(acc_cfg, 1)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error adding SIP account", status)
	else:
		g_acc_id = acc_id
		write_log(3, "Account " + acc_cfg.id + " added")

def add_player():
	global g_wav_files
	global g_wav_id
	global g_wav_port
	
	file_name = ""
	status = -1
	wav_id = 0
	
	print "Enter the path of the file player(e.g. /tmp/audio.wav): ",
	file_name = sys.stdin.readline()
	if file_name == "\n": 
		return
	file_name = file_name.replace("\n", "")
	status, wav_id = py_pjsua.player_create(file_name, 0)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error adding file player ", status)
	else:
		g_wav_files.append(file_name)
		if g_wav_id == 0:
			g_wav_id = wav_id
			g_wav_port = py_pjsua.player_get_conf_port(wav_id)
		write_log(3, "File player " + file_name + " added")
		
def add_recorder():
	global g_rec_file
	global g_rec_id
	global g_rec_port
	
	file_name = ""
	status = -1
	rec_id = 0
	
	print "Enter the path of the file recorder(e.g. /tmp/audio.wav): ",
	file_name = sys.stdin.readline()
	if file_name == "\n": 
		return
	file_name = file_name.replace("\n", "")
	status, rec_id = py_pjsua.recorder_create(file_name, 0, None, 0, 0)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error adding file recorder ", status)
	else:
		g_rec_file = file_name
		g_rec_id = rec_id
		g_rec_port = py_pjsua.recorder_get_conf_port(rec_id)
		write_log(3, "File recorder " + file_name + " added")

def conf_list():
	ports = None
	print "Conference ports : "
	ports = py_pjsua.enum_conf_ports()

	for port in ports:
		info = None
		info = py_pjsua.conf_get_port_info(port)
		txlist = ""
		for listener in info.listeners:
			txlist = txlist + "#" + `listener` + " "
		
		print "Port #" + `info.slot_id` + "[" + `(info.clock_rate/1000)` + "KHz/" + `(info.samples_per_frame * 1000 / info.clock_rate)` + "ms] " + info.name + " transmitting to: " + txlist
		
def connect_port():
	src_port = 0
	dst_port = 0
	
	print "Connect src port # (empty to cancel): "
	src_port = sys.stdin.readline()
	if src_port == "\n": 
		return
	src_port = src_port.replace("\n", "")
	src_port = int(src_port)
	print "To dst port # (empty to cancel): "
	dst_port = sys.stdin.readline()
	if dst_port == "\n": 
		return
	dst_port = dst_port.replace("\n", "")
	dst_port = int(dst_port)
	status = py_pjsua.conf_connect(src_port, dst_port)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error connecting port ", status)
	else:		
		write_log(3, "Port connected from " + `src_port` + " to " + `dst_port`)
		
def disconnect_port():
	src_port = 0
	dst_port = 0
	
	print "Disconnect src port # (empty to cancel): "
	src_port = sys.stdin.readline()
	if src_port == "\n": 
		return
	src_port = src_port.replace("\n", "")
	src_port = int(src_port)
	print "From dst port # (empty to cancel): "
	dst_port = sys.stdin.readline()
	if dst_port == "\n": 
		return
	dst_port = dst_port.replace("\n", "")
	dst_port = int(dst_port)
	status = py_pjsua.conf_disconnect(src_port, dst_port)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error disconnecting port ", status)
	else:		
		write_log(3, "Port disconnected " + `src_port` + " from " + `dst_port`)

def dump_call_quality():
	global g_current_call
	
	buf = ""
	if g_current_call != -1:
		buf = py_pjsua.call_dump(g_current_call, 1, 1024, "  ")
		write_log(3, "\n" + buf)
	else:
		write_log(3, "No current call")

def xfer_call():
	global g_current_call
	
	if g_current_call == -1:
		
		write_log(3, "No current call")

	else:
		call = g_current_call		
		ci = py_pjsua.call_get_info(g_current_call)
		print "Transfering current call ["+ `g_current_call` + "] " + ci.remote_info
		print "Enter sip url : "
		url = sys.stdin.readline()
		if url == "\n": 
			return
		url = url.replace("\n", "")
		if call != g_current_call:
			print "Call has been disconnected"
			return
		msg_data = py_pjsua.msg_data_init()
		status = py_pjsua.call_xfer(g_current_call, url, msg_data);
		if status != 0:
			py_pjsua.perror(THIS_FILE, "Error transfering call ", status)
		else:		
			write_log(3, "Call transfered to " + url)
		
def xfer_call_replaces():
	if g_current_call == -1:
		write_log(3, "No current call")
	else:
		call = g_current_call
		
		ids = py_pjsua.enum_calls()
		if len(ids) <= 1:
			print "There are no other calls"
			return		

		ci = py_pjsua.call_get_info(g_current_call)
		print "Transfer call [" + `g_current_call` + "] " + ci.remote_info + " to one of the following:"
		for i in range(0, len(ids)):		    
			if ids[i] == call:
				continue
			call_info = py_pjsua.call_get_info(ids[i])
			print `ids[i]` + "  " +  call_info.remote_info + "  [" + call_info.state_text + "]"		

		print "Enter call number to be replaced : "
		buf = sys.stdin.readline()
		buf = buf.replace("\n","")
		if buf == "":
			return
		dst_call = int(buf)
		
		if call != g_current_call:
			print "Call has been disconnected"
			return		
		
		if dst_call == call:
			print "Destination call number must not be the same as the call being transfered"
			return
		
		if dst_call >= py_pjsua.PJSUA_MAX_CALLS:
			print "Invalid destination call number"
			return
	
		if py_pjsua.call_is_active(dst_call) == 0:
			print "Invalid destination call number"
			return		

		py_pjsua.call_xfer_replaces(call, dst_call, 0, None)
		
#
# Worker thread function.
# Python doesn't like it when it's called from an alien thread
# (pjsua's worker thread, in this case), so for Python we must
# disable worker thread in pjsua and poll pjsua from Python instead.
#
def worker_thread_main(arg):
	global C_QUIT
	thread_desc = 0;
	status = py_pjsua.thread_register("python worker", thread_desc)
	if status != 0:
		py_pjsua.perror(THIS_FILE, "Error registering thread", status)
	else:
		while C_QUIT == 0:
			py_pjsua.handle_events(50)
		print "Worker thread quitting.."
		C_QUIT = 2


# Start pjsua
#
def app_start():
	# Done with initialization, start pjsua!!
	#
	status = py_pjsua.start()
	if status != 0:
		err_exit("Error starting pjsua!", status)

	# Start worker thread
	thr = thread.start_new(worker_thread_main, (0,))
    
	print "PJSUA Started!!"


# Print account and buddy list
def print_acc_buddy_list():
	global g_acc_id
	
	acc_ids = py_pjsua.enum_accs()
	print "Account list:"
	for acc_id in acc_ids:
		acc_info = py_pjsua.acc_get_info(acc_id)
		if acc_info.has_registration == 0:
			acc_status = acc_info.status_text
		else:
			acc_status = `acc_info.status` + "/" + acc_info.status_text + " (expires=" + `acc_info.expires` + ")"

		if acc_id == g_acc_id:
			print " *",
		else:
			print "  ",

		print "[" + `acc_id` + "] " + acc_info.acc_uri + ": " + acc_status
		print "       Presence status: ",
		if acc_info.online_status != 0:
			print "Online"
		else:
			print "Invisible"

	if py_pjsua.get_buddy_count() > 0:
		print ""
		print "Buddy list:"
		buddy_ids = py_pjsua.enum_buddies()
		for buddy_id in buddy_ids:
			bi = py_pjsua.buddy_get_info(buddy_id)
			print "   [" + `buddy_id` + "] " + bi.status_text + " " + bi.uri
	
		
# Print application menu
#
def print_menu():
	print ""
	print ">>>"
	print_acc_buddy_list()
	print """
+============================================================================+
|         Call Commands :      |  Buddy, IM & Presence:   |    Account:      |
|                              |                          |                  |
|  m  Make call                | +b  Add buddy            | +a  Add account  |
|  a  Answer current call      | -b  Delete buddy         | -a  Delete accnt |
|  h  Hangup current call      |                          |                  |
|  H  Hold call                |  i  Send instant message | rr  register     |
|  v  re-inVite (release Hold) |  s  Subscribe presence   | ru  Unregister   |
|  #  Send DTMF string         |  u  Unsubscribe presence |                  |
| dq  Dump curr. call quality  |  t  ToGgle Online status |                  |
|                              +--------------------------+------------------+
|  x  Xfer call                |     Media Commands:      |    Status:       |
|  X  Xfer with Replaces       |                          |                  |
|                              | cl  List ports           |  d  Dump status  |
|                              | cc  Connect port         | dd  Dump detail  |
|                              | cd  Disconnect port      |                  |
|                              | +p  Add file player      |                  |
|------------------------------+ +r  Add file recorder    |                  |
|  q  Quit application         |                          |                  |
+============================================================================+"""
	print "You have " + `py_pjsua.call_get_count()` + " active call(s)"
	print ">>>", 

# Menu
#
def app_menu():
	global g_acc_id
	global g_current_call

	quit = 0
	while quit == 0:
		print_menu()
		choice = sys.stdin.readline()

		if choice[0] == "q":
			quit = 1

		elif choice[0] == "i":
			# Sending IM	
			print "Send IM to SIP URL: ",
			url = sys.stdin.readline()
			if url == "\n":
				continue

			# Send typing indication
			py_pjsua.im_typing(g_acc_id, url, 1, None) 

			print "The content: ",
			message = sys.stdin.readline()
			if message == "\n":
				py_pjsua.im_typing(g_acc_id, url, 0, None) 		
				continue

			# Send the IM!
			py_pjsua.im_send(g_acc_id, url, None, message, None, 0)

		elif choice[0] == "m":
			# Make call 
			print "Using account ", g_acc_id
			print "Make call to SIP URL: ",
			url = sys.stdin.readline()
			url = url.replace("\n", "")
			if url == "":
				continue

			# Initiate the call!
			status, call_id = py_pjsua.call_make_call(g_acc_id, url, 0, 0, None)
            
			if status != 0:
				py_pjsua.perror(THIS_FILE, "Error making call", status)
			else:
				g_current_call = call_id

		elif choice[0] == "+" and choice[1] == "b":
			# Add new buddy
			bc = py_pjsua.Buddy_Config()
			print "Buddy URL: ",
			bc.uri = sys.stdin.readline()
			if bc.uri == "\n":
				continue
            
			bc.uri = bc.uri.replace("\n", "")
			bc.subscribe = 1
			status, buddy_id = py_pjsua.buddy_add(bc)
			if status != 0:
				py_pjsua.perror(THIS_FILE, "Error adding buddy", status)
		elif choice[0] == "-" and choice[1] == "b":
			print "Enter buddy ID to delete : "
			buf = sys.stdin.readline()
			buf = buf.replace("\n","")
			if buf == "":
				continue
			i = int(buf)
			if py_pjsua.buddy_is_valid(i) == 0:
				print "Invalid buddy id " + `i`
			else:
				py_pjsua.buddy_del(i)
				print "Buddy " + `i` + " deleted"		
		elif choice[0] == "+" and choice[1] == "a":
			# Add account
			add_account()
		elif choice[0] == "-" and choice[1] == "a":
			print "Enter account ID to delete : "
			buf = sys.stdin.readline()
			buf = buf.replace("\n","")
			if buf == "":
				continue
			i = int(buf)

			if py_pjsua.acc_is_valid(i) == 0:
				print "Invalid account id " + `i`
			else:
				py_pjsua.acc_del(i)
				print "Account " + `i` + " deleted"
	    
		elif choice[0] == "+" and choice[1] == "p":
			add_player()
		elif choice[0] == "+" and choice[1] == "r":
			add_recorder()
		elif choice[0] == "c" and choice[1] == "l":
			conf_list()
		elif choice[0] == "c" and choice[1] == "c":
			connect_port()
		elif choice[0] == "c" and choice[1] == "d":
			disconnect_port()
		elif choice[0] == "d" and choice[1] == "q":
			dump_call_quality()
		elif choice[0] == "x":
			xfer_call()
		elif choice[0] == "X":
			xfer_call_replaces()
		elif choice[0] == "h":
			if g_current_call != py_pjsua.PJSUA_INVALID_ID:
				py_pjsua.call_hangup(g_current_call, 603, None, None)
			else:
				print "No current call"
		elif choice[0] == "H":
			if g_current_call != py_pjsua.PJSUA_INVALID_ID:
				py_pjsua.call_set_hold(g_current_call, None)
		
			else:
				print "No current call"
		elif choice[0] == "v":
			if g_current_call != py_pjsua.PJSUA_INVALID_ID:
		
				py_pjsua.call_reinvite(g_current_call, 1, None);

			else:
				print "No current call"
		elif choice[0] == "#":
			if g_current_call == py_pjsua.PJSUA_INVALID_ID:		
				print "No current call"
			elif py_pjsua.call_has_media(g_current_call) == 0:
				print "Media is not established yet!"
			else:
				call = g_current_call
				print "DTMF strings to send (0-9*#A-B)"
				buf = sys.stdin.readline()
				buf = buf.replace("\n", "")
				if buf == "":
					continue
				if call != g_current_call:
					print "Call has been disconnected"
					continue
				status = py_pjsua.call_dial_dtmf(g_current_call, buf)
				if status != 0:
					py_pjsua.perror(THIS_FILE, "Unable to send DTMF", status);
				else:
					print "DTMF digits enqueued for transmission"
		elif choice[0] == "s":
			print "Subscribe presence of (buddy id) : "
			buf = sys.stdin.readline()
			buf = buf.replace("\n","")
			if buf == "":
				continue
			i = int(buf)
			py_pjsua.buddy_subscribe_pres(i, 1)
		elif choice[0] == "u":
			print "Unsubscribe presence of (buddy id) : "
			buf = sys.stdin.readline()
			buf = buf.replace("\n","")
			if buf == "":
				continue
			i = int(buf)
			py_pjsua.buddy_subscribe_pres(i, 0)
		elif choice[0] == "t":
			acc_info = py_pjsua.acc_get_info(g_acc_id)
			if acc_info.online_status == 0:
				acc_info.online_status = 1
			else:
				acc_info.online_status = 0
			py_pjsua.acc_set_online_status(g_acc_id, acc_info.online_status)
			st = ""
			if acc_info.online_status == 0:
				st = "offline"
			else:
				st = "online"
			print "Setting " + acc_info.acc_uri + " online status to " + st
		elif choice[0] == "r":
			if choice[1] == "r":	    
				py_pjsua.acc_set_registration(g_acc_id, 1)
			elif choice[1] == "u":
				py_pjsua.acc_set_registration(g_acc_id, 0)
		elif choice[0] == "d":
			py_pjsua.dump(choice[1] == "d")
		elif choice[0] == "a":
			if g_current_call != py_pjsua.PJSUA_INVALID_ID:				
				
				py_pjsua.call_answer(g_current_call, 200, None, None)
			else:
				print "No current call"


#
# main
#
app_init()
app_start()
app_menu()

#
# Done, quitting..
#
print "PJSUA shutting down.."
C_QUIT = 1
# Give the worker thread chance to quit itself
while C_QUIT != 2:
    py_pjsua.handle_events(50)

print "PJSUA destroying.."
py_pjsua.destroy()

