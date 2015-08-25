# $Id: 100_resample_lf_8_44.py 2052 2008-06-25 18:18:32Z nanang $
#
from inc_cfg import *

# simple test
test_param = TestParam(
		"Resample (large filter) 8 KHZ to 44 KHZ",
		[
			InstanceParam("endpt", "--null-audio --quality 10 --clock-rate 44100 --play-file wavs/input.8.wav --rec-file wavs/tmp.44.wav")
		]
		)
