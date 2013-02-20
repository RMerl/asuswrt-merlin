#!/bin/sh

i=1 # Will count available partitions (+ 1)
cd /tmp

echo Info: This script will guide you through the Entware installation.
echo Info: Script modifies only \"entware\" folder on the chosen drive,
echo Info: no other data will be touched. Existing installation will be
echo Info: replaced with this one. Also some start scripts will be installed,
echo Info: the old ones will be saved to .\entware\jffs_scripts_backup.tgz
echo 

if [ ! -d /jffs/scripts ]
then
  echo Error: Please enable JFFS partition from web UI, reboot router and
  echo Error: try again.  Exiting...
  exit 1
fi

echo Info: Looking for available  partitions...
for mounted in `/bin/mount | grep -E 'ext2|ext3' | cut -d" " -f3`
do
  isPartitionFound="true"
  echo "[$i] -->" $mounted
  eval mounts$i=$mounted
  i=`expr $i + 1`
done

if [ $i == "1" ]
then
  echo Error: No ext2/ext3 partition available. Exiting...
  exit 1
fi

echo "Info: Please enter partition number or 0 to exit [0-`expr $i - 1`]:"
read partitionNumber
if [ "$partitionNumber" == "0" ]
then
  echo Info: Exiting...
  exit 0
fi

eval entPartition=\$mounts$partitionNumber
echo "Info: $entPartition selected."
entFolder=$entPartition/entware

if [ -d $entFolder ]
then
  echo Warning: Found previous installation, deleting...
  rm -fr $entFolder
fi
echo Info: Creating $entFolder folder...
mkdir $entFolder

if [ -d /tmp/opt ]
then
  echo Warning: Deleting old /tmp/opt symlink...
  rm /tmp/opt
fi
echo Info: Creating /tmp/opt symlink...
ln -s $entFolder /tmp/opt

echo Info: Creating /jffs scripts backup...
tar -cvzf $entPartition/jffs_scripts_backup.tgz /jffs/*

echo "Info: Modifying start scripts..."
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
  ln -sf \$1/entware /tmp/opt
fi
EOF
eval sed -i 's,__Partition__,$entPartition,g' /jffs/scripts/post-mount
chmod +x /jffs/scripts/post-mount

echo Info: Starting Entware deployment...
wget http://wl500g-repo.googlecode.com/svn/ipkg/entware_install.sh
sh ./entware_install.sh
