# $Id: 100_defaults.py 3286 2010-08-18 14:30:15Z bennylp $
#
from inc_cfg import *

ADD_PARAM = ""

if (HAS_SND_DEV == 0):
	ADD_PARAM += "--null-audio"

# Call with default pjsua settings
test_param = TestParam(
		"PESQ defaults pjsua settings",
		[
			InstanceParam("UA1", ADD_PARAM + " --max-calls=1 --play-file wavs/input.16.wav --no-vad"),
			InstanceParam("UA2", "--null-audio --max-calls=1 --rec-file  wavs/tmp.16.wav --clock-rate 16000 --auto-answer 200")
		]
		)

pesq_threshold = 3.8
