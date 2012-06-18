#!/bin/bash

if [ "$1" = "" ]; then
    echo "Please provide version string, eg: 1.2.0"
    exit 1
fi

if [ ! -d "lib/tdb" ]; then
    echo "Run this script from the samba base directory."
    exit 1
fi

git clean -f -x -d lib/tdb
git clean -f -x -d lib/replace

curbranch=`git branch |grep "^*" | tr -d "* "`

version=$1
strver=`echo ${version} | tr "." "-"`

# Checkout the release tag
git branch -f tdb-release-script-${strver} tdb-${strver}
if [ ! "$?" = "0" ];  then
    echo "Unable to checkout tdb-${strver} release"
    exit 1
fi

git checkout tdb-release-script-${strver}

# Test configure agrees with us
confver=`grep "^AC_INIT" lib/tdb/configure.ac | tr -d "AC_INIT(tdb, " | tr -d ")"`
if [ ! "$confver" = "$version" ]; then
    echo "Wrong version, requested release for ${version}, found ${confver}"
    exit 1
fi

# Now build tarball
cp -a lib/tdb tdb-${version}
cp -a lib/replace tdb-${version}/libreplace
pushd tdb-${version}
./autogen.sh
popd
tar cvzf tdb-${version}.tar.gz tdb-${version}
rm -fr tdb-${version}

#Clean up
git checkout $curbranch
git branch -d tdb-release-script-${strver}
