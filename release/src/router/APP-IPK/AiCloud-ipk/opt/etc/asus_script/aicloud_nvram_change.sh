#!/bin/sh
touch /tmp/aicloud_nvram.txt
echo "$1 $2"|sed -e 's/\//\\&/g'>/tmp/aicloud_nvram.txt
