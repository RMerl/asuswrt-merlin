# $Id: 100_resample_lf_8_48.py 2052 2008-06-25 18:18:32Z nanang $
#
from inc_cfg import *

# simple test
test_param = TestParam(
		"Resample (large filter) 8 KHZ to 48 KHZ",
		[
			InstanceParam("endpt", "--null-audio --quality 10 --clock-rate 48000 --play-file wavs/input.8.wav --rec-file wavs/tmp.48.wav")
		]
		)
