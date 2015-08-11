# $Id: 100_simple.py 2028 2008-06-16 13:04:44Z bennylp $
#
# Just about the simple pjsua command line parameter, which should
# never fail in any circumstances
from inc_cfg import *

test_param = TestParam(
		"Basic run", 
		[
			InstanceParam("pjsua", "--null-audio --rtp-port 0")
		]
		)

