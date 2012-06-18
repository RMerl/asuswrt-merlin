#!/bin/sh

CONF_DIR=/etc
MT_DAAPD_FILE=/etc/mt-daapd.conf

if [ ! -n "$2" ]; then
  echo "insufficient arguments!"
  echo "Usage: $0 <server_name> <passwd> <mp3_dir> "
  exit 0
fi

SERVER_NAME="$1"
PASSWD="$2"
MP3_DIR="$3"

if [ ! -d $CONF_DIR ] ; then mkdir -p $CONF_DIR; fi

echo "web_root /etc_ro/web/admin-root" > $MT_DAAPD_FILE  
echo "port 3689" >> $MT_DAAPD_FILE  
echo "admin_pw $PASSWD" >> $MT_DAAPD_FILE  
echo "db_dir  /var/cache/mt-daapd" >> $MT_DAAPD_FILE  
echo "mp3_dir $MP3_DIR" >> $MT_DAAPD_FILE  
echo "servername $SERVER_NAME" >> $MT_DAAPD_FILE  
echo "runas admin" >> $MT_DAAPD_FILE  
echo "extensions .mp3,.m4a,.m4p" >> $MT_DAAPD_FILE  
echo "rescan_interval 300" >> $MT_DAAPD_FILE  
echo "always_scan 1" >> $MT_DAAPD_FILE  
echo "compress 1" >> $MT_DAAPD_FILE  
