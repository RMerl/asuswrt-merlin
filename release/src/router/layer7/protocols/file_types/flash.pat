# Flash - Macromedia Flash.  
# Pattern attributes: good slow notsofast subset
# Protocol groups: file

# Copyright (C) 2008 Matthew Strait, Ethan Sommer; See ../LICENSE
# Thanks to Brandon Enright {bmenrigh AT ucsd.edu} and chinalantian at 
# 126 dot com

# Macromedia spec:
# http://download.macromedia.com/pub/flash/flash_file_format_specification.pdf
# See also:
# http://www.digitalpreservation.gov/formats/fdd/fdd000130.shtml
# http://osflash.org/flv

flash
# FWS = uncompressed, CWS = compressed, next byte is version number
# FLV = video 
[FC]WS[\x01-\x09]|FLV\x01\x05\x09
