#!/bin/sh
######################
# stealthMode Sunset #
######################
#
# Turn off/on all LEDs based on sun set for Asus routers which uses ASUSWRT-Merlin firmware
#
# Version: 1.0m "Sunset"
# Author : Monter (http://techlog.pl)
#
# this makes you put and call script where / how you want
dir=`echo $( cd "$(dirname "$0")" ; pwd -P )`
stms=`basename ${0}`
# LEDs status
ISLEDSON=`nvram get led_disable`
# Go!
case "$1" in
    on)
        if [ "$ISLEDSON" = "0" ]; then
          logger -p6 -s -t stealthMode Activated
          nvram set led_disable=1
          service restart_leds > /dev/null 2>&1
        else
          logger -p6 -s -t stealthMode It\'s currently enabled!
        fi
    ;;
    sun)
        if [ "$2" != "" -a "0$(echo $2 | tr -d ' ')" -ge 0 ] 2>/dev/null; then
            logger -p6 -s -t stealthMode Sunset Enabled
            cru a stealthsun_on "0 2 * * * $dir/$stms sun_on $2"
            $dir/$stms sun_on $2
        else
            logger -p3 -s -t stealthMode Sunset error - the city code does not specified!
            $dir/$stms
            exit 1
        fi
    ;;
    sun_on)
        # check internet connection
        CONERR=1
        # ping for check internet connection
        YAHOOAPIS="weather.yahoo.com"
        if [ `echo $YAHOOAPIS | wc -c` -gt 2 ]; then
          ping -q -w 1 -c 1 $YAHOOAPIS > /dev/null
          if [ $? == 0 ]; then CONERR=0; fi
        fi
        # result of internet connection check
        if [ "$CONERR" -ne 0 ] ; then
            logger -p3 -s -t stealthMode Sunset error - no active Internet connection!
            $dir/$stms
            exit 1
        else
            if [ "$2" != "" -a "0$(echo $2 | tr -d ' ')" -ge 0 ] 2>/dev/null; then
                $dir/$stms sch_clear # clear scheduled tasks when sunset is enabled
                sun=`l=$2;wget -q -O - http://xml.weather.yahoo.com/forecastrss?w=$l | grep astronomy | awk -F\" '{print $4}' | awk -F\" '{split($1, A, " ");split(A[1], B, ":");HR=B[1];MIN=B[2];if(A[2] == "pm") HR+=12;$1=sprintf("%d %d", MIN, HR);}1'`
                if [ "$sun" = "" ] ; then logger -p3 -s -t stealthMode Weather sunset time error!; exit 1; else cru a stealthsunset $sun "* * * $dir/$stms on"; fi
                sur=`l=$2;wget -q -O - http://xml.weather.yahoo.com/forecastrss?w=$l | grep astronomy | awk -F\" '{print $2}' | awk -F\" '{split($1, A, " ");split(A[1], B, ":");HR=B[1];MIN=B[2];if(A[2] == "pm") HR+=12;$1=sprintf("%d %d", MIN, HR);}1'`
                if [ "$sur" = "" ] ; then logger -p3 -s -t stealthMode Weather sunrise time error!; exit 1; else cru a stealthsunrise $sur "* * * $dir/$stms off"; fi
                logger -p6 -s -t stealthMode Sunset city code: $2, sunset: $( echo $sun | awk '{$1=sprintf("%02d", $1); $2=sprintf("%02d", $2); print $2":"$1 }' ), sunrise: $( echo $sur | awk '{$1=sprintf("%02d", $1); $2=sprintf("%02d", $2); print $2":"$1 }' )
                HOUR=$(date +%k); SUNRUN=$( echo $sun | awk '{ print $2 }' ); SURRUN=$( echo $sur | awk '{ print $2 }' ); if [ "$HOUR" -ge "$SURRUN" -a "$HOUR" -le "$SUNRUN" ] ; then
                    logger -p6 -s -t stealthMode The time now is $( date +%H:%M), so StealthMode will be activated on the next sunset
                else
                    $dir/$stms on
                fi
            else
                logger -p3 -s -t stealthMode Sunset error - the city code does not specified!
                $dir/$stms
                exit 1
            fi
        fi
    ;;
    sun_off)
        logger -p6 -s -t stealthMode Sunset Disactivated
        cru d stealthsun_on
        cru d stealthsunset
        cru d stealthsunrise
        $dir/$stms off
    ;;
    sch_on)
        if [ "${2}" != "" -a "${2}" -le 23 -a "0$(echo $2 | tr -d ' ')" -ge 0 ] 2>/dev/null; then
          if [ "${3}" != "" -a "${3}" -le 59 -a "${3}" -eq "${3}" ] 2>/dev/null; then SCHMIN=${3}; else SCHMIN="0"; fi
          cru a stealthsheduleon $SCHMIN $2 "* * * $dir/$stms on"
          logger -p6 -s -t stealthMode Scheduled On set to $2:$(printf "%02d" $SCHMIN)
        else
          logger -p3 -s -t stealthMode Scheduled On error - Hour / Minutes does not specified!
          $dir/$stms
          exit 1
        fi
    ;;
    sch_off)
        if [ "${2}" != "" -a "${2}" -le 23 -a "0$(echo $2 | tr -d ' ')" -ge 0 ] 2>/dev/null; then
          if [ "${3}" != "" -a "${3}" -le 59 -a "${3}" -eq "${3}" ] 2>/dev/null; then SCHMIN=${3}; else SCHMIN="0"; fi
          cru a stealthsheduleoff $SCHMIN $2 "* * * $dir/$stms off"
          logger -p6 -s -t stealthMode Scheduled Off set to $2:$(printf "%02d" $SCHMIN)
        else
          logger -p3 -s -t stealthMode Scheduled Off error - Hour / Minutes does not specified!
          $dir/$stms
          exit 1
        fi
    ;;
    sch_clear)
        SON=`cru l | grep stealthsheduleon | wc -l`
        SOFF=`cru l | grep stealthsheduleoff | wc -l`
        if [ "$SON" = "1" -o "$SOFF" = "1" ]; then
          cru d stealthsheduleon
          cru d stealthsheduleoff
          logger -p6 -s -t stealthMode Scheduler Tasks Deleted
        fi
    ;;
    clear_all)
        $dir/$stms sun_off
        $dir/$stms sch_clear
        logger -p6 -s -t stealthMode Complete shutdown and delete all jobs from Crontab - done
    ;;
    off)
        nvram set led_disable=0
        service restart_leds > /dev/null 2>&1
        logger -p6 -s -t stealthMode Disactivated
    ;;
    *)
        echo "stealthMode Sunset v1.0m by Monter [released on 29.12.2014]"
        logger -p1 -s -t Usage "$stms {on|off|sun <city_code>|sun_off|sch_on <H> <M>|sch_off <H> <M>|sch_clear|clear_all}"
        echo
        echo " [Standard mode]"
        echo "   on | off        - enable or disable steathMode in real time"
        echo
        echo " [Sunset mode]"
        echo "   sun <city_code> - will daily get the sunrise and sunset times of a specific"
        echo "                     location automatically and activate Sunset mode"
        echo "                     To be able to determine your <city_code> you need to first"
        echo "                     go to http://weather.yahoo.com/ and look up your location"
        echo "                     The last NUMBERS in the URL will be your <city_code>"
        echo "                     Example: \"$stms sun 514048\" if you live in Poznan/Poland ;)"
        echo "                     This feature requires a working Internet connection"
        echo "   sun_off         - fully deactivate Sunset mode"
        echo
        echo " [Scheduled mode]"
        echo "   sch_on <H> <M>  - set the hour and minutes of the scheduled enable/disable"
        echo "   sch_off <H> <M>   stealthMode in Standard mode and adding jobs to the Crontab"
        echo "                     Hour and minute time must be a numbers without any additional"
        echo "                     characters, where hour is a mandatory parameter, while not"
        echo "                     specifying an minute will assign a default 00 value"
        echo "                     These options add just the right job for Crontab, nothing more"
        echo "   sch_clear       - removes tasks from Crontab for scheduled enable/disable"
        echo "                     stealthMode function set by sch_on and sch_off switches"
        echo
        echo " [Repair / debug]"
        echo "   clear_all       - removes all jobs from Crontab and completely disables all"
        echo "                     available stealthMode modes"
        echo
        echo " Send your bug report to monter[at]techlog.pl"
        exit 1
esac
