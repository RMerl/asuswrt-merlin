#!/bin/ash
# $Id: reverse_replace.sh 18 2015-03-01 16:12:35Z jo $
#
# Usage e.g.: netstat -n -4 | reverse_replace.sh 
# Parses stdin for IP4 addresses and replaces them 
# with names retrieved by parsing the dnsmasq log.
# This currently only gives CNAMEs. But these 
# usually tell ou more than the mones from reverse 
# lookups. 
#
# This has been tested on debian and asuswrt. Plese
# report successful tests on other platforms.
#
# Author: Joachim Zobel <jz-2014@heute-morgen.de>
# License: Consider this MIT style licensed. You can 
#   do as you ike, but you must not remove my name.
#

LOG=/var/log/dnsmasq.log
MAX_LINES=15000

# sed regex do match IPs
IP_regex='[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}'
# private IP ranges
IP_private='\(^127\.\)\|\(^192\.168\.\)\|\(^10\.\)\|\(^172\.1[6-9]\.\)\|\(^172\.2[0-9]\.\)\|\(^172\.3[0-1]\.\)'

#######################################################################
# Find Commands
  
HOST=nslookup
if type host > /dev/null 2>&1; then
  # echo "No need for nslookup, host is there"
  HOST=host
fi

#######################################################################
# Functions

# Use shell variables for an (IP) lookup table
create_lookup_table()
{
  # Parse log into lookup table
  local CMDS="$( tail -"$MAX_LINES" "$LOG" | \
    grep " is $IP_regex" | \
    sed "s#.* \([^ ]*\) is \($IP_regex\).*#set_val \2 \1;#" )"

  local IFS='
'
  for CMD in $CMDS
  do
    eval $CMD
  done
}

set_val()
{
  local _IP=$(echo $1 | tr . _)
  local KEY="__IP__$_IP"
  eval "$KEY"=$2
}

get_val()
{
  local _IP=$(echo $1 | tr . _)
  local KEY="__IP__$_IP"
  eval echo -n '${'"$KEY"'}'
}

dns_lookup()
{
  local IP=$1

  local RTN="$($HOST $IP | \
        sed 's#\s\+#\n#g' | \
        grep -v '^$' | \
        tail -1 | tr -d '\n' | \
        sed 's#\.$##')"
  if echo $RTN | grep -q NXDOMAIN; then
    echo -n $IP
  else
    echo -n "$RTN"
  fi     
}

reverse_dns()
{
  local IP=$1

  # Skip if it is not an IP
  if ! echo $IP | grep -q "^$IP_regex$"; then
    echo -n $IP
    return 
  fi
    
  # Do a dns lookup, if it is a local IP
  if echo $IP | grep -q $IP_private; then
    dns_lookup $IP
    return
  fi
    
  local NAME="$(get_val $IP)"
  
  if [ -z "$NAME" ]; then
    echo -n $IP
  else
    echo -n $NAME
  fi
}

#######################################################################
# Main
create_lookup_table

while read LINE; do
  for IP in $(echo "$LINE" | \
              sed "s#\b\($IP_regex\)\b#\n\1\n#g" | \
              grep $IP_regex) 
  do
    NAME=`reverse_dns $IP `
    # echo "$NAME $IP"
    LINE=`echo "$LINE" | sed "s#$IP#$NAME#" ` 
  done
  echo $LINE
done

