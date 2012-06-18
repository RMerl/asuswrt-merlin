#!/bin/sh
# Run this to generate configure, Makefile.in's, etc

(autoreconf --version) < /dev/null > /dev/null 2>&1 || {
  (autoconf --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have the GNU Build System (autoconf, automake, "
    echo "libtool, etc) to update the ntfs-3g build system.  Download the "
    echo "appropriate packages for your distribution, or get the source "
    echo "tar balls from ftp://ftp.gnu.org/pub/gnu/."
    exit 1
  }
  echo
  echo "**Error**: Your version of autoconf is too old (you need at least 2.57)"
  echo "to update the ntfs-3g build system.  Download the appropriate "
  echo "updated package for your distribution, or get the source tar ball "
  echo "from ftp://ftp.gnu.org/pub/gnu/."
  exit 1
}

echo Running autoreconf --verbose --install --force
autoreconf --verbose --install --force
