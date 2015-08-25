# $Id: 300_ice_1_1.py 2084 2008-06-27 23:53:00Z bennylp $
#
from inc_cfg import *

# ICE mismatch
test_param = TestParam(
		"Callee=use ICE, caller=use ICE",
		[
			InstanceParam("callee", "--null-audio --use-ice --max-calls=1", enable_buffer=True),
			InstanceParam("caller", "--null-audio --use-ice --max-calls=1", enable_buffer=True)
		]
		)
