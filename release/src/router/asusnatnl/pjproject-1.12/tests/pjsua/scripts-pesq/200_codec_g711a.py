# $Id: 200_codec_g711a.py 2063 2008-06-26 18:52:16Z nanang $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with PCMA codec
test_param = TestParam(
		"PESQ codec PCMA",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --add-codec pcma --clock-rate 8000 --play-file wavs/input.8.wav"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --add-codec pcma --clock-rate 8000 --rec-file  wavs/tmp.8.wav --auto-answer 200")
		]
		)

pesq_threshold = 3.5
