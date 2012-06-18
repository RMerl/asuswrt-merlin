#! /bin/sh
#
# create printcap file
#
if [ -r /usr/samba/printcap ]
then
  cp /usr/samba/printcap /usr/samba/printcap.O
fi

echo "#" > /usr/samba/printcap
echo "# Samba printcap file" >> /usr/samba/printcap
echo "# Alias names are separated by |, any name with spaces is taken as a comment" >> /usr/samba/printcap
echo "#" >> /usr/samba/printcap
lpstat -a | sed -e "s/ .*//" >> /usr/samba/printcap

