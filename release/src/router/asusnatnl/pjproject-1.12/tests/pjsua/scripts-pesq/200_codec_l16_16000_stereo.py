# $Id: 200_codec_l16_16000_stereo.py 2075 2008-06-27 16:18:13Z nanang $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with L16/16000/2 codec
test_param = TestParam(
		"PESQ defaults pjsua settings",
		[
			InstanceParam("UA1", ADD_PARAM + " --stereo --max-calls=1 --clock-rate 16000 --add-codec L16/16000/2 --play-file wavs/input.2.16.wav"),
			InstanceParam("UA2", "--null-audio --stereo --max-calls=1 --clock-rate 16000 --add-codec L16/16000/2 --rec-file  wavs/tmp.2.16.wav   --auto-answer 200")
		]
		)

pesq_threshold = None
