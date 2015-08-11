# $Id: 400_tel_uri.py 3323 2010-09-28 07:43:18Z bennylp $
#
from inc_cfg import *

# Simple call
test_param = TestParam(
		"tel: URI in From",
		[
			InstanceParam("callee", "--null-audio --max-calls=1 --id tel:+111"),
			InstanceParam("caller", "--null-audio --max-calls=1")
		]
		)
