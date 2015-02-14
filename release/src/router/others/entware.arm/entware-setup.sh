#!/bin/sh

BOLD="\033[1m"
NORM="\033[0m"
INFO="$BOLD Info: $NORM"
ERROR="$BOLD *** Error: $NORM"
WARNING="$BOLD * Warning: $NORM"
INPUT="$BOLD => $NORM"

i=1 # Will count available partitions (+ 1)
cd /tmp

echo -e $INFO This script will guide you through the Entware-Arm installation.
echo -e $INFO Script modifies only \"entware.arm\" folder on the chosen drive,
echo -e $INFO no other data will be touched. Existing installation will be
echo -e $INFO replaced with this one. Also some start scripts will be installed,
echo -e $INFO the old ones will be saved on partition where Entware is installed
echo -e $INFO like /tmp/mnt/sda1/jffs_scripts_backup.tgz
echo 

if [ ! -d /jffs/scripts ]
then
  echo -e "$ERROR Please \"Enable JFFS partition\" & \"JFFS custom scripts and configs\""
  echo -e "$ERROR from router web UI: www.asusrouter.com/Advanced_System_Content.asp"
  echo -e "$ERROR then reboot router and try again. Exiting..."
  exit 1
fi

echo -e $INFO Looking for available  partitions...
for mounted in `/bin/mount | grep -E 'ext2|ext3|ext4' | cut -d" " -f3`
do
  isPartitionFound="true"
  echo "[$i] -->" $mounted
  eval mounts$i=$mounted
  i=`expr $i + 1`
done

if [ $i == "1" ]
then
  echo -e "$ERROR No ext2/ext3/ext4 partition available. Exiting..."
  exit 1
fi

echo -en "$INPUT Please enter partition number or 0 to exit\n$BOLD[0-`expr $i - 1`]$NORM: "
read partitionNumber
if [ "$partitionNumber" == "0" ]
then
  echo -e $INFO Exiting...
  exit 0
fi

if [ "$partitionNumber" -gt `expr $i - 1` ]                                                           
then                                                                                       
  echo -e "$ERROR Invalid partition number!  Exiting..."                                                                 
  exit 1                                                                                   
fi

eval entPartition=\$mounts$partitionNumber
echo -e "$INFO $entPartition selected.\n"
entFolder=$entPartition/entware.arm

if [ -d $entFolder ]
then
  echo -e "$WARNING Found previous installation, deleting..."
  rm -fr $entFolder
fi
echo -e $INFO Creating $entFolder folder...
mkdir $entFolder

if [ -d /tmp/opt ]
then
  echo -e "$WARNING Deleting old /tmp/opt symlink..."
  rm /tmp/opt
fi
echo -e $INFO Creating /tmp/opt symlink...
ln -s $entFolder /tmp/opt

echo -e $INFO Creating /jffs scripts backup...
tar -czf $entPartition/jffs_scripts_backup_`date +\%F_\%H-\%M`.tgz /jffs/scripts/* >/dev/nul

echo -e "$INFO Modifying start scripts..."
cat > /jffs/scripts/services-start << EOF
#!/bin/sh

sleep 10
/opt/etc/init.d/rc.unslung start
EOF
chmod +x /jffs/scripts/services-start

cat > /jffs/scripts/services-stop << EOF
#!/bin/sh

/opt/etc/init.d/rc.unslung stop
EOF
chmod +x /jffs/scripts/services-stop

cat > /jffs/scripts/post-mount << EOF
#!/bin/sh

if [ \$1 = "__Partition__" ]
then
  ln -sf \$1/entware.arm /tmp/opt
fi
EOF
eval sed -i 's,__Partition__,$entPartition,g' /jffs/scripts/post-mount
chmod +x /jffs/scripts/post-mount

echo -e "$INFO Starting Entware-Arm deployment....\n"
wget http://qnapware.zyxmon.org/binaries-armv7/installer/entware_install_arm.sh
sh ./entware_install_arm.sh
sync
