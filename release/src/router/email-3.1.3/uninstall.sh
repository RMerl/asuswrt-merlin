#!/bin/sh

help ()
{
cat <<EOF

This script is written and maintained by Dean Jones.
Please send all bugs to <dean@cleancode.org>
Options are described below and all are MANDITORY.

    --bindir dir        Specifies the directory to find the binary to remove.
    --mandir dir        Specifies the directory to find the manual page to remove.
    --sysconfdir dir    Specifies the directory to find the config files to remove.
    --docdir dir        Specifies the directory to find the documentation to remove.
    --version version   Specifies the version of the program
    --help              Duh.

EOF
}

# Get command line arguments
while [ "$1" ]
do
    case "$1" in
        "-b" | "--bindir" | "-bindir")
            if [ -z "$2" ]
            then
                echo "error: option '$1' requires an argument"
                exit 2
            fi

            shift
            bindir="$1"
            ;;

        "-m" | "--mandir" | "-mandir")
            if [ -z "$2" ]
            then
                echo "error: option '$1' requires an argument"
                exit 2
            fi

            shift
            mandir="$1"
            ;;

        "-s" | "--sysconfdir" | "-sysconfdir")
            if [ -z "$2" ]
            then
                echo "error: option '$1' requires an argument"
                exit 2
            fi

            shift
            sysconfdir="$1"
            ;;

        "-d" | "--docdir" | "-docdir")
            if [ -z "$2" ]
            then
                echo "error: option '$1' requires an argument"
                exit 2
            fi

            shift
            docdir="$1"
            ;;

        "-v" | "--version" | "-version")
            if [ -z "$2" ]
            then
                echo "error: option '$1' requires an argument"
                exit 2
            fi

            shift
            VERSION="$1"
            ;;

        "-h" | "--help" | "-help" )
            help
            ;;

        * )
            help
            ;;
    esac

    shift
done

if [ -z "$bindir" ] || [ -z "$mandir" ] || [ -z "$sysconfdir" ] || [ -z "$VERSION" ]
then
    echo "Not enough arguments on the command line."
    help
    exit 2
fi


if [ -f "$bindir/email" ]
then
    echo "Removing Binary..."
    rm -f "$bindir/email"
elif [ -f "$bindir/email.exe" ]
then
    echo "Removing Binary..."
    rm -f "$bindir/email.exe"
fi

if [ -d "$sysconfdir/email" ]
then
    echo "Removing $sysconfdir/email..."
    rm -rf "$sysconfdir/email"
fi

if [ "`uname | cut -b 1-6`" = "CYGWIN" ]
then
	rm -f "$bindir/email-config"
fi

if [ -d "$docdir/email-$VERSION" ]
then
    echo "Removing $docdir/email-$VERSION..."
    rm -rf "$docdir/email-$VERSION"
fi

if [ -f "$mandir/man1/email.1" ]
then
    echo "Removing $mandir/man1/email.1..."
    rm -f "$mandir/man1/email.1"
fi

echo -n "Would you like to remove users personal ~/.email.conf files? "
read answer

if [ "$answer" = "yes" ] || [ "$answer" = "y" ]
then
    for home_dir in `cat /etc/passwd | awk -F : '{print $6}'`
    do
        if [ -f "$home_dir/.email.conf" ]
        then
            echo "Removing $home_dir/.email.conf"
            rm -f "$home_dir/.email.conf"
        fi
    done
fi

echo "Finished!"
