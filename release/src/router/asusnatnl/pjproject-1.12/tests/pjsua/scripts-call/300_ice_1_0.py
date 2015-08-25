# $Id: 300_ice_1_0.py 2025 2008-06-15 19:43:43Z bennylp $
#
from inc_cfg import *
 
# ICE mismatch
test_param = TestParam(
		"Callee=use ICE, caller=no ICE",
		[
			InstanceParam("callee", "--null-audio --use-ice --max-calls=1"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
