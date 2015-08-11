# $Id: 100_resample_lf_8_11.py 2052 2008-06-25 18:18:32Z nanang $
#
from inc_cfg import *

# simple test
test_param = TestParam(
		"Resample (large filter) 8 KHZ to 11 KHZ",
		[
			InstanceParam("endpt", "--null-audio --quality 10 --clock-rate 11025 --play-file wavs/input.8.wav --rec-file wavs/tmp.11.wav")
		]
		)
