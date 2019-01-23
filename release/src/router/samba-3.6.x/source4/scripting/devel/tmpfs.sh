#!/bin/bash

# This sets up bin/ and st/ as tmpfs filesystems, which saves a lot of
# time waiting on the disk!

sudo echo "About to (re)mount bin and st as tmpfs"
rm -rf bin st 
sudo umount bin > /dev/null 2>&1 
sudo umount st  > /dev/null 2>&1 
mkdir -p bin st || exit 1
sudo mount -t tmpfs /dev/null bin || exit 1
sudo chown $USER bin/. || exit 1
echo "tmpfs setup for bin/"
sudo mount -t tmpfs /dev/null st || exit 1
sudo chown $USER st/. || exit 1
echo "tmpfs setup for st/"
