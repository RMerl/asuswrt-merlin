# $Id: 201_codec_g722.py 2063 2008-06-26 18:52:16Z nanang $
#
from inc_cfg import *

# Call with G722 codec
test_param = TestParam(
		"PESQ codec G722 (RX side uses snd dev)",
		[
			InstanceParam("UA1", "--max-calls=1 --add-codec g722 --clock-rate 16000 --play-file wavs/input.16.wav --null-audio"),
			InstanceParam("UA2", "--max-calls=1 --add-codec g722 --clock-rate 16000 --rec-file  wavs/tmp.16.wav   --auto-answer 200")
		]
		)

if (HAS_SND_DEV == 0):
	test_param.skip = True

pesq_threshold = 3.7
