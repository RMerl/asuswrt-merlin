#!/bin/sh

echo "chmod +x debian/rules"
chmod +x debian/rules

# in some environments the '-rfakeroot' can cause a failure (e.g. when
# building as root). If so, remove that argument from the following:
echo "dpkg-buildpackage -b -rfakeroot"
dpkg-buildpackage -b -rfakeroot

# If the above succeeds then the ".deb" binary package is placed in the
# parent directory.
