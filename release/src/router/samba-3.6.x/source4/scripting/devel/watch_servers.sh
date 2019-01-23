#!/bin/sh

[ $# -ge 3 ] || {
    echo "Usage: watch_servers.sh DB1 DB2 PASSWORD SEARCH <attrs>"
    exit 1
}

host1="$1"
host2="$2"
password="$3"
search="$4"
shift 4

watch -n1 "echo '$host1:'; bin/ldbsearch -S -H $host1 -Uadministrator%$password '$search' description $* | egrep -v '^ref|Ref|returned|entries|referrals' | uniq; echo; echo '$host2:'; bin/ldbsearch -S -H $host2 -Uadministrator%$password '$search' description $* | egrep -v '^ref|Ref|returned|entries|referrals' | uniq;"
