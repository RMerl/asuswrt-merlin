#!/bin/sh

export PATH=/opt/bin:/opt/sbin:/sbin:/bin:/usr/sbin:/usr/bin$PATH

BOLD="\033[1m"
NORM="\033[0m"
INFO="$BOLD Info: $NORM"
ERROR="$BOLD *** Error: $NORM"
WARNING="$BOLD * Warning: $NORM"
INPUT="$BOLD => $NORM"

i=1 # Will count available partitions (+ 1)
cd /tmp

echo -e $INFO This script will guide you through the Optware-NG installation.
echo -e $INFO Script creates only \"optware-ng\" folder on the chosen drive,
echo -e $INFO no other data will be touched. Existing installation will be
echo -e $INFO replaced with this one. Also some start scripts will be installed,
echo -e $INFO the old ones will be saved on partition where Optware-NG is installed
echo -e $INFO like /tmp/mnt/sda1/jffs_scripts_backup.tgz
echo

if [ ! -d /jffs/scripts ] ; then
  echo -e "$ERROR Please \"Enable JFFS partition\" from \"Administration > System\""
  echo -e "$ERROR from router web UI: www.asusrouter.com/Advanced_System_Content.asp"
  echo -e "$ERROR then reboot router and try again. Exiting..."
  exit 1
fi

case $(uname -m) in
  armv7l)
    PART_TYPES='ext2|ext3|ext4'
    INST_URL='http://optware-ng.zyxmon.org/buildroot-armeabi-ng/buildroot-armeabi-ng-bootstrap.sh'
    OPT_FOLD='optware-ng.arm'
    ;;
  mips)
    PART_TYPES='ext2|ext3'
    INST_URL='http://optware-ng.zyxmon.org/buildroot-mipsel-ng/buildroot-mipsel-ng-bootstrap.sh'
    OPT_FOLD='optware-ng'
    ;;
  *)
    echo "This is unsupported platform, sorry."
    ;;
esac

echo -e "$INFO Looking for available partitions..."
for mounted in `/bin/mount | grep -E "$PART_TYPES" | cut -d" " -f3` ; do
  isPartitionFound="true"
  echo "[$i] --> $mounted"
  eval mounts$i=$mounted
  i=`expr $i + 1`
done

if [ $i == "1" ] ; then
  echo -e "$ERROR No $PART_TYPES partitions available. Exiting..."
  exit 1
fi

echo -en "$INPUT Please enter partition number or 0 to exit\n$BOLD[0-`expr $i - 1`]$NORM: "
read partitionNumber
if [ "$partitionNumber" == "0" ] ; then
  echo -e $INFO Exiting...
  exit 0
fi

if [ "$partitionNumber" -gt `expr $i - 1` ] ; then
  echo -e "$ERROR Invalid partition number! Exiting..."
  exit 1
fi

eval optPartition=\$mounts$partitionNumber
echo -e "$INFO $optPartition selected.\n"
optFolder=$optPartition/$OPT_FOLD
optwareFolder=$optPartition/optware-ng
optwarearmFolder=$optPartition/optware-ng.arm
entwareFolder=$optPartition/entware
entwarearmFolder=$optPartition/entware.arm
entwarengFolder=$optPartition/entware-ng
entwarengarmFolder=$optPartition/entware-ng.arm
asuswareFolder=$optPartition/asusware
asuswarearmFolder=$optPartition/asusware.arm

if [ -d /opt/debian ]
then
  echo -e "$WARNING Found chrooted-debian installation, stopping..."
  debian stop
fi

if [ -f /jffs/scripts/services-stop ]
then
  echo -e "$WARNING stopping running services..."
  /jffs/scripts/services-stop
fi

