# $Id: 100_peertopeer.py 2392 2008-12-22 18:54:58Z bennylp $
#
from inc_cfg import *

# Direct peer to peer presence
test_param = TestParam(
		"Direct peer to peer presence",
		[
			InstanceParam("client1", "--null-audio"),
			InstanceParam("client2", "--null-audio")
		]
		)
