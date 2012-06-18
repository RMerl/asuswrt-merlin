#!/usr/bin/sh
#- Below variables are editable.
#-  VERSION     Script tries to set the version automatically if it's empty
#-              string. The version number is assumed to be in
#-              ../../source/include/version.h. Should this fail to find correct
#-              version, set it manually to override automatic search.
#-  BIN         List of binaries from ../../source/bin. Note: do not include
#-              swat here for it is a part of Samba.swat fileset
#-  SCRIPT      List of scripts
#-  OS_REVISION The regular expression to determine the supported OS version.
#-              The default versions are HP-UX 10.10 or later as well as 11.*.
#-              You can modify this to reflect the OS you compile on. For
#-              instance, to support any 10.? and 11.? releases, use the
#-              following string:
#-                  '?.10.*|?.11.*
    VERSION=""

        BIN="smbd nmbd smbclient testparm testprns smbstatus\
             rpcclient smbpasswd make_smbcodepage nmblookup make_printerdef"

     SCRIPT="smbtar addtosmbpass convert_smbpasswd"

 OS_RELEASE='?.10.[2-9]?|?.11.*'


#- Below variables should be exported from create_package.sh

if [ -z "$PSF" ]
then
  PSF=samba.psf
fi

if [ -z "$DEPOT" ]
then
  DEPOT=samba.depot
fi

if [ -z "$PRODUCT" ]
then
  PRODUCT=Samba
fi

#--------------------------------------------------------------------------
CODEPAGES=""
if [ -z "$VERSION" ]
then
  echo "Deducing Samba version from version.h ... \c"
  VERSION=`grep VERSION ../../source/include/version.h | awk '{print $3}' |\
           tr -d '"'`
  if [ $? -ne 0 -o -z "$VERSION" ]
  then
    echo "failed."
    echo "Cannot find Samba version. Edit gen_psf.sh and set VERSION"
    echo "variable manually."
    exit 1
  else
    echo "$VERSION"
  fi
fi

echo "Creating list of codepage definitions ..."

#- create codepages from definition and add them to PSF file
for a in ../../source/codepages/codepage_def.[0-9][0-9][0-9]
do
  b=${a##../../source/codepages/codepage_def.}
  CODEPAGES="$CODEPAGES $b"
done
echo "\t$CODEPAGES"

echo "Running make_smbcodepage on codepage definitions ... \c"

mkdir codepage >/dev/null 2>&1
for a in $CODEPAGES
do
../../source/bin/make_smbcodepage c $a ../../source/codepages/codepage_def.$a\
                                    codepage/codepage.$a
done
echo "done."

#- HP-UX uses slightly different section numbers for man pages. The following
#- compares to "normal" sections:
#-
#-             Section        HP-UX section
#-                 1          1                user commands
#-                 5          4                files
#-                 7          5                concepts
#-                 8          1m               administration commands
#- NOTE:
#- Sed expressions used in below loops replaces original section references
#- inside man page with HP-UX section references. Assumption is that
#- only numbers in brackets are section references and nothing else.
#- So far I did not see the man pages corrupted by replacing anything
#- else but section references.

mkdir man >/dev/null 2>&1
echo "Coverting man pages to HP-UX numbering ..."
echo "\t Sections 1 \c"
for a in ../../docs/manpages/*.1
do
  sed -e 's/^[.]TH \([^ ][^ ]*\) .*/.TH \1 1/'\
      -e '1a\
.ds )H Samba Team'\
      -e "1a\\
.ds ]W $VERSION"\
      -e 's/(8)/(1m)/g' \
      -e 's/(5)/(4)/g' \
      -e 's/(7)/(5)/g' \
      $a >man/`basename $a`
done
echo "1m \c"
for a in ../../docs/manpages/*.8
do
  b=`basename $a`
  c=${b%.8}
  sed -e 's/^[.]TH \([^ ][^ ]*\) .*/.TH \1 1M/'\
      -e '1a\
.ds )H Samba Team'\
      -e "1a\\
.ds ]W $VERSION"\
      -e 's/(8)/(1m)/g' \
      -e 's/(5)/(4)/g' \
      -e 's/(7)/(5)/g' \
      $a >man/$c.1m
done
echo "4 \c"
for a in ../../docs/manpages/*.5
do
  b=`basename $a`
  c=${b%.5}
  sed -e 's/^[.]TH \([^ ][^ ]*\) .*/.TH \1 4/'\
      -e '1a\
.ds )H Samba Team'\
      -e "1a\\
.ds ]W $VERSION"\
      -e 's/(8)/(1m)/g' \
      -e 's/(5)/(4)/g' \
      -e 's/(7)/(5)/g' \
      $a >man/$c.4
done
echo "5"
for a in ../../docs/manpages/*.7
do
  b=`basename $a`
  c=${b%.7}
  sed -e 's/^[.]TH \([^ ][^ ]*\) .*/.TH \1 5/'\
      -e '1a\
.ds )H Samba Team'\
      -e "1a\\
.ds ]W $VERSION"\
      -e 's/(8)/(1m)/g' \
      -e 's/(5)/(4)/g' \
      -e 's/(7)/(5)/g' \
      $a >man/$c.5
done

echo "Creating product specification file:"
echo "\tVendor and product description"
#- vendor and header of product definition
cat <<_EOF_ >$PSF
vendor
	tag   	Samba
	title 	Samba Team
	description <vendor_description
end

