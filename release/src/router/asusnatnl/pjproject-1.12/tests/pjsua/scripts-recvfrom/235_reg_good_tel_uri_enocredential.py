# $Id: 235_reg_good_tel_uri_enocredential.py 3323 2010-09-28 07:43:18Z bennylp $
import inc_sip as sip
import inc_sdp as sdp

pjsua = "--null-audio --id=tel:+12345 --registrar sip:127.0.0.1:$PORT"

req1 = sip.RecvfromTransaction("", 401,
				include=["REGISTER sip"], 
				exclude=["Authorization"],
				resp_hdr=["WWW-Authenticate: Digest realm=\"python\", nonce=\"1234\""],
				expect="PJSIP_ENOCREDENTIAL"
			  )

recvfrom_cfg = sip.RecvfromCfg("Failed registration with tel: URI test",
			       pjsua, [req1])