if [ -d $entwareFolder ] ; then
  echo -e "$WARNING Found Entware installation, saving..."
  mv $entwareFolder $entwareFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $entwarearmFolder ] ; then
  echo -e "$WARNING Found Entware Arm installation, saving..."
  mv $entwarearmFolder $entwarearmFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $asuswareFolder ] ; then
  echo -e "$WARNING Found Old Optware installation, saving..."
  mv $asuswareFolder $asuswareFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $asuswarearmFolder ] ; then
  echo -e "$WARNING Found Old Optware Arm installation, saving..."
  mv $asuswarearmFolder $asuswarearmFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $optwareFolder ] ; then
  echo -e "$WARNING Found previous mipsel installation, saving..."
  mv $optwareFolder $optwareFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $optwarearmFolder ] ; then
  echo -e "$WARNING Found previous arm installation, saving..."
  mv $optwarearmFolder $optwarearmFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $entwarengFolder ] ; then
  echo -e "$WARNING Found Entware-NG installation, saving..."
  mv $entwarengFolder $entwarengFolder.bak_`date +\%F_\%H-\%M`
fi

if [ -d $entwarengarmFolder ] ; then
  echo -e "$WARNING Found Entware-NG Arm installation, saving..."
  mv $entwarengarmFolder $entwarengarmFolder.bak_`date +\%F_\%H-\%M`
fi

echo -e $INFO Creating $optFolder folder...
mkdir $optFolder

if [ -d /tmp/opt ] ; then
  echo -e "$WARNING Deleting old /tmp/opt symlink..."
  rm /tmp/opt
fi

echo -e $INFO Creating /tmp/opt symlink...
ln -sf $optFolder /tmp/opt

