# $Id: 100_resample_lf_11_8.py 2052 2008-06-25 18:18:32Z nanang $
#
from inc_cfg import *

# simple test
test_param = TestParam(
		"Resample (large filter) 11 KHZ to 8 KHZ",
		[
			InstanceParam("endpt", "--null-audio --quality 10 --clock-rate 8000 --play-file wavs/input.11.wav --rec-file wavs/tmp.8.wav")
		]
		)
