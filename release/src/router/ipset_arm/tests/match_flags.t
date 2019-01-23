# Create sets and inet rules which call set match
0 ./iptables.sh inet start_flags
# Send probe packet from 10.0.0.0,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.0.0 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched with --return-nomatch
0 ./check_klog.sh 10.0.0.0 tcp 1025 test-nomatch
# Send probe packet from 10.0.0.1,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.0.1 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched
0 ./check_klog.sh 10.0.0.1 tcp 1025 test
# Send probe packet from 10.0.0.2,tcp:1025
0 sendip -p ipv4 -id 127.0.0.2 -is 10.0.0.2 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched with --return-nomatch
0 ./check_klog.sh 10.0.0.2 tcp 1025 test-nomatch
# Send probe packet from 10.0.0.255,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.0.255 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched with --return-nomatch
0 ./check_klog.sh 10.0.0.255 tcp 1025 test-nomatch
# Send probe packet from 10.0.1.0,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.1.0 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched
0 ./check_klog.sh 10.0.1.0 tcp 1025 test
# Destroy sets and rules
0 ./iptables.sh inet stop
# Create sets and inet rules which call set match, reversed rule order
0 ./iptables.sh inet start_flags_reversed
# Send probe packet from 10.0.0.0,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.0.0 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched with --return-nomatch
0 ./check_klog.sh 10.0.0.0 tcp 1025 test-nomatch
# Send probe packet from 10.0.0.1,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.0.1 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched
0 ./check_klog.sh 10.0.0.1 tcp 1025 test
# Send probe packet from 10.0.0.2,tcp:1025
0 sendip -p ipv4 -id 127.0.0.2 -is 10.0.0.2 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched with --return-nomatch
0 ./check_klog.sh 10.0.0.2 tcp 1025 test-nomatch
# Send probe packet from 10.0.0.255,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.0.255 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched with --return-nomatch
0 ./check_klog.sh 10.0.0.255 tcp 1025 test-nomatch
# Send probe packet from 10.0.1.0,tcp:1025
0 sendip -p ipv4 -id 127.0.0.1 -is 10.0.1.0 -p tcp -td 80 -ts 1025 127.0.0.1
# Check that test set matched
0 ./check_klog.sh 10.0.1.0 tcp 1025 test
# Destroy sets and rules
0 ./iptables.sh inet stop
# eof
