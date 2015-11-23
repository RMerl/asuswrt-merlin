#!/bin/sh

BOLD="\033[1m"
NORM="\033[0m"
INFO="$BOLD Info: $NORM"
ERROR="$BOLD *** Error: $NORM"
WARNING="$BOLD * Warning: $NORM"
INPUT="$BOLD => $NORM"

i=1 # Will count available partitions (+ 1)
cd /tmp

echo -e "$INFO This script will guide you through the Entware installation."
echo -e "$INFO Script modifies \"entware\" folder only on the chosen drive,"
echo -e "$INFO no other data will be changed. Existing installation will be"
echo -e "$INFO replaced with this one. Also some start scripts will be installed,"
echo -e "$INFO the old ones will be saved on Entware partition with name"
echo -e "$INFO like /tmp/mnt/sda1/jffs_scripts_backup.tgz"
echo

case $(uname -m) in
  armv7l)
    PART_TYPES='ext2|ext3|ext4'
    INST_URL='http://entware.zyxmon.org/binaries/armv7/installer/entware_install.sh'
    ;;
  mips)
    PART_TYPES='ext2|ext3'
    INST_URL='http://entware.zyxmon.org/binaries/mipsel/installer/installer.sh'
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

eval entPartition=\$mounts$partitionNumber
echo -e "$INFO $entPartition selected.\n"
entFolder=$entPartition/entware

if [ -d $entFolder ] ; then
  echo -e "$WARNING Found previous installation, saving..."
  mv $entFolder $entFolder-old_`date +\%F_\%H-\%M`
fi
echo -e "$INFO Creating $entFolder folder..."
mkdir $entFolder

if [ -d /tmp/opt ] ; then
  echo -e "$WARNING Deleting old /tmp/opt symlink..."
  rm /tmp/opt
fi
echo -e "$INFO Creating /tmp/opt symlink..."
ln -s $entFolder /tmp/opt

echo -e "$INFO Creating /jffs scripts backup..."
tar -czf $entPartition/jffs_scripts_backup_`date +\%F_\%H-\%M`.tgz /jffs/scripts/* >/dev/nul

echo -e "$INFO Modifying start scripts..."
cat > /jffs/scripts/services-start << EOF
#!/bin/sh

RC='/opt/etc/init.d/rc.unslung'

i=30
until [ -x "\$RC" ] ; do
  i=\$((\$i-1))
  if [ "\$i" -lt 1 ] ; then
    logger "Could not start Entware"
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
  ln -nsf \$1/entware /tmp/opt
fi
EOF
eval sed -i 's,__Partition__,$entPartition,g' /jffs/scripts/post-mount
chmod +x /jffs/scripts/post-mount

if [ "$(nvram get jffs2_scripts)" != "1" ] ; then
  echo -e "$INFO Enabling custom scripts and configs from /jffs..."
  nvram set jffs2_scripts=1
  nvram commit
fi

wget -qO - $INST_URL | sh
