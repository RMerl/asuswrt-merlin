# $Id: 100_resample_lf_8_22.py 2052 2008-06-25 18:18:32Z nanang $
#
from inc_cfg import *

# simple test
test_param = TestParam(
		"Resample (large filter) 8 KHZ to 22 KHZ",
		[
			InstanceParam("endpt", "--null-audio --quality 10 --clock-rate 22050 --play-file wavs/input.8.wav --rec-file wavs/tmp.22.wav")
		]
		)
