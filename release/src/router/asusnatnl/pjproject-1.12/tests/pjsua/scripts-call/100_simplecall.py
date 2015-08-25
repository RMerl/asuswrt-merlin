# $Id: 100_simplecall.py 2392 2008-12-22 18:54:58Z bennylp $
#
from inc_cfg import *

# Simple call
test_param = TestParam(
		"Basic call",
		[
			InstanceParam("callee", "--null-audio --max-calls=1"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
