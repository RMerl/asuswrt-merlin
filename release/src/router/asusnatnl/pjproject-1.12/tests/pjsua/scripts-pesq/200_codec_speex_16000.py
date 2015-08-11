# $Id: 200_codec_speex_16000.py 3286 2010-08-18 14:30:15Z bennylp $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with Speex/16000 codec
test_param = TestParam(
		"PESQ codec Speex WB",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --clock-rate 16000 --add-codec speex/16000 --play-file wavs/input.16.wav --no-vad"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --clock-rate 16000 --add-codec speex/16000 --rec-file  wavs/tmp.16.wav --auto-answer 200")
		]
		)

pesq_threshold = 3.8
