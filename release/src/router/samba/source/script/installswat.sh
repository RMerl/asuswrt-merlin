#!/bin/sh
#fist version March 1998, Andrew Tridgell

SWATDIR=$1
SRCDIR=$2/
BOOKDIR=$3

echo Installing SWAT in $SWATDIR
echo Installing the Samba Web Administration Tool

for d in $SWATDIR $SWATDIR/help $SWATDIR/images $SWATDIR/include; do
    if [ ! -d $d ]; then
        mkdir $d
        if [ ! -d $d ]; then
            echo Failed to make directory $d, does $USER have privileges?
            exit 1
        fi
    fi
done

# Install images

for f in $SRCDIR../swat/images/*.gif; do
      FNAME=$SWATDIR/images/`basename $f`
      echo $FNAME
      cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
      chmod 0644 $FNAME
done

# Install html help

for f in $SRCDIR../swat/help/*.html; do
      FNAME=$SWATDIR/help/`basename $f`
      echo $FNAME
      if [ "x$BOOKDIR" = "x" ]; then
        cat $f | sed 's/@BOOKDIR@.*$//' > $f.tmp
      else
        cat $f | sed 's/@BOOKDIR@//' > $f.tmp
      fi
      f=$f.tmp
      cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
      rm -f $f
      chmod 0644 $FNAME
done

# Install html documentation

for f in $SRCDIR../docs/htmldocs/*.html; do
      FNAME=$SWATDIR/help/`basename $f`
      echo $FNAME
      cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
      chmod 0644 $FNAME
done

# Install "server-side" includes

for f in $SRCDIR../swat/include/*.html; do
      FNAME=$SWATDIR/include/`basename $f`
      echo $FNAME
      cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
      chmod 0644 $FNAME
done

# Install Using Samba book

if [ "x$BOOKDIR" != "x" ]; then

    # Create directories

    for d in $BOOKDIR $BOOKDIR/figs $BOOKDIR/gifs; do
        if [ ! -d $d ]; then
            mkdir $d
            if [ ! -d $d ]; then
                echo Failed to make directory $d, does $USER have privileges?
                exit 1
            fi
        fi
    done

    # HTML files

    for f in $SRCDIR../docs/htmldocs/using_samba/*.html; do
        FNAME=$BOOKDIR/`basename $f`
        echo $FNAME
        cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
        chmod 0644 $FNAME
    done

    # Figures

    for f in $SRCDIR../docs/htmldocs/using_samba/figs/*.gif; do
        FNAME=$BOOKDIR/figs/`basename $f`
        echo $FNAME
        cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
        chmod 0644 $FNAME
    done

    # Gifs

    for f in $SRCDIR../docs/htmldocs/using_samba/gifs/*.gif; do
        FNAME=$BOOKDIR/gifs/`basename $f`
        echo $FNAME
        cp $f $FNAME || echo Cannot install $FNAME. Does $USER have privileges?
        chmod 0644 $FNAME
    done

fi

cat << EOF
======================================================================
The SWAT files have been installed. Remember to read the swat/README
for information on enabling and using SWAT
======================================================================
EOF

exit 0

