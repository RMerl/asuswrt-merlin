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
result=`curl -k "https://$USER:$PASSWD@domains.google.com/nic/update?hostname=$HOST"`
#curl -k "https://www.duckdns.org/update?domains=$HOST&token=$TOKEN&ip=" >/dev/null 2>&1
# *************************************

# ***** for duckdns service ******
#result=`curl -k "https://www.duckdns.org/update?domains=$HOST&token=$TOKEN&ip="`
#curl -k "https://www.duckdns.org/update?domains=brucechin.duckdns.org&token=886cd7c1-7ba2-43ee-90e0-dac4cb914ac8&ip=" >/dev/null 2>&1
# ********************************

case $result in
	good*|nochg*)
	nvram set ddns_return_code=200
	nvram set ddns_return_code_chk=200
	`ddns_updated`
	rm -rf /tmp/etc/ddns_result
	echo "user : $USER host : $HOST passwd : $PASSWD">>/tmp/etc/ddns_result
	#echo "host : $HOST token : $TOKEN">>/tmp/etc/ddns_result
	echo "result : $result">>/tmp/etc/ddns_result
    	echo "ddns result : OK">/tmp/etc/ddns_result
	;;
	
	OK)
	nvram set ddns_return_code=200
	nvram set ddns_return_code_chk=200
	`ddns_updated`
	rm -rf /tmp/etc/ddns_result
	#echo "host : $HOST token : $TOKEN">>/tmp/etc/ddns_result
	echo "user : $USER host : $HOST passwd : $PASSWD">>/tmp/etc/ddns_result
        echo "result : $result">>/tmp/etc/ddns_result
	echo "ddns result : OK">/tmp/etc/ddns_result
	;;

	abuse)
	nvram set ddns_return_code=200
        nvram set ddns_return_code_chk=200
	`ddns_updated`
        rm -rf /tmp/etc/ddns_result
	#echo "host : $HOST token : $TOKEN">>/tmp/etc/ddns_result
	echo "user : $USER host : $HOST passwd : $PASSWD">>/tmp/etc/ddns_result
        echo "result : $result">>/tmp/etc/ddns_result
        echo "ddns result : OK">/tmp/etc/ddns_result
	;;

	*)
	nvram set ddns_return_code=401
	nvram set ddns_return_code_chk=401
	rm -rf /tmp/etc/ddns_result
	#echo "host : $HOST token : $TOKEN">>/tmp/etc/ddns_result
	echo "user : $USER host : $HOST passwd : $PASSWD">>/tmp/etc/ddns_result
        echo "result : $result">>/tmp/etc/ddns_result
	echo "ddns result : NG">>/tmp/etc/ddns_result
	;;
esac
