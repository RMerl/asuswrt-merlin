# $Id: confbot.py 2912 2009-08-24 11:56:13Z bennylp $
#
# SIP Conference Bot
#
# Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
#
import pjsua as pj
import string
import sys

CFG_FILE = "config"

INFO = 1
TRACE = 2
	
# Call callback. This would just forward the event to the Member class
class CallCb(pj.CallCallback):
	def __init__(self, member, call=None):
		pj.CallCallback.__init__(self, call)
		self.member = member
	
	def on_state(self):
		self.member.on_call_state(self.call)
	
	def on_media_state(self):
		self.member.on_call_media_state(self.call)

	def on_dtmf_digit(self, digits):
		self.member.on_call_dtmf_digit(self.call, digits)

	def on_transfer_request(self, dst, code):
		return self.member.on_call_transfer_request(self.call, dst, code)
	
	def on_transfer_status(self, code, reason, final, cont):
		return self.member.on_call_transfer_status(self.call, code, reason, final, cont)

	def on_replace_request(self, code, reason):
		return self.member.on_call_replace_request(self.call, code, reason)
		
	def on_replaced(self, new_call):
		self.member.on_call_replaced(self.call, new_call)

	def on_typing(self, is_typing):
		self.member.on_typing(is_typing, call=self.call)

	def on_pager(self, mime_type, body):
		self.member.on_pager(mime_type, body, call=self.call)
	
	def on_pager_status(self, body, im_id, code, reason):
		self.member.on_pager_status(body, im_id, code, reason, call=self.call)

# Buddy callback. This would just forward the event to Member class
class BuddyCb(pj.BuddyCallback):
	def __init__(self, member, buddy=None):
		pj.BuddyCallback.__init__(self, buddy)
		self.member = member
	
	def on_pager(self, mime_type, body):
		self.member.on_pager(mime_type, body, buddy=self.buddy)

	def on_pager_status(self, body, im_id, code, reason):
		self.member.on_pager_status(body, im_id, code, reason, buddy=self.buddy)

	def on_state(self):
		self.member.on_pres_state(self.buddy)

	def on_typing(self, is_typing):
		self.member.on_typing(is_typing, buddy=self.buddy)




