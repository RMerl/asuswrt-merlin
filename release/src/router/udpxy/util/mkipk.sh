#!/bin/sh
# @(#) utility to create ipkg package for a udpxy distro

# extract architecture tag from file
getarch()
{
    fname=${1:?}

    tag=`file -b $fname | cut -d',' -f2 | tr -d ' '` || return 1
    case "$tag" in
        Intel80386) arch=i686     ;;
        MIPS*)    arch=mipsel     ;;
        *)        arch=$tag       ;;
    esac

    echo $arch
    return 0
}

####
# main
#

if [ $# -lt 1 ]; then
    echo >&2 Usage: $0 udpxy_source_dir package_dest_dir
    exit $?
fi

udpxy_root=${1:?}
destdir=${2:-.}

if [ ! -d "$udpxy_root" -o ! -d "$destdir" ]; then
    echo >&2 "Cannot find [$udpxy_root] or [$destdir] directory"
    exit 1
fi

binfiles="$udpxy_root/udpxy $udpxy_root/udpxrec"

files="$binfiles $udpxy_root/BUILD $udpxy_root/VERSION"
for f in `echo $files`; do
    if [ ! -f "$f" ]; then
        echo >&2 "$0: Cannot find [$f] executable"
        exit 1
    fi
done

if ! type ipkg-build >/dev/null 2>&1; then
    echo >&2 "$0: ipkg-build tool is not accessible"
    exit 1
else
    build=`which ipkg-build` || exit 1
fi

ARCH=`getarch $udpxy_root/udpxy 2>/dev/null` || return $?
[ -n "$MKIPK_DEBUG" ] && echo "Architecture: [${ARCH}]"

#case $EUID in
#    0) SUDO= ;;
#    *) SUDO=sudo ;;
#esac
SUDO=
: ${MKIPK_USE_TAR:='1'}


rc=1; while :
do
    ipkg_dir=${TMPDIR:-/tmp}/ipkg.$$

    ${SUDO} mkdir -p $ipkg_dir/opt/bin $ipkg_dir/CONTROL || break

    UDPXY_NAME="udpxy"
    VERSION=`cat $udpxy_root/VERSION | tr -d '"'`-`cat $udpxy_root/BUILD`
    control_spec=$ipkg_dir/CONTROL/control

    [ -n "$MKIPK_DEBUG" ] && echo >&2 "Creating [$control_spec]"
    ${SUDO} touch $control_spec || break
    ${SUDO} chmod o+w $control_spec || break

    cat <<__EOM_ >$control_spec
Package: $UDPXY_NAME
Architecture: ${ARCH}
Priority: optional
Section: base
Version: ${VERSION}
Maintainer: Pavel Cherenkov<pcherenkov@gmail.com>
Source: http://downloads.sourceforge.net/udpxy/udpxy.${VERSION}.tgz
Description: UDP-to-HTTP multicast traffic relay daemon
Depends:
Suggests:
Conflicts:
__EOM_
    [ $? -eq 0 ] || break

    ${SUDO} cp -P $binfiles  $ipkg_dir/opt/bin || break
    if [ -n "${STRIP}" ]; then
        [ -n "${MKIPK_DEBUG}" ] && echo >&2 "Stripping udpxy executable"
        ${SUDO} ${STRIP} $ipkg_dir/opt/bin/udpxy || break
        [ -n "${MKIPK_DEBUG}" ] && ls -lh $ipkg_dir/opt/bin/udpxy
    fi

    buildopt="-o root -g root"
    [ -n "${MKIPK_USE_TAR}" -a "0" != "${MKIPK_USE_TAR}" ] && \
        buildopt="$buildopt -c"
    ${SUDO} ${build} $buildopt $ipkg_dir $destdir || break

    rc=0; break
done

if [ -z "$MKIPK_DEBUG" ]; then
    ${SUDO} rm -fr $ipkg_dir
else
    echo >&2 "Leaving behind [$ipkg_dir]"
fi

[ -n "${MKIPK_DEBUG}" ] && echo "$0: exiting with rc=$rc"
exit $rc

# __EOF__

