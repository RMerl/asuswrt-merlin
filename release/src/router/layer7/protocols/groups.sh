#!/bin/bash
cat */*.pat | grep -i "Protocol Groups" | cut -d\  -f 4- | tr ' ' '\n' | sort | uniq -c | sort -n
