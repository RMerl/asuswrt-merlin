#!/bin/ksh
#
# elfdepend.sh
#
# given a path, this scripts searches for ELF binaries and libraries
# and generates package dependency file entries according to ther dependencies
#
# Usage: elfdepend <ELF-binary>|<directory>
#
# 2002/11 Stefan.Radman@CTBTO.ORG
#
# /var/sadm/install/contents format:
#
# /dev d none 0775 root sys SUNWcsr SUNWcsd
# path type class mode owner group packages
# /etc/.login f renamenew 0644 root sys 516 37956 904647695 SUNWcsr
# /etc/acct/holidays e preserve 0664 bin bin 289 22090 904647603 SUNWaccr
# path type class mode owner group x x x packages
# /bin=./usr/bin s none SUNWcsr
# path=link type class packages
# /devices/pseudo/clone@0:hme c none 11 7 0600 root sys SUNWcsd
# path type class x x owner mode packages
#
# types e (sed), v (volatile) have same format like type f (file)
# type l (hardlink) has same format like type s (symlink)
#
prog=`basename $0`
LAST_CHANCE=/opt/OSS/lib

if [ -d "$1" ] ; then
  find $1 -type f -exec file {} \;
elif [ -x "$1" ] ; then
  file $1
else
  echo 1>&2 "usage: $0 <directory>|<ELF executable>"
  exit 1
fi | awk '$2 == "ELF" { print }' | cut -d: -f1 |\
while read elf
do
  ldd "$elf" | while read lib x path
  do
    [ -z "$path" ] && continue
    if [ "$path" = '(file not found)' ]
    then
      if [ -x $LAST_CHANCE/$lib ]
      then
        path="$LAST_CHANCE/$lib"
      else
        echo "# $prog: $lib $x $path"
        continue # not found
      fi
    fi
    echo "$path"
    # need symlink handling here - see /usr/platform/SUNW,*/lib/*
  done
done | sort -u | while read libpath other
do
  [ "$libpath" = "#" ] && echo "$libpath $other" && continue # error message
  set -- `grep "^$libpath[ =]"  /var/sadm/install/contents | head -1`
  path=$1; type=$2
  case $type in
    f) # file
      shift 9 # first package
      ;;
    s|l) # link
      shift 3 # first package
      ;;
    '') # not found
      echo "# $prog: $libpath is not associated with any package"
      continue
      ;;
    *) # dubious file type
      echo "# $prog: path $1 has dubious file type $2"
      continue
      ;;
  esac
  set -- `echo $1 | tr : ' '`
  echo $1 # strip off classes
done | sort -u | while read pkg other
do
  if [ "$pkg" = "#" ] ; then # error message
    echo 1>&2 "$other" # goes to stderr
    continue
  fi
  eval `pkgparam -v $pkg PKG NAME`
  printf "P  %-15s%s\n" "$PKG" "$NAME"
done
