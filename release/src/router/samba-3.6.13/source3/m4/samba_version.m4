dnl
dnl Samba3 build environment - Samba version variables
dnl
dnl Copyright (C) Michael Adam 2008
dnl
dnl Released under the GNU General Public License
dnl http://www.gnu.org/licenses/
dnl
dnl

SMB_VERSION_STRING=`cat $srcdir/include/version.h | grep 'SAMBA_VERSION_OFFICIAL_STRING' | cut -d '"' -f2`
echo "SAMBA VERSION: ${SMB_VERSION_STRING}"

SAMBA_VERSION_GIT_COMMIT_FULLREV=`cat $srcdir/include/version.h | grep 'SAMBA_VERSION_GIT_COMMIT_FULLREV' | cut -d ' ' -f3- | cut -d '"' -f2`
if test -n "${SAMBA_VERSION_GIT_COMMIT_FULLREV}";then
	echo "BUILD COMMIT REVISION: ${SAMBA_VERSION_GIT_COMMIT_FULLREV}"
fi
SAMBA_VERSION_COMMIT_DATE=`cat $srcdir/include/version.h | grep 'SAMBA_VERSION_COMMIT_DATE' | cut -d ' ' -f3-`
if test -n "${SAMBA_VERSION_COMMIT_DATE}";then
	echo "BUILD COMMIT DATE: ${SAMBA_VERSION_COMMIT_DATE}"
fi
SAMBA_VERSION_COMMIT_TIME=`cat $srcdir/include/version.h | grep 'SAMBA_VERSION_COMMIT_TIME' | cut -d ' ' -f3-`
if test -n "${SAMBA_VERSION_COMMIT_TIME}";then
	echo "BUILD COMMIT TIME: ${SAMBA_VERSION_COMMIT_TIME}"

	# just to keep the build-farm gui happy for now...
	echo "BUILD REVISION: ${SAMBA_VERSION_COMMIT_TIME}"
fi

