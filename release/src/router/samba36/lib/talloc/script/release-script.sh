#!/bin/bash

LNAME=talloc
LINCLUDE=talloc.h

if [ "$1" = "" ]; then
    echo "Please provide version string, eg: 1.2.0"
    exit 1
fi

if [ ! -d "lib/${LNAME}" ]; then
    echo "Run this script from the samba base directory."
    exit 1
fi

curbranch=`git branch |grep "^*" | tr -d "* "`

version=$1
strver=`echo ${version} | tr "." "-"`

# Checkout the release tag
git branch -f ${LNAME}-release-script-${strver} ${LNAME}-${strver}
if [ ! "$?" = "0" ];  then
    echo "Unable to checkout ${LNAME}-${strver} release"
    exit 1
fi

function cleanquit {
    #Clean up
    git checkout $curbranch
    git branch -d ${LNAME}-release-script-${strver}
    exit $1
}

# NOTE: use cleanquit after this point
git checkout ${LNAME}-release-script-${strver}

# Test configure agrees with us
confver=`grep "^AC_INIT" lib/${LNAME}/configure.ac | tr -d "AC_INIT(${LNAME}, " | tr -d ")"`
if [ ! "$confver" = "$version" ]; then
    echo "Wrong version, requested release for ${version}, found ${confver}"
    cleanquit 1
fi

# Check exports and signatures are up to date
pushd lib/${LNAME}
./script/abi_checks.sh ${LNAME} ${LINCLUDE}
abicheck=$?
popd
if [ ! "$abicheck" = "0" ]; then
    echo "ERROR: ABI Checks produced warnings!"
    cleanquit 1
fi

git clean -f -x -d lib/${LNAME}
git clean -f -x -d lib/replace

# Now build tarball
cp -a lib/${LNAME} ${LNAME}-${version}
cp -a lib/replace ${LNAME}-${version}/libreplace
pushd ${LNAME}-${version}
./autogen.sh
popd
tar cvzf ${LNAME}-${version}.tar.gz ${LNAME}-${version}
rm -fr ${LNAME}-${version}

cleanquit 0

