# Load in the ip_set kernel module
0 modprobe ip_set
# List our test set: the testsuite fails if it exists
1 ipset -L test >/dev/null
# Delete our test set: the testsuite fails if it exists
1 ipset -X test
# Check mandatory create arguments
2 ipset -N test
# eof
