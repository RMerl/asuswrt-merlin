# $Id: 200_tcp.py 2028 2008-06-16 13:04:44Z bennylp $
#
from inc_cfg import *

# TCP call
test_param = TestParam(
		"TCP transport",
		[
			InstanceParam("callee", "--null-audio --no-udp --max-calls=1", uri_param=";transport=tcp"),
			InstanceParam("caller", "--null-audio --no-udp --max-calls=1")
		]
		)
