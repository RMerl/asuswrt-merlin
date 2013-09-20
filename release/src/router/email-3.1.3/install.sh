#!/bin/sh
# This is the install script... Nice and short.

help ()
{
    cat <<EOF
This script is witten and maintained by Dean Jones.
Please send bugs to <dean@cleancode.org>
Options are described below.

    --prefix dir            Specifies the main prefix to all directories
    --bindir dir            Specifies the bin directory. Overrides --prefix
    --mandir dir            Specifies the man directory. Overrides --prefix
    --sysconfdir dir        Specifies the sys config directory. Overrides --prefix
    --binext ext            Specifies the extension to binaries (ex: .exe)
    --version version       Specifies the version of the program to install

EOF
    exit
}

# Get command line arguments
while [ "$1" ]
do
    case "$1" in
        "-p" | "--prefix" | "-prefix")
            if [ -z "$2" ]
            then
                echo "error: option '$1' requires an argument"
                exit 2
            fi

            shift
            prefix="$1"
            ;;

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
            sysconf="$1"
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

        "--binext" | "-binext" )
            shift
            binext="$1"
            ;;

        "--version" | "-version" )
            if [ "$2" ]
            then
                shift
                VERSION="$1"
            else
                VERSION=""
            fi
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

if [ -z "$VERSION" ]
then
    echo "error: You MUST specify the --version flag"
    exit 2
fi

if [ -z "$prefix" ]
then
    if [ -z "$bindir" ] || [ -z "$mandir" ] || [ -z "$sysconf" ]
    then
        echo "If --bindir --mandir and --sysconfdir are not specified"
        echo "then you MUST specify --prefix!"
        echo
        help
        exit 2
    fi
fi

mkdir -p $bindir $mandir $sysconf $docdir

mkdir -p $bindir $mandir $mandir/man1 $sysconf $docdir

echo "Installing email v$VERSION..."
if [ -z "$bindir" ]
then
    bindir="$prefix/bin"
fi
if [ -z "$mandir" ]
then
    mandir="$prefix/man"
fi
if [ -z "$sysconf" ]
then
    sysconf="$prefix/etc"
fi
if [ -z "$docdir" ]
then
    docdir="$prefix/doc"
fi

echo "Binary directory: $bindir"
echo "Man directory: $mandir"
echo "System configuration file directory: $sysconf"

echo "Copying src/email$binext to $bindir..."
if [ -x "src/email$binext" ]
then
    cp -f "src/email$binext" "$bindir"
    if [ $? -ne 0 ]; then
        echo "FAILED TO COPY EXECUTABLE"
        exit
    fi
    chmod 755 "$bindir/email$binext"
fi

if [ ! -d "$sysconf/email" ]
then
    echo "Creating Email Directory... "
    mkdir -p "$sysconf/email" 
    if [ $? -ne 0 ]
    then
        echo "FAILED TO CREATE '$sysconf/email'"
        exit 2
    else
        echo "Created '$sysconf/email'... "
    fi
fi

echo "Copying Files to '$sysconf/email' directory... "
if [ ! -f "$sysconf/email/email.sig" ]; then
    cp -f email.sig "$sysconf/email"
    if [ $? -ne 0 ]
    then
        echo "FAILED TO COPY email.sig to $sysconf/email"
        exit 2
    fi
fi
if [ ! -f "$sysconf/email/email.address.template" ]; then
    cp -f email.address.template "$sysconf/email"
    if [ $? -ne 0 ]
    then
        echo "FAILED TO COPY email.address.template to $sysconf/email"
        exit 2
    fi
fi
if [ ! -f "$sysconf/email/email.conf" ]; then
    if [ -f email.conf ]
    then
        cp -f email.conf "$sysconf/email"
        if [ $? -ne 0 ]
        then
            echo "FAILED TO COPY email.conf to $sysconf/email"
            exit 2
        fi
		if [ "`uname | cut -b 1-6`" = "CYGWIN" ]
		then
			if [ -f "generate_config" ]
			then
				chmod +x "generate_config"
				cp generate_config "$bindir/email-config"
				chmod 755 "$bindir/email-config"
				"$bindir/email-config" "$sysconf/email/email.conf"
			fi
		fi
    fi
fi
if [ ! -f "$sysconf/email/mime.types" ]; then
    cp -f mime.types "$sysconf/email"
    if [ $? -ne 0 ]
    then
        echo "FAILED TO COPY mime.types to $sysconf/email"
        exit 2
    fi
fi
cp -f email.help "$sysconf/email"
if [ $? -ne 0 ]
then
    echo "FAILED TO COPY email.help to $sysconf/email"
    exit 2
fi

echo "Copying man pages to $mandir/man1..."
cp -f email.1 "$mandir/man1"
if [ "$?" -ne 0 ]
then
    echo "FAILED TO COPY email.1 to $mandir/man1"
    exit 2
fi
    
echo "Copying email help documentation to $docdir/email-$VERSION... "
if [ ! -d "$docdir/email-$VERSION" ]; then
    mkdir -p "$docdir/email-$VERSION"
    if [ "$?" -ne 0 ]
    then
        echo "FAILED TO MKDIR $docdir/email-$VERSION"
        exit 2
    fi
fi

cp -f email.1 README ChangeLog THANKS COPYING AUTHORS "$docdir/email-$VERSION"
if [ "$?" -ne 0 ]
then
    echo "FAILED TO COPY documentation to $docdir/email-$VERSION"
    exit 2
fi

echo " Success!"
cat << EOF

#######################################################
# Done installing E-Mail client.                      #
# Please read README for information on setup and use.#
#                                                     #
# If you have any questions or concerns...            #
# Please e-mail: software@cleancode.org               #
#######################################################
EOF


