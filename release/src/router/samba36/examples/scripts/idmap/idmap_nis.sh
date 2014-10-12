#!/bin/bash
# idmap script to map SIDs to UIDs/GIDs using NIS
# tridge@samba.org June 2009

DOMAIN=$(ypdomainname)

(
    date
    echo $*
) >> /var/log/samba/idmap.log

cmd=$1
shift

PATH=/usr/bin:bin:$PATH

shopt -s nocasematch || {
    echo "shell option nocasematch not supported"
    exit 1
}

# map from a domain and name to a uid/gid
map_name() {
    domain="$1"
    name="$2"
    ntype="$3"
    case $ntype in
	1)
	    rtype="UID"
	    map="passwd"
	    ;;
	2)
	    rtype="GID"
	    map="group"
	    ;;
	*)
	    echo "ERR: bad name type $ntype"
	    exit 1
	    ;;
    esac
    id=$(ypmatch "$name" "$map".byname 2>/dev/null | cut -d: -f3)
    [ -z "$id" ] && {
	echo "ERR: bad match for $name in map $map"
	exit 1
    }
    echo "$rtype":"$id"
}

# map from a unix id to a name
map_id() {
    ntype="$1"
    id="$2"
    case $ntype in
	UID)
	    map="passwd.byuid"
	    ;;
	GID)
	    map="group.bygid"
	    ;;
	*)
	    echo "ERR: bad name type $ntype"
	    exit 1
	    ;;
    esac
    name="$(ypmatch "$id" "$map" 2>/dev/null | cut -d: -f1)"
    [ -z "$name" ] && {
	echo "ERR: bad match for $name in map $map"
	exit 1
    }
    echo "$name"
}


case $cmd in
    SIDTOID)
	sid=$1
	rid=`echo $sid | cut -d- -f8`
	[ -z "$rid" ] && {
	    echo "ERR: bad rid in SID $sid"
	    exit 1
	}
	
	unset _NO_WINBINDD
	# oh, this is ugly. Shell is just not meant for parsing text
	fullname=`wbinfo -s $sid 2> /dev/null`
	domain=`echo $fullname | cut -d'\' -f1`
	[[ "$domain" = $DOMAIN ]] || {
	    echo "ERR: bad domain $domain"
	    exit 1
	}
	name=`echo $fullname | cut -d'\' -f2`
	nwords=`echo $name | wc -w`
	ntype=`echo $name | cut -d' ' -f$nwords`
	nminusone=`expr $nwords - 1`
	name=`echo $name | cut -d' ' -f-$nminusone`
	[ -z "$name" ] && {
	    echo "ERR: bad name $fullname for SID $sid"
	    exit 1
	}
	map_name "$domain" "$name" "$ntype"
	;;
    IDTOSID)
	ntype=$1
	id=$2
	name="$(map_id "$ntype" "$id")"
	sid="$(wbinfo -n "$name" 2>/dev/null | cut -d' ' -f1)"
	[ -z "$sid" ] && {
	    echo "ERR: name $name not found in ADS"
	    exit 1
	}
	echo "SID:$sid"
	;;
    *)
	echo "ERR: Unknown command $cmd"
	exit 1;
	;;
esac

exit 0
