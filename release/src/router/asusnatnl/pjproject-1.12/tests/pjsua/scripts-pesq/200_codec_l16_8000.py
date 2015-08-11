# $Id: 200_codec_l16_8000.py 2075 2008-06-27 16:18:13Z nanang $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with L16/8000/1 codec
test_param = TestParam(
		"PESQ codec L16/8000/1",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --add-codec L16/8000/1 --clock-rate 8000 --play-file wavs/input.8.wav"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --add-codec L16/8000/1 --clock-rate 8000 --rec-file  wavs/tmp.8.wav --auto-answer 200")
		]
		)

pesq_threshold = 3.5
