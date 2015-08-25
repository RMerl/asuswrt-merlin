# $Id: 350_prack_a.py 2392 2008-12-22 18:54:58Z bennylp $
#
from inc_cfg import *

# TCP call
test_param = TestParam(
		"Callee requires PRACK",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --use-100rel"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
