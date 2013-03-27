# hddtemp - Hard drive temperature reporting
# Pattern attributes: great veryfast fast
# Protocol groups: monitoring open_source
# Wiki: http://www.protocolinfo.org/wiki/HDDtemp
# 
# Usually runs on port 7634
#
# You're a silly person if you use this pattern.
#
# This pattern has been tested and is believed to work well.

hddtemp
^\|/dev/[a-z][a-z][a-z]\|[0-9a-z]*\|[0-9][0-9]\|[cfk]\|
