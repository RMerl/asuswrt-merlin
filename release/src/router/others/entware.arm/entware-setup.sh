#!/bin/sh

BOLD="\033[1m"
NORM="\033[0m"
INFO="$BOLD Info: $NORM"
ERROR="$BOLD *** Error: $NORM"
WARNING="$BOLD * Warning: $NORM"
INPUT="$BOLD => $NORM"

i=1 # Will count available partitions (+ 1)
cd /tmp

printf "$INFO This script will guide you through the Entware-Arm installation.\n"
printf "$INFO Script modifies only \"entware.arm\" folder on the chosen drive,\n"
printf "$INFO no other data will be touched. Existing installation will be\n"
printf "$INFO replaced with this one. Also some start scripts will be installed,\n"
printf "$INFO the old ones will be saved on partition where Entware is installed\n"
printf "$INFO like /tmp/mnt/sda1/jffs_scripts_backup.tgz\n"
printf "\n"

if [ ! -d /jffs/scripts ]
then
  printf "$ERROR Please \"Enable JFFS partition\" & \"JFFS custom scripts and configs\"\n"
  printf "$ERROR from router web UI: www.asusrouter.com/Advanced_System_Content.asp\n"
  printf "$ERROR then reboot router and try again. Exiting...\n"
  exit 1
fi

printf "$INFO Looking for available  partitions...\n"
for mounted in `/bin/mount | grep -E 'ext2|ext3|ext4' | cut -d" " -f3`
do
  isPartitionFound="true"
  printf "[%d] --> %s\n" "$i" "$mounted"
  eval mounts$i=$mounted
  i=`expr $i + 1`
done

if [ $i = "1" ]
then
  printf "$ERROR No ext2/ext3/ext4 partition available. Exiting...\n"
  exit 1
fi

printf "$INPUT Please enter partition number or 0 to exit\n$BOLD%s$NORM: " "[0-`expr $i - 1`]"
read partitionNumber
if [ "$partitionNumber" = "0" ]
then
  printf "$INFO Exiting...\n"
  exit 0
fi

if [ "$partitionNumber" -gt `expr $i - 1` ]
then
  printf "$ERROR Invalid partition number!  Exiting...\n"
  exit 1
fi

eval entPartition=\$mounts$partitionNumber
printf "$INFO %s selected.\n\n" "$entPartition"
entFolder=$entPartition/entware.arm

if [ -d $entFolder ]
then
  printf "$WARNING Found previous installation, saving...\n"
  mv $entFolder $entFolder-old_`date +\%F_\%H-\%M`
fi
printf "$INFO Creating %s folder...\n" "$entFolder"
mkdir $entFolder

if [ -d /tmp/opt ]
then
  printf "$WARNING Deleting old /tmp/opt symlink...\n"
  rm /tmp/opt
fi
printf "$INFO Creating /tmp/opt symlink...\n"
ln -s $entFolder /tmp/opt

printf "$INFO Creating /jffs scripts backup...\n"
tar -czf $entPartition/jffs_scripts_backup_`date +\%F_\%H-\%M`.tgz /jffs/scripts/* >/dev/nul

printf "$INFO Modifying start scripts...\n"
cat > /jffs/scripts/services-start << EOF
#!/bin/sh
script="/opt/etc/init.d/rc.unslung"

i=60
until [ -x "\${script}" ]
do
        i=\$((\$i-1))
        if [ "\$i" -lt 1 ]
        then
                logger "Could not start Entware"
                exit
        fi
        sleep 1
done
\${script} start
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
  ln -nsf \$1/entware.arm /tmp/opt
fi
EOF
eval sed -i 's,__Partition__,$entPartition,g' /jffs/scripts/post-mount
chmod +x /jffs/scripts/post-mount

printf "$INFO Starting Entware-Arm deployment....\n\n"
wget http://qnapware.zyxmon.org/binaries-armv7/installer/entware_install_arm.sh
sh ./entware_install_arm.sh
sync
