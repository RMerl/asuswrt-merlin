# $Id: 201_codec_l16_8000_stereo.py 2075 2008-06-27 16:18:13Z nanang $
#
from inc_cfg import *

# Call with L16/8000/2 codec
test_param = TestParam(
		"PESQ defaults pjsua settings",
		[
			InstanceParam("UA1", "--stereo --max-calls=1 --clock-rate 8000 --add-codec L16/8000/2 --play-file wavs/input.2.8.wav --null-audio"),
			InstanceParam("UA2", "--stereo --max-calls=1 --clock-rate 8000 --add-codec L16/8000/2 --rec-file  wavs/tmp.2.8.wav   --auto-answer 200")
		]
		)

if (HAS_SND_DEV == 0):
	test_param.skip = True
	
pesq_threshold = None
