# $Id: 200_register.py 2028 2008-06-16 13:04:44Z bennylp $
#
from inc_cfg import *

# Basic registration
test_param = TestParam(
		"Basic registration",
		[
			InstanceParam(	"client", 
					"--null-audio"+
						" --id=\"<sip:test1@pjsip.org>\""+
						" --registrar=sip:sip.pjsip.org" +
						" --username=test1" +
						" --password=test1" +
						" --realm=*",
					uri="sip:test1@pjsip.org",
					have_reg=True),
		]
		)

