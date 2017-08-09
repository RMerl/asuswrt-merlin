# tests for charsets for Samba3

from Configure import conf

@conf
def CHECK_SAMBA3_CHARSET(conf, crossbuild=False):
    '''Check for default charsets for Samba3
    '''
    if conf.CHECK_ICONV(define='HAVE_NATIVE_ICONV'):
        default_dos_charset=False
        default_display_charset=False
        default_unix_charset=False

        # check for default dos charset name
        for charset in ['CP850', 'IBM850']:
            if conf.CHECK_CHARSET_EXISTS(charset, headers='iconv.h'):
                default_dos_charset=charset
                break

        # check for default display charset name
        for charset in ['ASCII', '646']:
            if conf.CHECK_CHARSET_EXISTS(charset, headers='iconv.h'):
                default_display_charset=charset
                break

        # check for default unix charset name
        for charset in ['UTF-8', 'UTF8']:
            if conf.CHECK_CHARSET_EXISTS(charset, headers='iconv.h'):
                default_unix_charset=charset
                break

	# At this point, we have a libiconv candidate. We know that
	# we have the right headers and libraries, but we don't know
	# whether it does the conversions we want. We can't test this
	# because we are cross-compiling. This is not necessarily a big
	# deal, since we can't guarantee that the results we get now will
	# match the results we get at runtime anyway.
	if crossbuild:
	    default_dos_charset="CP850"
	    default_display_charset="ASCII"
	    default_unix_charset="UTF-8"
            # TODO: this used to warn about the set charset on cross builds

        conf.DEFINE('DEFAULT_DOS_CHARSET', default_dos_charset, quote=True)
        conf.DEFINE('DEFAULT_DISPLAY_CHARSET', default_display_charset, quote=True)
        conf.DEFINE('DEFAULT_UNIX_CHARSET', default_unix_charset, quote=True)

    else:
        conf.DEFINE('DEFAULT_DOS_CHARSET', "ASCII", quote=True)
        conf.DEFINE('DEFAULT_DISPLAY_CHARSET', "ASCII", quote=True)
        conf.DEFINE('DEFAULT_UNIX_CHARSET', "UTF8", quote=True)

