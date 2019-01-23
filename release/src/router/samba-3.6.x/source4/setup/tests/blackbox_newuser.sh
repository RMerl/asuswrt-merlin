#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: blackbox_newuser.sh PREFIX
EOF
exit 1;
fi

PREFIX="$1"
shift 1

. `dirname $0`/../../../testprogs/blackbox/subunit.sh


rm -rf $PREFIX/simple-dc
testit "simple-dc" $PYTHON $SRCDIR/source4/setup/provision --server-role="dc" --domain=FOO --realm=foo.example.com --domain-sid=S-1-5-21-4177067393-1453636373-93818738 --targetdir=$PREFIX/simple-dc
samba_tool="./bin/samba-tool"

CONFIG="--configfile=$PREFIX/simple-dc/etc/smb.conf"

#two test for creating new user
#newuser  account is created with cn=Given Name Initials. Surname
#newuser1 account is created using cn=username
testit "newuser" $samba_tool newuser $CONFIG --given-name="User" --surname="Tester" --initials="T" --profile-path="\\\\myserver\\my\\profile" --script-path="\\\\myserver\\my\\script" --home-directory="\\\\myserver\\my\\homedir" --job-title="Tester" --department="Testing" --company="Samba.org" --description="Description" --mail-address="tester@samba.org" --internet-address="http://samba.org" --telephone-number="001122334455" --physical-delivery-office="101" --home-drive="H:" NewUser testp@ssw0Rd
testit "newuser" $samba_tool newuser $CONFIG --use-username-as-cn --given-name="User1" --surname="Tester1" --initials="UT1" --profile-path="\\\\myserver\\my\\profile" --script-path="\\\\myserver\\my\\script" --home-directory="\\\\myserver\\my\\homedir" --job-title="Tester" --department="Testing" --company="Samba.org" --description="Description" --mail-address="tester@samba.org" --internet-address="http://samba.org" --telephone-number="001122334455" --physical-delivery-office="101" --home-drive="H:" NewUser1 testp@ssw0Rd

# check the enable account script
testit "enableaccount" $samba_tool enableaccount $CONFIG NewUser
testit "enableaccount" $samba_tool enableaccount $CONFIG NewUser1

# check the enable account script
testit "setpassword" $samba_tool setpassword $CONFIG NewUser --newpassword=testp@ssw0Rd2
testit "setpassword" $samba_tool setpassword $CONFIG NewUser1 --newpassword=testp@ssw0Rd2

# check the setexpiry script
testit "noexpiry" $samba_tool setexpiry $CONFIG NewUser --noexpiry
testit "noexpiry" $samba_tool setexpiry $CONFIG NewUser1 --noexpiry
testit "expiry" $samba_tool setexpiry $CONFIG NewUser --days=7
testit "expiry" $samba_tool setexpiry $CONFIG NewUser1 --days=7

exit $failed