##############################################################################
#
#
# This class represents individual room member (either/both chat and voice conf)
#
#
class Member:
	def __init__(self, bot, uri):
		self.uri = uri
		self.bot = bot
		self.call = None
		self.buddy = None
		self.bi = pj.BuddyInfo()
		self.in_chat = False
		self.in_voice = False
		self.im_error = False
		self.html = False
	
	def __str__(self):
		str = string.ljust(self.uri, 30) + " -- " 
		if self.buddy:
			bi = self.buddy.info()
			str = str + bi.online_text
		else:
			str = str + "Offline"
		str = str + " ["
		if (self.in_voice):
			str = str + " voice"
		if (self.in_chat):
			str = str + " chat"
			if (self.html):
				str = str + " html"
			else:
				str = str + " plain"

		if (self.im_error):
			str = str + " im_error"
		str = str + "]"
		return str
		
	def join_call(self, call):
		if self.call:
			self.call.hangup(603, "You have been disconnected for making another call")
		self.call = call
		call.set_callback(CallCb(self, call))
		msg = "%(uri)s is attempting to join the voice conference" % \
			  {'uri': self.uri}
		self.bot.DEBUG(msg + "\n", INFO)
		self.bot.broadcast_pager(None, msg)
	
	def join_chat(self):
		if not self.buddy:
			self.bot.DEBUG(self.uri + " joining chatroom...\n", INFO)
			self.buddy = self.bot.acc.add_buddy(self.uri)
			self.buddy.set_callback(BuddyCb(self, self.buddy))
			self.buddy.subscribe()
		else:
			self.bot.DEBUG(self.uri + " already in chatroom, resubscribing..\n", INFO)
			self.buddy.subscribe()

	def send_pager(self, body, mime="text/plain"):
		self.bot.DEBUG("send_pager() to " + self.uri)
		if self.in_chat and not self.im_error and self.buddy:
			if self.html:
				#This will make us receive html!
				#mime = "text/html"
				body = body.replace("<", "&lt;")
				body = body.replace(">", "&gt;")
				body = body.replace('"', "&quot;")
				body = body.replace("\n", "<BR>\n")
			self.buddy.send_pager(body, content_type=mime)
			self.bot.DEBUG("..sent\n")
		else:
			self.bot.DEBUG("..not sent!\n")
		
	def on_call_state(self, call):
		ci = call.info()
		if ci.state==pj.CallState.DISCONNECTED:
			if self.in_voice:
				msg = "%(uri)s has left the voice conference (%(1)d/%(2)s)" % \
					  {'uri': self.uri, '1': ci.last_code, '2': ci.last_reason}
				self.bot.DEBUG(msg + "\n", INFO)
				self.bot.broadcast_pager(None, msg)
			self.in_voice = False
			self.call = None
			self.bot.on_member_left(self)
		elif ci.state==pj.CallState.CONFIRMED:
			msg = "%(uri)s has joined the voice conference" % \
				  {'uri': self.uri}
			self.bot.DEBUG(msg + "\n", INFO)
			self.bot.broadcast_pager(None, msg)
	
	def on_call_media_state(self, call):
		self.bot.DEBUG("Member.on_call_media_state\n")
		ci = call.info()
		if ci.conf_slot!=-1:
			if not self.in_voice:
				msg = self.uri + " call media is active"
				self.bot.broadcast_pager(None, msg)
			self.in_voice = True
			self.bot.add_to_voice_conf(self)
		else:
			if self.in_voice:
				msg = self.uri + " call media is inactive"
				self.bot.broadcast_pager(None, msg)
			self.in_voice = False

	def on_call_dtmf_digit(self, call, digits):
		msg = "%(uri)s sent DTMF digits %(dig)s" % \
			  {'uri': self.uri, 'dig': digits}
		self.bot.broadcast_pager(None, msg)

	def on_call_transfer_request(self, call, dst, code):
		msg = "%(uri)s is transfering the call to %(dst)s" % \
			  {'uri': self.uri, 'dst': dst}
		self.bot.broadcast_pager(None, msg)
		return 202
	
	def on_call_transfer_status(self, call, code, reason, final, cont):
		msg = "%(uri)s call transfer status is %(code)d/%(res)s" % \
			  {'uri': self.uri, 'code': code, 'res': reason}
		self.bot.broadcast_pager(None, msg)
		return True

	def on_call_replace_request(self, call, code, reason):
		msg = "%(uri)s is requesting call replace" % \
			  {'uri': self.uri}
		self.bot.broadcast_pager(None, msg)
		return (code, reason)
		
	def on_call_replaced(self, call, new_call):
		msg = "%(uri)s call is replaced" % \
			  {'uri': self.uri}
		self.bot.broadcast_pager(None, msg)

	def on_pres_state(self, buddy):
		old_bi = self.bi
		self.bi = buddy.info()
		msg = "%(uri)s status is %(st)s" % \
		  {'uri': self.uri, 'st': self.bi.online_text}
		self.bot.DEBUG(msg + "\n", INFO)
		self.bot.broadcast_pager(self, msg)

		if self.bi.sub_state==pj.SubscriptionState.ACTIVE:
			if not self.in_chat:
				self.in_chat = True
				buddy.send_pager("Welcome to chatroom")
				self.bot.broadcast_pager(self, self.uri + " has joined the chat room")
			else:
				self.in_chat = True
		elif self.bi.sub_state==pj.SubscriptionState.NULL or \
			 self.bi.sub_state==pj.SubscriptionState.TERMINATED or \
			 self.bi.sub_state==pj.SubscriptionState.UNKNOWN:
			self.buddy.delete()
			self.buddy = None
			if self.in_chat:
				self.in_chat = False
				self.bot.broadcast_pager(self, self.uri + " has left the chat room")
			else:
				self.in_chat = False
			self.bot.on_member_left(self)
		
	def on_typing(self, is_typing, call=None, buddy=None):
		if is_typing:
			msg = self.uri + " is typing..."
		else:
			msg = self.uri + " has stopped typing"
		self.bot.broadcast_pager(self, msg)

	def on_pager(self, mime_type, body, call=None, buddy=None):
		if not self.bot.handle_cmd(self, None, body):
			msg = self.uri + ": " + body
			self.bot.broadcast_pager(self, msg, mime_type)
	
	def on_pager_status(self, body, im_id, code, reason, call=None, buddy=None):
		self.im_error = (code/100 != 2)



