# Tencent QQ Protocol - Chinese instant messenger protocol - http://www.qq.com
# Pattern attributes: good notsofast fast
# Protocol groups: chat
# Wiki: http://www.protocolinfo.org/wiki/QQ
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# Over six million people use QQ in China, according to wsgtrsys.
# 
# This pattern has been tested and is believed to work well.
#
# QQ uses three (two?) methods to connect to server(s?).
# one is udp, and another is tcp
# udp protocol: the first byte is 02 and last byte is 03
# tcp protocol: the second byte is 02 and last byte is 03
#   tony on protocolinfo.org says that now the *third* byte is 02:
#     "but when I tested on my PC, I found that when qq2007/qq2008 
#     use tcp protocol, the third byte instead of the second is always 02.
#
#     So the QQ protocol changed again, or I have made a mistake, I wonder 
#     that."
#   So now the pattern allows any of the first three bytes to be 02.  Delete
#   one of the ".?" to restore to the old behaviour.
# pattern written by www.routerclub.com wsgtrsys

qq
^.?.?\x02.+\x03$
