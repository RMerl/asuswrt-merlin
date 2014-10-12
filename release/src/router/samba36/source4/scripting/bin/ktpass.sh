#!/bin/sh
# vim: expandtab
#
# Copyright (C) Matthieu Patou <mat@matws.net>  2010
#
#
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

name="ktpass.sh"
TEMP=`getopt -o h --long princ:,pass:,out:,host:,ptype:,enc:,path-to-ldbsearch: \
     -n "$name" -- "$@"`
eval set -- "$TEMP"

usage () {
  echo -ne "$name --out <keytabfile> --princ <principal> --pass <password>|*\n"
  echo -ne "      [--host hostname] [--enc <encryption>]\n"
  echo -ne "      [--ptype <type>] [--path-to-ldbsearch <path>]\n"
  echo -ne "\nEncoding should be one of:\n"
  echo -ne " * des-cbc-crc\n"
  echo -ne " * des-cbc-md5\n"
  echo -ne " * rc4-hmac (default)\n"
  echo -ne " * aes128-cts\n"
  echo -ne " * aes256-cts\n"
  exit 0
}
while true ; do
  case "$1" in
    --out) outfile=$2 ; shift 2 ;;
    --princ) princ=$2 ; shift 2 ;;
    --pass) pass=$2 ; shift 2 ;;
    --host) host=$2 ; shift 2 ;;
    --ptype) shift 2 ;;
    --enc) enc=$2; shift 2;;
    --path-to-ldbsearch) path="$2/"; shift 2;;
    -h) usage;;
    --) shift ; break ;;
    *) echo "Internal error!" ; exit 1 ;;
  esac
done
#RC4-HMAC-NT|AES256-SHA1|AES128-SHA
if [ -z "$enc" ]; then
    enc="rc4-hmac"
fi
if [ -z "$path" ]; then
  path=`dirname $0`/../bin/
  if [ ! -f ${path}ldbsearch ]; then
    path=`dirname $0`/../../bin/
  fi
fi
if [ -z "$outfile" -o -z "$princ" -o -z "$pass" ]; then
  echo "At least one mandatory parameter (--out, --princ, --pass) was not specified"
  usage
fi
if [ -z $host ]; then
  host=`hostname`
fi

kvno=`${path}ldbsearch -H ldap://$host "(|(samaccountname=$princ)(serviceprincipalname=$princ)(userprincipalname=$princ))" msds-keyversionnumber  -k 1 -N 2>/dev/null| grep -i msds-keyversionnumber`
if [ x"$kvno" = x"" ]; then
  echo -ne "Unable to find kvno for principal $princ\n"
  echo -ne " check that you are authentified with kerberos\n"
  exit 1
else
  kvno=`echo $kvno | sed 's/^.*: //'`
fi

if [ "$pass" = "*" ]; then
  echo -n "Enter password for $princ: "
  stty -echo
  read pass
  stty echo
  echo ""
fi

ktutil >/dev/null <<EOF
add_entry -password -p $princ -k $kvno -e $enc
$pass
wkt $outfile
EOF

if [ $? -eq 0 ]; then
   echo "Keytab file $outfile created with success"
else
  echo "Error while creating the keytab file $outfile"
fi
