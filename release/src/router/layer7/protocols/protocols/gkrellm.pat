# Gkrellm - a system monitor - http://gkrellm.net
# Pattern attributes: great veryfast fast
# Protocol groups: monitoring open_source
# Wiki: http://www.protocolinfo.org/wiki/Gkrellm
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
# 
# This pattern has been tested and is believed to work well.
# Since this is not anything resembling a published protocol, it may change without
# warning in new versions of gkrellm.

gkrellm
# tested with gkrellm 2.2.7
^gkrellm [23].[0-9].[0-9]\x0a$
