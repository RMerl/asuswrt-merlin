# $Id: 101_defaults.py 2063 2008-06-26 18:52:16Z nanang $
#
from inc_cfg import *

# Call with default pjsua settings
test_param = TestParam(
		"PESQ defaults pjsua settings (RX side uses snd dev)",
		[
			InstanceParam("UA1", "--max-calls=1 --play-file wavs/input.16.wav --null-audio"),
			InstanceParam("UA2", "--max-calls=1 --rec-file  wavs/tmp.16.wav --clock-rate 16000 --auto-answer 200")
		]
		)


if (HAS_SND_DEV == 0):
	test_param.skip = True

pesq_threshold = None
