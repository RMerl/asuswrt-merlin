#!/bin/sh

if [ $# -lt 1 ]; then
cat <<EOF
Usage: blackbox_group.sh PREFIX
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

#creation of two test subjects
testit "newuser" $samba_tool newuser $CONFIG --given-name="User" --surname="Tester" --initial="UT" testuser testp@ssw0Rd
testit "newuser" $samba_tool newuser $CONFIG --given-name="User1" --surname="Tester" --initial="UT" testuser1 testp@ssw0Rd

#test creation of six different groups
testit "group add" $samba_tool group add $CONFIG --group-scope='Domain' --group-type='Security' --description='DomainSecurityGroup' --mail-address='dsg@samba.org' --notes='Notes' dsg
testit "group add" $samba_tool group add $CONFIG --group-scope='Global' --group-type='Security' --description='GlobalSecurityGroup' --mail-address='gsg@samba.org' --notes='Notes' gsg
testit "group add" $samba_tool group add $CONFIG --group-scope='Universal' --group-type='Security' --description='UniversalSecurityGroup' --mail-address='usg@samba.org' --notes='Notes' usg
testit "group add" $samba_tool group add $CONFIG --group-scope='Domain' --group-type='Distribution' --description='DomainDistributionGroup' --mail-address='ddg@samba.org' --notes='Notes' ddg
testit "group add" $samba_tool group add $CONFIG --group-scope='Global' --group-type='Distribution' --description='GlobalDistributionGroup' --mail-address='gdg@samba.org' --notes='Notes' gdg
testit "group add" $samba_tool group add $CONFIG --group-scope='Universal' --group-type='Distribution' --description='UniversalDistributionGroup' --mail-address='udg@samba.org' --notes='Notes' udg

#test adding test users to all groups by their username
testit "group addmembers" $samba_tool group addmembers $CONFIG dsg newuser,newuser1
testit "group addmembers" $samba_tool group addmembers $CONFIG gsg newuser,newuser1
testit "group addmembers" $samba_tool group addmembers $CONFIG usg newuser,newuser1
testit "group addmembers" $samba_tool group addmembers $CONFIG ddg newuser,newuser1
testit "group addmembers" $samba_tool group addmembers $CONFIG gdg newuser,newuser1
testit "group addmembers" $samba_tool group addmembers $CONFIG udg newuser,newuser1

#test removing test users from all groups by their username
testit "group removemembers" $samba_tool group removemembers $CONFIG dsg newuser,newuser1
testit "group removemembers" $samba_tool group removemembers $CONFIG gsg newuser,newuser1
testit "group removemembers" $samba_tool group removemembers $CONFIG usg newuser,newuser1
testit "group removemembers" $samba_tool group removemembers $CONFIG ddg newuser,newuser1
testit "group removemembers" $samba_tool group removemembers $CONFIG gdg newuser,newuser1
testit "group removemembers" $samba_tool group removemembers $CONFIG udg newuser,newuser1

#test adding test users to all groups by their cn
#testit "group addmembers" $samba_tool group addmembers $CONFIG dsg "User UT. Tester,User1 UT. Tester"
#testit "group addmembers" $samba_tool group addmembers $CONFIG gsg "User UT. Tester,User1 UT. Tester"
#testit "group addmembers" $samba_tool group addmembers $CONFIG usg "User UT. Tester,User1 UT. Tester"
#testit "group addmembers" $samba_tool group addmembers $CONFIG ddg "User UT. Tester,User1 UT. Tester"
#testit "group addmembers" $samba_tool group addmembers $CONFIG gdg "User UT. Tester,User1 UT. Tester"
#testit "group addmembers" $samba_tool group addmembers $CONFIG udg "User UT. Tester,User1 UT. Tester"

#test removing test users from all groups by their cn
#testit "group removemembers" $samba_tool group removemembers $CONFIG dsg "User UT. Tester,User1 UT. Tester"
#testit "group removemembers" $samba_tool group removemembers $CONFIG gsg "User UT. Tester,User1 UT. Tester"
#testit "group removemembers" $samba_tool group removemembers $CONFIG usg "User UT. Tester,User1 UT. Tester"
#testit "group removemembers" $samba_tool group removemembers $CONFIG ddg "User UT. Tester,User1 UT. Tester"
#testit "group removemembers" $samba_tool group removemembers $CONFIG gdg "User UT. Tester,User1 UT. Tester"
#testit "group removemembers" $samba_tool group removemembers $CONFIG ugg "User UT. Tester,User1 UT. Tester"

#test deletion of the groups
testit "group delete" $samba_tool group delete $CONFIG dsg
testit "group delete" $samba_tool group delete $CONFIG gsg
testit "group delete" $samba_tool group delete $CONFIG usg
testit "group delete" $samba_tool group delete $CONFIG ddg
testit "group delete" $samba_tool group delete $CONFIG gdg
testit "group delete" $samba_tool group delete $CONFIG udg

exit $failed
