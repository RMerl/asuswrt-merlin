# $Id: 240_publish_scenarios.py 2661 2009-04-28 22:19:49Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

# Several PUBLISH failure scenarios that should be handled automatically


pjsua = "--null-audio --id=sip:127.0.0.1:$PORT --registrar sip:127.0.0.1:$PORT " + \
	"--realm=python --user=username --password=password " + \
	"--auto-update-nat=0 --publish"
#pjsua = "--null-audio --local-port 0 --rtp-port 0"

# Handle REGISTER first
req1 = sip.RecvfromTransaction("Initial REGISTER", 200,
				include=["REGISTER sip"], 
				exclude=[],
				resp_hdr=["Expires: 1800"]
			  	)

# First PUBLISH, reply with 412
req2 = sip.RecvfromTransaction("Initial PUBLISH, will be replied with 412", 412,
				include=["PUBLISH sip"], 
				exclude=["Expires:"]
			  	)

# Second PUBLISH
req3 = sip.RecvfromTransaction("Second PUBLISH, will be replied with 200", 200,
				include=["PUBLISH sip"], 
				exclude=["Expires:"],
				resp_hdr=["Expires: 60", "SIP-ETag: dx200xyz"]
			  	)

# PUBLISH refresh, respond with 408
req4 = sip.RecvfromTransaction("PUBLISH refresh, will be replied with 408", 408,
				include=["PUBLISH sip", "SIP-If-Match: dx200xyz"], 
				exclude=["Expires:"],
				resp_hdr=["Expires: 60", "SIP-ETag: dx200xyz"]
			  	)

# After 5 minutes, pjsua should retry again
req5 = sip.RecvfromTransaction("PUBLISH retry", 200,
				include=["PUBLISH sip"], 
				exclude=["Expires:", "SIP-If-Match:"],
				resp_hdr=["Expires: 60", "SIP-ETag: abc"]
			  	)



recvfrom_cfg = sip.RecvfromCfg("PUBLISH scenarios",
			       pjsua, [req1, req2, req3])