product
  tag   	$PRODUCT
  title		Samba Server
  description	< ../../WHATSNEW.txt
  copyright	"Copyright (c) 1998 Samba Team. See COPYING for details."
  readme	< ../../README
  revision	$VERSION
  machine_type	*
  os_name	HP-UX
  os_release	$OS_RELEASE
  os_version	*
  directory	/
  is_locatable	false
  vendor_tag	Samba

_EOF_

echo "\tFileset $PRODUCT.core"

cat <<_EOF_ >>$PSF
  fileset
    tag			core
    title		Samba server core components
    revision		$VERSION
    is_kernel		false
    is_reboot		false
    is_secure		false
    configure		configure.bin
    unconfigure		unconfigure.bin

    file    -m 0755 -o root -g sys / /opt/samba/
    file    -m 0755 -o root -g sys / /opt/samba/bin/
    file    -m 0755 -o root -g sys / /opt/samba/lib/
    file    -m 0755 -o root -g sys / /opt/samba/lib/codepages/
    file    -m 0755 -o root -g sys / /opt/samba/newconfig/
    file    -m 0755 -o root -g sys / /opt/samba/newconfig/examples/
    file    -m 0700 -o root -g sys / /opt/samba/private/
    file    -m 0755 -o root -g sys / /var/opt/samba/
    file    -m 0755 -o root -g sys / /var/opt/samba/locks/

    file    -m 0444 -o root -g sys ../../COPYING /opt/samba/COPYING
    file    -m 0555 -o root -g sys ./samba.boot /sbin/init.d/samba
    file    -m 0444 -o root -g sys ./samba.config /opt/samba/newconfig/samba.config

    file    -m 0444 -o root -g sys ../../examples/smb.conf.default /opt/samba/newconfig/examples/smb.conf.default
    file    -m 0444 -o root -g sys ../../examples/simple/smb.conf /opt/samba/newconfig/examples/smb.conf.simple
    file    -m 0444 -o root -g sys ../../examples/dce-dfs/smb.conf /opt/samba/newconfig/examples/smb.conf.dce-dfs

    directory ../../source/bin=/opt/samba/bin
_EOF_
  for a in $BIN
  do
    echo "    file    -m 0555 -o root -g sys $a" >>$PSF
  done

  echo "    directory ../../source/script=/opt/samba/bin" >>$PSF

  for a in $SCRIPT
  do
    echo "    file    -m 0555 -o root -g sys $a" >>$PSF
  done

  echo "    directory ./codepage=/opt/samba/lib/codepages" >> $PSF

  for a in $CODEPAGES
  do
    echo "    file    -m 0444 -o root -g sys codepage.$a" >>$PSF
  done
  echo "  end" >>$PSF
#- end of fileset CORE

echo "\tFileset $PRODUCT.man"
#- Man pages
cat <<_EOF_ >>$PSF
  fileset
    tag             man
    title           Samba server man pages
    revision        $VERSION
    is_kernel       false
    is_reboot       false
    is_secure       false
    configure       configure.man

    file    -m 0755 -o root -g sys / /opt/samba/man/
    file    -m 0755 -o root -g sys / /opt/samba/man/man1/
    file    -m 0755 -o root -g sys / /opt/samba/man/man1m/
    file    -m 0755 -o root -g sys / /opt/samba/man/man4/
    file    -m 0755 -o root -g sys / /opt/samba/man/man5/

_EOF_

#- HP-UX uses slightly different section numbers for man pages. The following
#- compares to "normal" sections:
#-
#-             Section        HP-UX section
#-                 1          1                user commands
#-                 5          4                files
#-                 7          5                concepts
#-                 8          1m               administration commands

  for a in man/*.1
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $a /opt/samba/man/man1/$b" >>$PSF
  done

  for a in man/*.1m
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $a /opt/samba/man/man1m/$b" >>$PSF
  done

  for a in man/*.4
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $a /opt/samba/man/man4/$b" >>$PSF
  done

  for a in man/*.5
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $a /opt/samba/man/man5/$b" >>$PSF
  done

  echo "  end" >>$PSF

echo "\tFileset $PRODUCT.swat"

cat <<_EOF_ >>$PSF

  fileset
    tag             swat
    title           Samba Web-based administration tool
    revision        $VERSION
    is_kernel       false
    is_reboot       false
    is_secure       false
    prerequisite    Samba.core
    configure       configure.swat
    unconfigure     unconfigure.swat

    file    -m 0755 -o root -g sys / /opt/samba/swat/
    file    -m 0755 -o root -g sys / /opt/samba/swat/help/
    file    -m 0755 -o root -g sys / /opt/samba/swat/images/
    file    -m 0755 -o root -g sys / /opt/samba/swat/include/

    directory ../../swat=/opt/samba/swat
    file    -m 0444 -o root -g sys README

    directory ../../source/bin=/opt/samba/bin
    file    -m 0555 -o root -g sys swat

    directory ../../swat/images=/opt/samba/swat/images
_EOF_

  for a in ../../swat/images/*.gif
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $b" >>$PSF   
  done

  echo "    directory ../../swat/help=/opt/samba/swat/help" >>$PSF

  for a in ../../swat/help/*.html
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $b" >>$PSF   
  done

  echo "    directory ../../docs/htmldocs=/opt/samba/swat/help" >>$PSF

  for a in ../../docs/htmldocs/*.html
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $b" >>$PSF   
  done
  
  echo "    directory ../../swat/include=/opt/samba/swat/include" >>$PSF

  for a in ../../swat/include/*.html
  do
    b=`basename $a`
    echo "    file    -m 0444 -o root -g sys $b" >>$PSF   
  done

cat <<_EOF_ >>$PSF
  end

end
_EOF_

