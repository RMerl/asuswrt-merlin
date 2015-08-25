# $Id: 150_srtp_3_2.py 3334 2010-10-05 16:32:04Z nanang $
#
from inc_cfg import *

test_param = TestParam(
		"Callee=optional (with duplicated offer) SRTP, caller=mandatory SRTP",
		[
			InstanceParam("callee", "--null-audio --use-srtp=3 --srtp-secure=0 --max-calls=1"),
			InstanceParam("caller", "--null-audio --use-srtp=2 --srtp-secure=0 --max-calls=1")
		]
		)