##############################################################################
#
#
# The Bot instance (singleton)
#
#
class Bot(pj.AccountCallback):
	def __init__(self):
		pj.AccountCallback.__init__(self, None)
		self.lib = pj.Lib()
		self.acc = None
		self.calls = []
		self.members = {}
		self.cfg = None

	def DEBUG(self, msg, level=TRACE):
		print msg,
		
	def helpstring(self):
		return """
--h[elp]            Display this help screen
--j[oin]            Join the chat room
--html on|off       Set to receive HTML or plain text

Participant commands:
--s[how]            Show confbot settings
--leave             Leave the chatroom
--l[ist]            List all members

Admin commands:
--a[dmin] <CMD> Where <CMD> are:
    list            List the admins
    add <URI>       Add URI as admin
    del <URI>       Remove URI as admin
    rr              Reregister account to server
    call <URI>      Make call to the URI and add to voice conf
    dc <URI>        Disconnect call to URI
    hold <URI>      Hold call with that URI
    update <URI>    Send UPDATE to call with that URI
    reinvite <URI>  Send re-INVITE to call with that URI
"""

	def listmembers(self):
		msg = ""
		for uri, m in self.members.iteritems():
			msg = msg + str(m) + "\n"
		return msg
	
	def showsettings(self):
		ai = self.acc.info()
		msg = """
ConfBot status and settings:
  URI:        %(uri)s
  Status:     %(pres)s
  Reg Status: %(reg_st)d
  Reg Reason: %(reg_res)s
""" % {'uri': ai.uri, 'pres': ai.online_text, \
	   'reg_st': ai.reg_status, 'reg_res': ai.reg_reason}
		return msg
  
	def main(self, cfg_file):
		try:
			cfg = self.cfg = __import__(cfg_file)
			
			self.lib.init(ua_cfg=cfg.ua_cfg, log_cfg=cfg.log_cfg, media_cfg=cfg.media_cfg)
			self.lib.set_null_snd_dev()
			
			transport = None
			if cfg.udp_cfg:
				transport = self.lib.create_transport(pj.TransportType.UDP, cfg.udp_cfg)
			if cfg.tcp_cfg:
				t = self.lib.create_transport(pj.TransportType.TCP, cfg.tcp_cfg)
				if not transport:
					transport = t
				
			self.lib.start()
			
			if cfg.acc_cfg:
				self.DEBUG("Creating account %(uri)s..\n" % {'uri': cfg.acc_cfg.id}, INFO)
				self.acc = self.lib.create_account(cfg.acc_cfg, cb=self)
			else:
				self.DEBUG("Creating account for %(t)s..\n" % \
							{'t': transport.info().description}, INFO)
				self.acc = self.lib.create_account_for_transport(transport, cb=self)
			
			self.acc.set_basic_status(True)
			
			# Wait for ENTER before quitting
			print "Press q to quit or --help/--h for help"
			while True:
				input = sys.stdin.readline().strip(" \t\r\n")
				if not self.handle_cmd(None, None, input):
					if input=="q":
						break
			
			self.lib.destroy()
			self.lib = None

		except pj.Error, e:
			print "Exception: " + str(e)
			if self.lib:
				self.lib.destroy()
				self.lib = None
	
	def broadcast_pager(self, exclude_member, body, mime_type="text/plain"):
		self.DEBUG("Broadcast: " + body + "\n")
		for uri, m in self.members.iteritems():
			if m != exclude_member:
				m.send_pager(body, mime_type)

	def add_to_voice_conf(self, member):
		if not member.call:
			return
		src_ci = member.call.info()
		self.DEBUG("bot.add_to_voice_conf\n")
		for uri, m in self.members.iteritems():
			if m==member:
				continue
			if not m.call:
				continue
			dst_ci = m.call.info()
			if dst_ci.media_state==pj.MediaState.ACTIVE and dst_ci.conf_slot!=-1:
				self.lib.conf_connect(src_ci.conf_slot, dst_ci.conf_slot)
				self.lib.conf_connect(dst_ci.conf_slot, src_ci.conf_slot)
	
	def on_member_left(self, member):
		if not member.call and not member.buddy:
			del self.members[member.uri]
			del member
			
	def handle_admin_cmd(self, member, body):
		if member and self.cfg.admins and not member.uri in self.cfg.admins:
			member.send_pager("You are not admin")
			return
		args = body.split()
		msg = ""

		if len(args)==1:
			args.append(" ")

		if args[1]=="list":
			if not self.cfg.admins:
				msg = "Everyone is admin!"
			else:
				msg = str(self.cfg.admins)
		elif args[1]=="add":
			if len(args)!=3:
				msg = "Usage: add <URI>"
			else:
				self.cfg.admins.append(args[2])
				msg = args[2] + " added as admin"
		elif args[1]=="del":
			if len(args)!=3:
				msg = "Usage: del <URI>"
			elif args[2] not in self.cfg.admins:
				msg = args[2] + " is not admin"
			else:
				self.cfg.admins.remove(args[2])
				msg = args[2] + " has been removed from admins"
		elif args[1]=="rr":
			msg = "Reregistering.."
			self.acc.set_registration(True)
		elif args[1]=="call":
			if len(args)!=3:
				msg = "Usage: call <URI>"
			else:
				uri = args[2]
				try:
					call = self.acc.make_call(uri)
				except pj.Error, e:
					msg = "Error: " + str(e)
					call = None

				if call:
					if not uri in self.members:
						m = Member(self, uri)
						self.members[m.uri] = m
					else:
						m = self.members[uri]
					msg = "Adding " + m.uri + " to voice conference.."
					m.join_call(call)
		elif args[1]=="dc" or args[1]=="hold" or args[1]=="update" or args[1]=="reinvite":
			if len(args)!=3:
				msg = "Usage: " + args[1] + " <URI>"
			else:
				uri = args[2]
				if not uri in self.members:
					msg = "Member not found/URI doesn't match (note: case matters!)"
				else:
					m = self.members[uri]
					if m.call:
						if args[1]=="dc":
							msg = "Disconnecting.."
							m.call.hangup(603, "You're disconnected by admin")
						elif args[1]=="hold":
							msg = "Holding the call"
							m.call.hold()
						elif args[1]=="update":
							msg = "Sending UPDATE"
							m.call.update()
						elif args[1]=="reinvite":
							msg = "Sending re-INVITE"
							m.call.reinvite()
					else:
						msg = "He is not in call"
		else:
			msg = "Unknown admin command " + body

		#print "msg is '%(msg)s'" % {'msg': msg}

		if True:
			if member:
				member.send_pager(msg)
			else:
				print msg

	def handle_cmd(self, member, from_uri, body):
		body = body.strip(" \t\r\n")
		msg = ""
		handled = True
		if body=="--l" or body=="--list":
			msg = self.listmembers()
			if msg=="":
				msg = "Nobody is here"
		elif body[0:3]=="--s":
			msg = self.showsettings()
		elif body[0:6]=="--html" and member:
			if body[8:11]=="off":
				member.html = False
			else:
				member.html = True
		elif body=="--h" or body=="--help":
			msg = self.helpstring()
		elif body=="--leave":
			if not member or not member.buddy:
				msg = "You are not in chatroom"
			else:
				member.buddy.unsubscribe()
		elif body[0:3]=="--j":
			if not from_uri in self.members:
				m = Member(self, from_uri)
				self.members[m.uri] = m
				self.DEBUG("Adding " + m.uri + " to chatroom\n")
				m.join_chat()
			else:
				m = self.members[from_uri]
				self.DEBUG("Adding " + m.uri + " to chatroom\n")
				m.join_chat()
		elif body[0:3]=="--a":
			self.handle_admin_cmd(member, body)
			handled = True
		else:
			handled = False

		if msg:
			if member:
				member.send_pager(msg)
			elif from_uri:
				self.acc.send_pager(from_uri, msg);
			else:
				print msg
		return handled
	
	def on_incoming_call(self, call):
		self.DEBUG("on_incoming_call from %(uri)s\n" % {'uri': call.info().remote_uri}, INFO)
		ci = call.info()
		if not ci.remote_uri in self.members:
			m = Member(self, ci.remote_uri)
			self.members[m.uri] = m
			m.join_call(call)
		else:
			m = self.members[ci.remote_uri]
			m.join_call(call)
		call.answer(200)
	
	def on_incoming_subscribe(self, buddy, from_uri, contact_uri, pres_obj):
		self.DEBUG("on_incoming_subscribe from %(uri)s\n" % from_uri, INFO)
		return (200, 'OK')

	def on_reg_state(self):
		ai = self.acc.info()
		self.DEBUG("Registration state: %(code)d/%(reason)s\n" % \
					{'code': ai.reg_status, 'reason': ai.reg_reason}, INFO)
		if ai.reg_status/100==2 and ai.reg_expires > 0:
			self.acc.set_basic_status(True)

	def on_pager(self, from_uri, contact, mime_type, body):
		body = body.strip(" \t\r\n")
		if not self.handle_cmd(None, from_uri, body):
			self.acc.send_pager(from_uri, "You have not joined the chat room. Type '--join' to join or '--help' for the help")

	def on_pager_status(self, to_uri, body, im_id, code, reason):
		pass

	def on_typing(self, from_uri, contact, is_typing):
		pass




##############################################################################
#
#
# main()
#
#
if __name__ == "__main__":
	bot = Bot()
	bot.main(CFG_FILE)

