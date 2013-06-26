#!/bin/bash

if [ "$1" = "" ]; then
    echo "Please provide version string, eg: 1.2.0"
    exit 1
fi

if [ ! -d "lib/tevent" ]; then
    echo "Run this script from the samba base directory."
    exit 1
fi

git clean -f -x -d lib/tevent
git clean -f -x -d lib/replace

curbranch=`git-branch |grep "^*" | tr -d "* "`

version=$1
strver=`echo ${version} | tr "." "-"`

# Checkout the release tag
git branch -f tevent-release-script-${strver} tevent-${strver}
if [ ! "$?" = "0" ];  then
    echo "Unable to checkout tevent-${strver} release"
    exit 1
fi

git checkout tevent-release-script-${strver}

# Test configure agrees with us
confver=`grep "^AC_INIT" lib/tevent/configure.ac | tr -d "AC_INIT(tevent, " | tr -d ")"`
if [ ! "$confver" = "$version" ]; then
    echo "Wrong version, requested release for ${version}, found ${confver}"
    exit 1
fi

# Now build tarball
cp -a lib/tevent tevent-${version}
cp -a lib/replace tevent-${version}/libreplace
pushd tevent-${version}
./autogen.sh
popd
tar cvzf tevent-${version}.tar.gz tevent-${version}
rm -fr tevent-${version}

#Clean up
git checkout $curbranch
git branch -d tevent-release-script-${strver}
