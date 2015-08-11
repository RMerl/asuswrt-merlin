# $Id: 200_publish.py 2028 2008-06-16 13:04:44Z bennylp $
#
from inc_cfg import *

# Basic registration
test_param = TestParam(
		"Presence with PUBLISH",
		[
			InstanceParam(	"ua1", 
					"--null-audio"+
						" --id=\"<sip:test1@pjsip.org>\""+
						" --registrar=sip:sip.pjsip.org" +
						" --username=test1" +
						" --password=test1" +
						" --realm=*" +
						" --proxy=\"sip:sip.pjsip.org;lr\"" +
						" --publish",
					uri="<sip:test1@pjsip.org>",
					have_reg=True,
					have_publish=True),
			InstanceParam(	"ua2", 
					"--null-audio"+
						" --id=\"<sip:test2@pjsip.org>\""+
						" --registrar=sip:sip.pjsip.org" +
						" --username=test2" +
						" --password=test2" +
						" --realm=*" +
						" --proxy=\"sip:sip.pjsip.org;lr\"" +
						" --publish",
					uri="<sip:test2@pjsip.org>",
					have_reg=True,
					have_publish=True),
		]
		)

