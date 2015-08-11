# $Id: 150_srtp_0_1.py 2392 2008-12-22 18:54:58Z bennylp $
#
from inc_cfg import *

test_param= TestParam(
		"Callee=no SRTP, caller=optional SRTP",
		[
			InstanceParam("callee", "--null-audio --max-calls=1"),
			InstanceParam("caller", "--null-audio --use-srtp=1 --srtp-secure=0 --max-calls=1")
		]
		)
