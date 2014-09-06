#!/bin/sh

# current firmware information
current_firm=`nvram get firmver`
current_firm=`echo $current_firm | sed s/'\.'//g;`
current_buildno=`nvram get buildno`
current_extendno=`nvram get extendno`
current_extendno=`echo $current_extendno | sed s/-g.*//;`

# get firmware information
forsq=`nvram get apps_sq`
echo "forsq = ${forsq}" > /tmp/notif.log #1 for sq
model=`nvram get productid`
echo "model = ${model}" >> /tmp/notif.log #ex. RT-AC66U

#current info
flag_orig=`nvram get webs_notif_flag`
index_orig=`nvram get webs_notif_index`

nvram set webs_notif_update=0 # INITIALIZING
nvram set webs_notif_error=0
if [ "$flag_orig" = "" ];then
	nvram set webs_notif_flag=""
fi
if [ "$index_orig" = "" ];then
	nvram set webs_notif_index=""
fi	
nvram set webs_notif_info=""
nvram set webs_notif_error_msg=""

if [ "$forsq" = "1" ]; then
  wget -q http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless_SQ/asuswrt_notif.zip -O /tmp/asuswrt_notif.txt
else
	wget -q http://dlcdnet.asus.com/pub/ASUS/LiveUpdate/Release/Wireless/asuswrt_notif.zip -O /tmp/asuswrt_notif.txt
fi

if [ "$?" != "0" ]; then
	nvram set webs_notif_error=1
	nvram set webs_notif_error_msg="download asuswrt_notif.zip fail"
else
	filename='/tmp/asuswrt_notif.txt'
	exec < $filename
	while read line
	do		
		if [ "`echo $line | grep INDEX`" != "" ]; then
			index=`echo $line | grep INDEX | sed 's/.*#SN_//;'`
			echo "---- index : $index ----" >> /tmp/notif.log
			if [ "$forsq" = "1" ]; then
					nvram set webs_notif_flag=1
			elif [ "`nvram get webs_notif_index`" != "$index" ]; then
					nvram set webs_notif_flag=1
			fi
			if [ "$forsq" = "1" ]; then
				nvram set webs_notif_index="0000"
			else
				nvram set webs_notif_index="$index"
			fi
		fi			
		if [ "`echo $line | grep $model`" != "" ]; then	#Parsing single model
				EQ_qual=`echo $line | grep $model | sed 's/.*#EQ_//;' | sed 's/#.*//;'`
				FW=`echo $line | grep $model | sed 's/.*#FW//;' | sed 's/#.*//;'`
				FWB=`echo $FW | sed s/....//;`
				FWV=`echo $firmver | sed s/$FWB$//;`
				EXT=`echo $line | grep $model | sed 's/.*#EXT//;' | sed 's/#.*//;'`
				if [ "$EQ_qual" = "E" ]; then
						if [ "$current_buildno" = "$FWB" ] && [ "$current_firm" = "$FWV" ] && [ "$current_extendno" = "$EXT" ]; then
							hint=$hint++`echo $line | grep $model | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						fi
				elif [ "$EQ_qual" = "EL" ]; then
						if [ "$current_buildno" = "$FWB" ] && [ "$current_firm" = "$FWV" ] && [ "$current_extendno" = "$EXT" ]; then
							hint=$hint++`echo $line | grep $model | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						elif [ "$current_buildno" = "$FWB" ] && [ "$current_firm" = "$FWV" ] && [ "$current_extendno" -lt "$EXT" ]; then
							hint=$hint++`echo $line | grep $model | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						elif [ "$current_buildno" = "$FWB" ] && [ "$current_firm" -lt "$FWV" ]; then
							hint=$hint++`echo $line | grep $model | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						elif [ "$current_buildno" -lt "$FWB" ]; then
							hint=$hint++`echo $line | grep $model | sed 's/.*#NC_//;' | sed 's/#.*//;'`	
						fi
				fi			
		fi		
		if [ "`echo $line | grep ASUSWRT_ALL`" != "" ]; then	#Parsing ASUSWRT ALL model
				EQ_qual=`echo $line | grep ASUSWRT_ALL | sed 's/.*#EQ_//;' | sed 's/#.*//;'`
				FW=`echo $line | grep ASUSWRT_ALL | sed 's/.*#FW//;' | sed 's/#.*//;'`
				FWB=`echo $FW | sed s/....//;`
				FWV=`echo $firmver | sed s/$FWB$//;`
				EXT=`echo $line | grep ASUSWRT_ALL | sed 's/.*#EXT//;' | sed 's/#.*//;'`
				if [ "$EQ_qual" = "E" ]; then
						if [ "$current_buildno" = "$FWB" ] && [ "$current_firm" = "$FWV" ] && [ "$current_extendno" = "$EXT" ]; then
							hint=$hint++`echo $line | grep ASUSWRT_ALL | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						fi
				elif [ "$EQ_qual" = "EL" ]; then
						if [ "$current_buildno" = "$FWB" ] && [ "$current_firm" = "$FWV" ] && [ "$current_extendno" = "$EXT" ]; then
							hint=$hint++`echo $line | grep ASUSWRT_ALL | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						elif [ "$current_buildno" = "$FWB" ] && [ "$current_firm" = "$FWV" ] && [ "$current_extendno" -lt "$EXT" ]; then
							hint=$hint++`echo $line | grep ASUSWRT_ALL | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						elif [ "$current_buildno" = "$FWB" ] && [ "$current_firm" -lt "$FWV" ]; then
							hint=$hint++`echo $line | grep ASUSWRT_ALL | sed 's/.*#NC_//;' | sed 's/#.*//;'`
						elif [ "$current_buildno" -lt "$FWB" ]; then
							hint=$hint++`echo $line | grep ASUSWRT_ALL | sed 's/.*#NC_//;' | sed 's/#.*//;'`	
						fi
				fi
		fi
	done	
	echo "---- ${#hint} : $hint ----" >> /tmp/notif.log
	hint=`echo $hint`
	nvram set webs_notif_info="$hint"
fi
rm /tmp/asuswrt_notif.txt
nvram set webs_notif_update=1
