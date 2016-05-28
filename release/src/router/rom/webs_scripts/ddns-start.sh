#!/bin/sh

source_input_data=$@
# ***** for duckdns service ******
#HOST=`echo "$source_input_data"|awk -F " " '{print $1}'`
#TOKEN=`echo "$source_input_data"|awk -F " " '{print $2}'`
# ********************************

# ***** for google ddns service *******
USER=`echo "$source_input_data"|awk -F " " '{print $1}'`
PASSWD=`echo "$source_input_data"|awk -F " " '{print $2}'`
HOST=`echo "$source_input_data"|awk -F " " '{print $3}'`
# *************************************

# ***** for google ddns service *******
#result=`curl -k "https://$USER:$PASSWD@domains.google.com/nic/update?hostname=$HOST"`
wget -S --no-check-certificate "https://$USER:$PASSWD@domains.google.com/nic/update?hostname=$HOST" -O /tmp/wget_result
#curl -k "https://www.duckdns.org/update?domains=$HOST&token=$TOKEN&ip=" >/dev/null 2>&1
# *************************************

# ***** for duckdns service ******
#result=`curl -k "https://www.duckdns.org/update?domains=$HOST&token=$TOKEN&ip="`
#wget -S --no-check-certificate "https://www.duckdns.org/update?domains=$HOST&token=$TOKEN&ip=" -O /tmp/wget_result
#curl -k "https://www.duckdns.org/update?domains=brucechin.duckdns.org&token=886cd7c1-7ba2-43ee-90e0-dac4cb914ac8&ip=" >/dev/null 2>&1
# ********************************
result=`cat /tmp/wget_result`

case $result in
	good*|nochg*)
	nvram set ddns_return_code=200
	nvram set ddns_return_code_chk=200
	`ddns_updated`
	;;
	
	OK)
	nvram set ddns_return_code=200
	nvram set ddns_return_code_chk=200
	`ddns_updated`

	;;

	abuse)
	nvram set ddns_return_code=200
        nvram set ddns_return_code_chk=200
	`ddns_updated`
	;;

	*)
	nvram set ddns_return_code=401
	nvram set ddns_return_code_chk=401
	;;
esac
