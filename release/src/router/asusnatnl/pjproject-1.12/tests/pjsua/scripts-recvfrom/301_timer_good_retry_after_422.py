# $Id: 301_timer_good_retry_after_422.py 3287 2010-08-18 14:30:17Z nanang $
import inc_sip as sip
import inc_sdp as sdp

# Session timers retry after 422


pjsua = "--null-audio sip:127.0.0.1:$PORT --timer-min-se 100 --timer-se 1000"

# First INVITE with timer rejected with 422
req1 = sip.RecvfromTransaction("INVITE with SE too small", 422,
				include=["Session-Expires:\s*1000"], 
				exclude=[],
				resp_hdr=["Min-SE: 2000"]
			  	)

# Wait for ACK
req2 = sip.RecvfromTransaction("Wait ACK", 0, include=["ACK sip"])

# New INVITE with SE >= Min-SE
req3 = sip.RecvfromTransaction("Retrying with acceptable SE", 200,
				include=["Session-Expires:\s*2000", "Min-SE:\s*2000"], 
				exclude=[],
				resp_hdr=["Session-Expires: 2000;refresher=uac"]
			  	)


recvfrom_cfg = sip.RecvfromCfg("Session timers retry after 422",
			       pjsua, [req1, req2, req3])

