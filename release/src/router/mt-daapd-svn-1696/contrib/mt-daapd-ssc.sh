#!/bin/sh
#
# script to facilitate server-side transcoding of ogg files
#
#
# Usage: mt-daapd-ssc.sh <filename> <offset> <length in seconds>
#
# You may need to fix these paths:
#

WAVSTREAMER=wavstreamer
OGGDEC=oggdec
FLAC=flac

ogg_file() {
    $OGGDEC --quiet -o - "$FILE" | $WAVSTREAMER -o $OFFSET $FORGELEN
}


flac_file() {
    $FLAC --silent --decode --stdout "$FILE" | $WAVSTREAMER -o $OFFSET $FORGELEN
}

FILE=$1
OFFSET=${2:-0}

if [ "$3" != "" ]; then
    FORGELEN="-l $3"
fi

case "$1" in
  *.[oO][gG][gG])
  ogg_file
  ;;
  *.[fF][lL][aA][cC])
  flac_file
  ;;
  *)
# here you could cat a generic "error" wav...
# cat /path/to/error.wav
  ;;
esac
