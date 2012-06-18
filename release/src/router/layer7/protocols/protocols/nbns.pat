# NBNS - NetBIOS name service
# Pattern attributes: good slow notsofast
# Protocol groups: networking proprietary
# Wiki: http://www.protocolinfo.org/wiki/NBNS
# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
#
# This pattern has been tested and is believed to work well.
#
# name query
# \x01\x10 means name query
#
# registration NB
# (\x10 or )\x10 means registration
#
# release NB (merged with registration)
# 0\x10 means release

nbns
# This is not a valid basic GNU regular expression.
\x01\x10\x01|\)\x10\x01\x01|0\x10\x01
