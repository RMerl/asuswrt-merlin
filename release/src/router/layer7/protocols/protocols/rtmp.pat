# Adobe Real Time Messaging Protocol(RTMP). By Jonathan A.P. Marpaung
# Pattern attributes: works very fast
# Protocol Groups: streaming_video streaming_audio
# The RTMP Specification is availabe at 
# http://www.adobe.com/devnet/rtmp/pdf/rtmp_specification_1.0.pdf [^]
#
# First 12 bytes, starting at \x03 are the RTMP header. Next 25 bytes,
# starting at \x02, are part of the RTMP body which is an AMF Object.
# The first string "connect" is a command of the NetConnection class object.
# The next string "app" is a Command Object which is followed by values
# such as "video", . 
rtmp
^\x03.+\x14.+\x02.+\x07.(connect)?.+(app)?
