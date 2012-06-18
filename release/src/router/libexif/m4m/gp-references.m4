dnl
dnl Define external references
dnl
dnl Define once, use many times. 
dnl No more URLs and Mail addresses in translated strings and stuff.
dnl

AC_DEFUN([GP_REF],[
AC_SUBST([$1],["$2"])
AC_DEFINE_UNQUOTED([$1],["$2"],[$3])
])

AC_DEFUN([GP_REFERENCES],
[

GP_REF(	[URL_GPHOTO_HOME], 
	[http://www.gphoto.org/], 
	[gphoto project home page])dnl

GP_REF(	[URL_GPHOTO_PROJECT], 
	[http://sourceforge.net/projects/gphoto], 
	[gphoto sourceforge project page])

GP_REF(	[URL_DIGICAM_LIST],
	[http://www.teaser.fr/~hfiguiere/linux/digicam.html],
	[camera list with support status])

GP_REF(	[URL_JPHOTO_HOME],
	[http://jphoto.sourceforge.net/],
	[jphoto home page])

GP_REF(	[URL_USB_MASSSTORAGE],
	[http://www.linux-usb.org/USB-guide/x498.html],
 	[information about using USB mass storage])

GP_REF(	[MAIL_GPHOTO_DEVEL],
	[<gphoto-devel@lists.sourceforge.net>],
	[gphoto development mailing list])

GP_REF(	[MAIL_GPHOTO_USER],
	[<gphoto-user@lists.sourceforge.net>],
	[gphoto user mailing list])

GP_REF(	[MAIL_GPHOTO_TRANSLATION],
	[<gphoto-translation@lists.sourceforge.net>],
	[gphoto translation mailing list])

])
