#!/bin/sh
# do the funky auto* stuff

echo "Generating configure script using autoconf, automake and gettext"

# only call gettextize if developer uses a new version of gettext
case $1 in

    "--gettext")
	gettextize --copy --intl --no-changelog	;;

esac

aclocal -I m4
autoconf
autoheader
automake --add-missing --gnu

echo "Now you are ready to run ./configure with the desired options"
