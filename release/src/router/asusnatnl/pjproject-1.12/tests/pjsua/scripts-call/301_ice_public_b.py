# $Id: 301_ice_public_b.py 2392 2008-12-22 18:54:58Z bennylp $
#
from inc_cfg import *

# This test:
#   to make call with ICE but without STUN.

# Note:
#	- need --dis-codec to make INVITE packet less than typical MTU
uas_args = "--null-audio --id=\"<sip:test1@pjsip.org>\" --registrar=sip:sip.pjsip.org --username=test1 --password=test1  --realm=pjsip.org  --proxy=\"sip:sip.pjsip.org;lr\"  --rtp-port 0 --use-ice --use-compact-form --max-calls 1 --dis-codec=i --dis-codec=s --dis-codec=g --log-file callee.log"

uac_args = "--null-audio --id=\"<sip:test2@pjsip.org>\" --registrar=sip:sip.pjsip.org --username=test2 --password=test2 --realm=pjsip.org --proxy=\"sip:sip.pjsip.org;lr\" --rtp-port 0 --use-ice --use-compact-form --max-calls 1 --dis-codec=i --dis-codec=s --dis-codec=g --log-file caller.log"

test_param = TestParam(
		"ICE via public internet with no STUN",
		[
			InstanceParam(	"callee", uas_args, 
					uri="<sip:test1@pjsip.org>",
					have_reg=True, have_publish=False),
			InstanceParam(	"caller", uac_args,
					uri="<sip:test2@pjsip.org>",
					have_reg=True, have_publish=False),
		]
		)

