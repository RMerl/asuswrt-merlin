:
# Rmon History testing script
# $Log$
# Revision 5.0  2002/04/20 07:30:01  hardaker
# cvs file version number change
#
# Revision 1.1  2001/05/09 19:36:13  slif
# Include Alex Rozin's Rmon.
#
#

#Only parameter: number of interface (ifIndex) to be tested.
#Default: 1

TSTIF=1
COMPAR="-m ALL localhost public"

if [ "X"${1} = "X" ] ; then
    echo got default parameter : $TSTIF
else
    TSTIF=$1
fi

echo interface ifIndex.$TSTIF will be tested

echo " "
echo 1. create control entry
snmpset $COMPAR historyControlBucketsRequested.4 i 4 historyControlInterval.4 i 3 \
historyControlDataSource.4 o interfaces.ifTable.ifEntry.ifIndex.$TSTIF \
historyControlStatus.4 i 2

snmpwalk $COMPAR historyControlTable
echo " "
echo 2. validate it
snmpset $COMPAR historyControlStatus.4 i 1
snmpwalk $COMPAR historyControlTable
echo "Sleep 3, take it chance to get something"
sleep 3
snmpwalk $COMPAR etherHistoryTable
echo "Sleep 6, take it chance to advance"
sleep 6
snmpwalk $COMPAR etherHistoryTable


echo " "
echo 3. change requested number of buckets
snmpset $COMPAR historyControlBucketsRequested.4 i 2
echo "Sleep 9, take it chance to get something"
sleep 9
snmpwalk $COMPAR etherHistoryTable

echo " "
echo 4. invalidate it
snmpset $COMPAR historyControlStatus.4 i 4
snmpwalk $COMPAR history


echo " "
echo 5. create and validate 2 control entries
snmpset $COMPAR historyControlBucketsRequested.4 i 3 historyControlInterval.4 i 2 \
historyControlDataSource.4 o interfaces.ifTable.ifEntry.ifIndex.$TSTIF \
historyControlStatus.4 i 1
snmpset $COMPAR historyControlBucketsRequested.2 i 2 historyControlInterval.2 i 4 \
historyControlStatus.2 i 1
snmptable $COMPAR historyControlTable
echo "Sleep 12, take them chance to get something"
sleep 12
snmpwalk $COMPAR etherHistoryTable

echo " "
echo 6. create entry and let it to be aged
snmpset $COMPAR historyControlStatus.3 i 2
snmptable $COMPAR historyControlTable
echo "Sleep 61, take it chance to be aged"
sleep 61
snmptable $COMPAR historyControlTable

echo " "
echo 7. clean everything
snmpset $COMPAR historyControlStatus.2 i 4
snmpset $COMPAR historyControlStatus.4 i 4
snmpwalk $COMPAR history


echo " "
echo "Goodbye, I'm a gonner"
echo " "

