# $Id: presence.py 2171 2008-07-24 09:01:33Z bennylp $
#
# Presence and instant messaging
#
# Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
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
import sys
import pjsua as pj

LOG_LEVEL = 3
pending_pres = None
pending_uri = None

def log_cb(level, str, len):
    print str,

class MyAccountCallback(pj.AccountCallback):
    def __init__(self, account=None):
        pj.AccountCallback.__init__(self, account)

    def on_incoming_subscribe(self, buddy, from_uri, contact_uri, pres):
        global pending_pres, pending_uri
        # Allow buddy to subscribe to our presence
        if buddy:
            return (200, None)
        print 'Incoming SUBSCRIBE request from', from_uri
        print 'Press "A" to accept and add, "R" to reject the request'
        pending_pres = pres
        pending_uri = from_uri
        return (202, None)


class MyBuddyCallback(pj.BuddyCallback):
    def __init__(self, buddy=None):
        pj.BuddyCallback.__init__(self, buddy)

    def on_state(self):
        print "Buddy", self.buddy.info().uri, "is",
        print self.buddy.info().online_text

    def on_pager(self, mime_type, body):
        print "Instant message from", self.buddy.info().uri, 
        print "(", mime_type, "):"
        print body

    def on_pager_status(self, body, im_id, code, reason):
        if code >= 300:
            print "Message delivery failed for message",
            print body, "to", self.buddy.info().uri, ":", reason

    def on_typing(self, is_typing):
        if is_typing:
            print self.buddy.info().uri, "is typing"
        else:
            print self.buddy.info().uri, "stops typing"


lib = pj.Lib()

try:
    # Init library with default config and some customized
    # logging config.
    lib.init(log_cfg = pj.LogConfig(level=LOG_LEVEL, callback=log_cb))

    # Create UDP transport which listens to any available port
    transport = lib.create_transport(pj.TransportType.UDP, 
                                     pj.TransportConfig(0))
    print "\nListening on", transport.info().host, 
    print "port", transport.info().port, "\n"
    
    # Start the library
    lib.start()

    # Create local account
    acc = lib.create_account_for_transport(transport, cb=MyAccountCallback())
    acc.set_basic_status(True)
    
    my_sip_uri = "sip:" + transport.info().host + \
                 ":" + str(transport.info().port)

    buddy = None

    # Menu loop
    while True:
        print "My SIP URI is", my_sip_uri
        print "Menu:  a=add buddy, d=delete buddy, t=toggle", \
              " online status, i=send IM, q=quit"

        input = sys.stdin.readline().rstrip("\r\n")
        if input == "a":
            # Add buddy
            print "Enter buddy URI: ", 
            input = sys.stdin.readline().rstrip("\r\n")
            if input == "":
                continue

            buddy = acc.add_buddy(input, cb=MyBuddyCallback())
            buddy.subscribe()

        elif input == "t":
            acc.set_basic_status(not acc.info().online_status)

        elif input == "i":
            if not buddy:
                print "Add buddy first"
                continue

            buddy.send_typing_ind(True)

            print "Type the message: ", 
            input = sys.stdin.readline().rstrip("\r\n")
            if input == "":
                buddy.send_typing_ind(False)
                continue
            
            buddy.send_pager(input)
        
        elif input == "d":
            if buddy:
                buddy.delete()
                buddy = None
            else:
                print 'No buddy was added'

        elif input == "A":
            if pending_pres:
                acc.pres_notify(pending_pres, pj.SubscriptionState.ACTIVE)
                buddy = acc.add_buddy(pending_uri, cb=MyBuddyCallback())
                buddy.subscribe()
                pending_pres = None
                pending_uri = None
            else:
                print "No pending request"

        elif input == "R":
            if pending_pres:
                acc.pres_notify(pending_pres, pj.SubscriptionState.TERMINATED,
                                "rejected")
                pending_pres = None
                pending_uri = None
            else:
                print "No pending request"

        elif input == "q":
            break

    # Shutdown the library
    acc.delete()
    acc = None
    if pending_pres:
        acc.pres_notify(pending_pres, pj.SubscriptionState.TERMINATED,
                        "rejected")
    transport = None
    lib.destroy()
    lib = None

except pj.Error, e:
    print "Exception: " + str(e)
    lib.destroy()
    lib = None