echo -e $INFO Creating /jffs scripts backup...
tar -czf $optPartition/jffs_scripts_backup_`date +\%F_\%H-\%M`.tgz /jffs/scripts/* >/dev/nul

echo -e "$INFO Modifying start scripts..."
cat > /jffs/scripts/services-start << EOF
#!/bin/sh

RC='/opt/etc/init.d/rc.unslung'

i=30
until [ -x "\$RC" ] ; do
  i=\$((\$i-1))
  if [ "\$i" -lt 1 ] ; then
    logger "Could not start Optware-NG"
    exit
  fi
  sleep 1
done
\$RC start
EOF
chmod +x /jffs/scripts/services-start

cat > /jffs/scripts/services-stop << EOF
#!/bin/sh

/opt/etc/init.d/rc.unslung stop
EOF
chmod +x /jffs/scripts/services-stop

cat > /jffs/scripts/post-mount << EOF
#!/bin/sh

if [ "\$1" = "__Partition__" ] ; then
  ln -nsf \$1/$OPT_FOLD /tmp/opt
fi

if [ -f /opt/swap ]
then
  echo -e "Mounting swap file..."
  swapon /opt/swap
else
  echo -e "Swap file not found or /opt is not mounted..."
fi
EOF
eval sed -i 's,__Partition__,$optPartition,g' /jffs/scripts/post-mount
chmod +x /jffs/scripts/post-mount

if [ "$(nvram get jffs2_scripts)" != "1" ] ; then
  echo -e "$INFO Enabling custom scripts and configs from /jffs..."
  nvram set jffs2_scripts=1
  nvram commit
fi

echo -e "$INFO Starting Optware-NG deployment....\n"
wget -qO - $INST_URL | sh

mkdir -p /opt/etc/init.d
mkdir -p /opt/tmp

# Swap file
while :
do
    clear
    echo Router model `cat "/proc/sys/kernel/hostname"`
    echo "---------"
    echo "SWAP FILE"
    echo "---------"
    echo "Choose swap file size (Highly Recommended)"
    echo "1. 512MB (recommended)"
    echo "2. 1024MB"
    echo "3. 2048MB (recommended for MySQL Server)"
    echo "4. Skip this step, I already have a swap file / partition"
    echo "   or I don't want to create one right now"
    read -p "Enter your choice [ 1 - 4 ] " choice
    case $choice in
        1)
            echo -e "$INFO Creating a 512MB swap file..."
            echo -e "$INFO This could take a while, be patient..."
            dd if=/dev/zero of=/opt/swap bs=1024 count=524288
            mkswap /opt/swap
            chmod 0600 /opt/swap
			swapon /opt/swap
            read -p "Press [Enter] key to continue..." readEnterKey
			break
            ;;
        2)
            echo -e "$INFO Creating a 1024MB swap file..."
            echo -e "$INFO This could take a while, be patient..."
            dd if=/dev/zero of=/opt/swap bs=1024 count=1048576
            mkswap /opt/swap
            chmod 0600 /opt/swap
			swapon /opt/swap
            read -p "Press [Enter] key to continue..." readEnterKey
			break
            ;;
        3)
            echo -e "$INFO Creating a 2048MB swap file..."
            echo -e "$INFO This could take a while, be patient..."
            dd if=/dev/zero of=/opt/swap bs=1024 count=2097152
            mkswap /opt/swap
            chmod 0600 /opt/swap
			swapon /opt/swap
            read -p "Press [Enter] key to continue..." readEnterKey
			break
            ;;
        4)
            break
            ;;
        *)
            echo "ERROR: INVALID OPTION!"
			echo "Press 1 to create a 512MB swap file (recommended)"
			echo "Press 2 to create a 1024MB swap file"
			echo "Press 3 to create a 2048MB swap file (recommended for mysql)"
			echo "Press 4 to skip swap creation (not recommended)"
            read -p "Press [Enter] key to continue..." readEnterKey
            ;;
    esac
done

cat >> /opt/etc/init.d/rc.func << EOF
#!/bin/sh

ACTION=\$1
CALLER=\$2

ansi_red="\033[1;31m";
ansi_white="\033[1;37m";
ansi_green="\033[1;32m";
ansi_yellow="\033[1;33m";
ansi_blue="\033[1;34m";
ansi_bell="\007";
ansi_blink="\033[5m";
ansi_std="\033[m";
ansi_rev="\033[7m";
ansi_ul="\033[4m";

start() {
    [ "\$CRITICAL" != "yes" -a "\$CALLER" = "cron" ] && return 7
        [ "\$ENABLED" != "yes" ] && return 8
    echo -e -n "\$ansi_white Starting \$DESC... "
    if [ -n "\`pidof \$PROC\`" ]; then
        echo -e "            \$ansi_yellow already running. \$ansi_std"
        return 0
    fi
    \$PRECMD > /dev/null 2>&1
    \$PREARGS \$PROC \$ARGS > /dev/null 2>&1 &
    #echo \$PREARGS \$PROC \$ARGS
    COUNTER=0
    LIMIT=10
    while [ -z "\`pidof \$PROC\`" -a "\$COUNTER" -le "\$LIMIT" ]; do
        sleep 1s;
        COUNTER=\`expr \$COUNTER + 1\`
    done
    \$POSTCMD > /dev/null 2>&1

    if [ -z "\`pidof \$PROC\`" ]; then
        echo -e "            \$ansi_red failed. \$ansi_std"
        logger "Failed to start \$DESC from \$CALLER."
        return 255
    else
        echo -e "            \$ansi_green done. \$ansi_std"
        logger "Started \$DESC from \$CALLER."
        return 0
    fi
}

stop() {
    case "\$ACTION" in
        stop | restart)
            echo -e -n "\$ansi_white Shutting down \$PROC... "
            killall \$PROC 2>/dev/null
            COUNTER=0
            LIMIT=10
            while [ -n "\`pidof \$PROC\`" -a "\$COUNTER" -le "\$LIMIT" ]; do
                sleep 1s;
                COUNTER=\`expr \$COUNTER + 1\`
            done
            ;;
        kill)
            echo -e -n "\$ansi_white Killing \$PROC... "
            killall -9 \$PROC 2>/dev/null
            ;;
    esac

    if [ -n "\`pidof \$PROC\`" ]; then
        echo -e "            \$ansi_red failed. \$ansi_std"
        return 255
    else
        echo -e "            \$ansi_green done. \$ansi_std"
        return 0
    fi
}

check() {
    echo -e -n "\$ansi_white Checking \$DESC... "
    if [ -n "\`pidof \$PROC\`" ]; then
        echo -e "            \$ansi_green alive. \$ansi_std";
        return 0
    else
        echo -e "            \$ansi_red dead. \$ansi_std";
        return 1
    fi
}

reconfigure() {
    SIGNAL=SIGHUP
    echo -e "\$ansi_white Sending \$SIGNAL to \$PROC... "
    killall -\$SIGNAL \$PROC 2>/dev/null
}

for PROC in \$PROCS; do
    case \$ACTION in
        start)
            start
            ;;
        stop | kill )
            check && stop
            ;;
        restart)
            check > /dev/null && stop
            start
            ;;
        check)
            check
            ;;
        reconfigure)
            reconfigure
            ;;
        *)
            echo -e "\$ansi_white Usage: \$0 (start|stop|restart|check|kill|reconfigure)\$ansi_std"
            exit 1
            ;;
    esac
done

#logger "Leaving \${0##*/}."
EOF

