# $Id: 200_codec_g722.py 2063 2008-06-26 18:52:16Z nanang $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with G722 codec
test_param = TestParam(
		"PESQ codec G722",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --add-codec g722 --clock-rate 16000 --play-file wavs/input.16.wav"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --add-codec g722 --clock-rate 16000 --rec-file  wavs/tmp.16.wav --auto-answer 200")
		]
		)

pesq_threshold = 3.7
