# $Id: 200_codec_speex_8000.py 3286 2010-08-18 14:30:15Z bennylp $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with Speex/8000 codec
test_param = TestParam(
		"PESQ codec Speex NB",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --add-codec speex/8000 --clock-rate 8000 --play-file wavs/input.8.wav --no-vad"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --add-codec speex/8000 --clock-rate 8000 --rec-file  wavs/tmp.8.wav --auto-answer 200")
		]
		)

pesq_threshold = 3.65
