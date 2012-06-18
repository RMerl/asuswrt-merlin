#!/bin/sh
#
# Copyright (C) Shirish A Kalele 2000
#
# Builds a Samba package from the samba distribution. 
# By default, the package will be built to install samba in /usr/local
# Change the INSTALL_BASE variable to change this: will modify the pkginfo 
# and samba.server files to point to the new INSTALL_BASE
#
INSTALL_BASE=/usr/local

add_dynamic_entries() 
{
  # First build the codepages and append codepage entries to prototype
  echo "#\n# Codepages \n#"
  echo d none samba/lib/codepages 0755 root other

  # Check if make_smbcodepage exists
  if [ ! -f $DISTR_BASE/source/bin/make_smbcodepage ]; then
    echo "Could not find $DISTR_BASE/source/bin/make_smbcodepage to generate codepages.\n\
Please create the binaries before packaging." >&2
    exit 1
  fi

  # PUll in the codepage_def list from source/codepages/codepage_def.*
  list=`find $DISTR_BASE/source/codepages -name "codepage_def.*" | sed 's|^.*codepage_def\.\(.*\)|\1|'`
  for cpdef in $list
  do
    $DISTR_BASE/source/bin/make_smbcodepage c $cpdef $DISTR_BASE/source/codepages/codepage_def.$cpdef $DISTR_BASE/source/codepages/codepage.$cpdef
    echo f none samba/lib/codepages/codepage.$cpdef=source/codepages/codepage.$cpdef 0644 root other
  done

  # Create unicode maps 
  if [ ! -f $DISTR_BASE/source/bin/make_unicodemap ]; then
    echo "Missing $DISTR_BASE/source/bin/make_unicodemap. Aborting." >&2
    exit 1
  fi

  # Pull in all the unicode map files from source/codepages/CP*.TXT
  list=`find $DISTR_BASE/source/codepages -name "CP*.TXT" | sed 's|^.*CP\(.*\)\.TXT|\1|'`
  for umap in $list
  do
    $DISTR_BASE/source/bin/make_unicodemap $umap $DISTR_BASE/source/codepages/CP$umap.TXT $DISTR_BASE/source/codepages/unicode_map.$umap
    echo f none samba/lib/codepages/unicode_map.$umap=source/codepages/unicode_map.$umap 0644 root other
  done

  # Add the binaries, docs and SWAT files

  echo "#\n# Binaries \n#"
  cd $DISTR_BASE/source/bin
  for binfile in *
  do
    if [ -f $binfile ]; then
      echo f none samba/bin/$binfile=source/bin/$binfile 0755 root other
    fi
  done

  echo "#\n# HTML documentation \n#"
  # Create the directories 
  cd $DISTR_BASE
  list=`find docs/htmldocs -type d`
  for docdir in $list
  do
    if [ -d $docdir ]; then
      echo d none samba/$docdir 0755 root other
    fi
  done

  list=`find docs/htmldocs -type f`
  for htmldoc in $list
  do
    if [ -f $htmldoc ]; then
      echo f none samba/$htmldoc=$htmldoc 0644 root other
    fi
  done

  # Create a symbolic link to the Samba book in docs/ for beginners
  echo 's none samba/docs/samba_book=htmldocs/using_samba'

  echo "#\n# Text Docs \n#"
  echo d none samba/docs/textdocs 0755 root other
  cd $DISTR_BASE/docs/textdocs
  for textdoc in *
  do 
    if [ -f $textdoc ]; then
      echo f none samba/docs/textdocs/$textdoc=docs/textdocs/$textdoc 0644 root other
    fi
  done

  echo "#\n# SWAT \n#"
  cd $DISTR_BASE
  list=`find swat -type d`
  for i in $list
  do
    echo "d none samba/$i 0755 root other"
  done
  list=`find swat -type f`
  for i in $list
  do
    echo "f none samba/$i=$i 0644 root other"
  done
  
  echo "#\n# HTML documentation for SWAT\n#"
  cd $DISTR_BASE/docs/htmldocs
  for htmldoc in *
  do
    if [ -f $htmldoc ]; then
      echo f none samba/swat/help/$htmldoc=docs/htmldocs/$htmldoc 0644 root other
    fi
  done

  echo "#\n# Using Samba Book files for SWAT\n#"
  cd $DISTR_BASE/docs/htmldocs
 
# set up a symbolic link instead of duplicating the book tree
  echo 's none samba/swat/using_samba=../docs/htmldocs/using_samba'

#  list=`find using_samba -type d`
#  for bookdir in $list
#  do
#    echo d none samba/swat/$bookdir 0755 root other
#  done
#  
#  list=`find using_samba -type f`
#  for bookdoc in $list
#  do
#    echo f none samba/swat/$bookdoc=docs/htmldocs/$bookdoc 0644 root other
#  done

}

if [ $# = 0 ]
then
	# Try to guess the distribution base..
	CURR_DIR=`pwd`
	DISTR_BASE=`echo $CURR_DIR | sed 's|\(.*\)/packaging.*|\1|'`
	echo "Assuming Samba build tree rooted at $DISTR_BASE.."
else
	DISTR_BASE=$1
fi

#
if [ ! -d $DISTR_BASE ]; then
	echo "Source build directory $DISTR_BASE does not exist."
	exit 1
fi

# Set up the prototype file from the static_prototype
if [ -f prototype ]; then
	rm prototype
fi

# Set the INSTALL_BASE in pkginfo, samba.server
	cp pkginfo pkginfo.bak
	cp samba.server samba.server.bak
	sed "s|BASEDIR=.*|BASEDIR=$INSTALL_BASE|g" pkginfo.bak > pkginfo
	sed "s|BASE=.*|BASE=$INSTALL_BASE/samba|g" samba.server.bak > samba.server
	rm -f pkginfo.bak samba.server.bak

cp static_prototype prototype

# Add the dynamic part to the prototype file
(add_dynamic_entries >> prototype)

[ $? != 0 ] && exit 1

# Create the package
pkgmk -o -d /tmp -b $DISTR_BASE -f prototype
if [ $? = 0 ]
then
	pkgtrans /tmp samba.pkg samba
fi
echo The samba package is in /tmp
rm -f prototype