cat >> /opt/etc/init.d/rc.unslung << EOF
#!/bin/sh

# Start/stop all init scripts in /opt/etc/init.d including symlinks
# starting them in numerical order and
# stopping them in reverse numerical order

#logger "Started \$0\${*:+ \$*}."

ACTION=\$1
CALLER=\$2

if [ \$# -lt 1 ]; then
    printf "Usage: \$0 {start|stop|restart|reconfigure|check|kill}\n" >&2
    exit 1
fi

[ \$ACTION = stop -o \$ACTION = restart -o \$ACTION = kill ] && ORDER="-r"

for i in \$(/opt/bin/find /opt/etc/init.d/ -perm '-u+x' -name 'S*' | sort \$ORDER ) ;
do
    case "\$i" in
        S* | *.sh )
            # Source shell script for speed.
            trap "" INT QUIT TSTP EXIT
            #set \$1
            #echo "trying \$i" >> /tmp/rc.log
            . \$i \$ACTION \$CALLER
            ;;
        *)
            # No sh extension, so fork subprocess.
            \$i \$ACTION \$CALLER
            ;;
    esac
done
EOF
chmod +x /opt/etc/init.d/*
echo -e $INFO init.d start/stop services scripts created

cat > /opt/bin/services << EOF
#!/bin/sh

case "\$1" in
 start)
   sh /jffs/scripts/services-start
   ;;
 stop)
   sh /jffs/scripts/services-stop
   ;;
 restart)
   sh /jffs/scripts/services-stop
   echo -e Restarting Optware-NG Installed Services...
   sleep 2
   sh /jffs/scripts/services-start
   ;;
 *)
   echo "Usage: services {start|stop|restart}" >&2
   exit 3
   ;;
esac
EOF
chmod +x /opt/bin/services

echo -e $INFO updating packages...
ipkg update
ipkg install findutils
echo -e $INFO New Generation OPTWARE installed successfully...
echo -e $INFO Now install some packages, with \"ipkg install package_name\",
echo -e $INFO like nano text editor, type in terminal \"ipkg install nano\"
echo -e $INFO List of installable mipsel packages
echo -e $INFO http://optware-ng.zyxmon.org/buildroot-mipsel-ng/Packages.html
echo -e $INFO List of installable arm packages
echo -e $INFO http://optware-ng.zyxmon.org/buildroot-armeabi/Packages.html
echo -e $INFO Thanks @ryzhov_al for initial script,
echo -e $INFO Thanks @alllexx88 for New Generation Optware,
echo -e $INFO Thanks @Rmerlin for awesome firmwares.
echo -e $INFO Brought to you by TeHashX!!!
echo -e $INFO Enjoy...
