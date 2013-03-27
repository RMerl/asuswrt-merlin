# Tencent QQ Protocol - Chinese instant messenger protocol - http://www.qq.com
# Pattern attributes: good fast fast
# Protocol groups: chat
# Wiki: http://www.protocolinfo.org/wiki/QQ
#
# Over six million people use QQ in China, according to wsgtrsys.
# 
# This pattern has been tested and is believed to work well.
#
# QQ uses three (two?) methods to connect to server(s?).
# one is udp, and another is tcp
# udp protocol: the first byte is 02 and last byte is 03
# tcp protocol: the second byte is 02 and last byte is 03
# pattern written by www.routerclub.com wsgtrsys

qq
^.?\x02.+\x03$

