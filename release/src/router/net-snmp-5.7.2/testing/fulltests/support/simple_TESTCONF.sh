#! /bin/sh -f

#
# Variables:  (* = exported)
#  *SNMP_TMPDIR:  	  place to put files used in testing.
#   SNMP_TESTDIR: 	  where the test scripts are kept.
#  *SNMP_PERSISTENT_FILE: where to store the agent's persistent information
#                         (XXX: this should be specific to just the agent)

# MinGW/MSYS only: translate an MSYS path back into a DOS path such that snmpd
# and the Net-SNMP applications can understand it. One of the features of MSYS
# is that if a POSIX-style path is passed as a command-line argument to an
# executable that MSYS translates that path to a DOS-style path before
# starting the executable. This is a key feature of MSYS that makes it
# possible to run shell scripts unmodified and at the same time to use
# executables that accept DOS-style paths. There is no support however for
# automatical translation of environment variables that contain paths. Hence
# this function that translates paths explicitly.
translate_path() {
  if [ "$OSTYPE" = msys ]; then
    local t=`set \
             | sed -n -e "s/^$1='\(.*\)'$/${SNMP_ENV_SEPARATOR}\1/p" \
                      -e "s/^$1=\(.*\)$/${SNMP_ENV_SEPARATOR}\1/p" \
             | sed -e "s|${SNMP_ENV_SEPARATOR}/c/|${SNMP_ENV_SEPARATOR}c:/|g" \
                   -e "s|${SNMP_ENV_SEPARATOR}/tmp/|${SNMP_ENV_SEPARATOR}c:/windows/temp/|g" \
             | sed -e "s/^${SNMP_ENV_SEPARATOR}//" \
            `
    eval "$1='$t'"
  fi
}

#
# Only allow ourselves to be eval'ed once
#
if [ "x$TESTCONF_SH_EVALED" != "xyes" ]; then
    TESTCONF_SH_EVALED=yes

#
# set cpu and memory limits to prevent major damage
#
# defaults: 1h CPU, 500MB VMEM
#
[ "x$SNMP_LIMIT_VMEM" = "x" ] && SNMP_LIMIT_VMEM=512000
[ "x$SNMP_LIMIT_CPU" = "x" ] && SNMP_LIMIT_CPU=3600
# ulimit will fail if existing limit is lower -- ignore because it's ok
ulimit -S -t $SNMP_LIMIT_CPU 2>/dev/null
# not all sh-alikes support "ulimit -v" -- play safe
[ "x$BASH_VERSION" != "x" ] && ulimit -S -v $SNMP_LIMIT_VMEM 2>/dev/null

#
# Set up an NL suppressing echo command
#
case "`echo 'x\c'`" in
  'x\c')
    ECHO() { echo -n $*; }
    ;;
  x)
    ECHO() { echo $*\\c; }
    ;;
  *)
    echo "I don't understand your echo command ..."
    exit 1
    ;;
esac
#
# how verbose should we be (0 or 1)
#
if [ "x$SNMP_VERBOSE" = "x" ]; then
    SNMP_VERBOSE=0
    export SNMP_VERBOSE
fi

SNMP_ENV_SEPARATOR="`${builddir}/net-snmp-config --env-separator`"

if [ "x$MIBDIRS" = "x" ]; then
  if [ "x$SNMP_PREFER_NEAR_MIBS" = "x1" ]; then
    MIBDIRS=${srcdir}/mibs
    export MIBDIRS
  fi
fi

# Set up the path to the programs we want to use.
if [ "x$SNMP_PATH" = "x" ]; then
    PATH=${builddir}/agent:${builddir}/apps:$PATH
    export PATH
    SNMP_PATH=yes
    export SNMP_PATH
fi

# make sure that we can fulfill all library dependencies
_ld_lib_path="${SNMP_UPDIR}/snmplib/.libs:${SNMP_UPDIR}/agent/.libs:${SNMP_UPDIR}/agent/helpers/.libs"
case `uname` in
  CYGWIN*)
    PATH="${_ld_lib_path}:$PATH"
    export PATH
    ;;
  Darwin*)
    if [ "x$DYLD_LIBRARY_PATH" != "x" ]; then
      DYLD_LIBRARY_PATH="${_ld_lib_path}:${DYLD_LIBRARY_PATH}"
    else
      DYLD_LIBRARY_PATH="${_ld_lib_path}"
    fi
    export DYLD_LIBRARY_PATH
    ;;
  HP-UX*)
    if [ "x$SHLIB_PATH" != "x" ]; then
      SHLIB_PATH="${_ld_lib_path}:${SHLIB_PATH}"
    else
      SHLIB_PATH="${_ld_lib_path}"
    fi
    export SHLIB_PATH
    ;;
  *)
    if [ "x$LD_LIBRARY_PATH" != "x" ]; then
      LD_LIBRARY_PATH="${_ld_lib_path}:${LD_LIBRARY_PATH}"
    else
      LD_LIBRARY_PATH="${_ld_lib_path}"
    fi
    export LD_LIBRARY_PATH
    ;;
esac

# Set up temporary directory
if [ "x$SNMP_HEADERONLY" != "xyes" ]; then
  if [ "x$SNMP_TMPDIR" = "x" ]; then
    if [ "x$testnum" = "x" ] ; then
        testnum=0
    fi
    SNMP_TMPDIR="/tmp/snmp-test-$testnum-$$"
    translate_path SNMP_TMPDIR
    export SNMP_TMPDIR
    if [ -d $SNMP_TMPDIR ]; then
	echo "$0: ERROR: $SNMP_TMPDIR already existed."
	exit 1;
    fi
  fi
  if [ ! -d $SNMP_TMPDIR ]; then
    mkdir -p $SNMP_TMPDIR
    chmod 0700 $SNMP_TMPDIR
  fi
  if [ "x$SNMP_TMP_PERSISTENTDIR" = "x" ]; then
    SNMP_TMP_PERSISTENTDIR=$SNMP_TMPDIR/persist
    export SNMP_TMP_PERSISTENTDIR
  fi
  if [ ! -d $SNMP_TMP_PERSISTENTDIR ]; then
    mkdir $SNMP_TMP_PERSISTENTDIR
    chmod 0700 $SNMP_TMP_PERSISTENTDIR
  fi
fi

if [ "x$SNMP_SAVE_TMPDIR" = "x" ]; then
    SNMP_SAVE_TMPDIR="no"
    export SNMP_SAVE_TMPDIR
fi

SNMP_IGNORE_WINDOWS_REGISTRY="true"
export SNMP_IGNORE_WINDOWS_REGISTRY
SNMP_PERLPROG="`${builddir}/net-snmp-config --perlprog`"
SNMP_TESTDIR="$SNMP_BASEDIR/tests"
SNMP_CONFIG_FILE="$SNMP_TMPDIR/snmpd.conf"
SNMPTRAPD_CONFIG_FILE="$SNMP_TMPDIR/snmptrapd.conf"
SNMPAPP_CONFIG_FILE="$SNMP_TMPDIR/snmp.conf"
AGENTX_CONFIG_FILE="$SNMP_TMPDIR/agentx.conf"
SNMP_SNMPTRAPD_LOG_FILE="$SNMP_TMPDIR/snmptrapd.log"
SNMP_SNMPTRAPD_PID_FILE="$SNMP_TMPDIR/snmptrapd.pid"
SNMP_SNMPD_PID_FILE="$SNMP_TMPDIR/snmpd.pid"
SNMP_SNMPD_LOG_FILE="$SNMP_TMPDIR/snmpd.log"
SNMP_AGENTX_PID_FILE="$SNMP_TMPDIR/agentx.pid"
SNMP_AGENTX_LOG_FILE="$SNMP_TMPDIR/agentx.log"
SNMPCONFPATH="${SNMP_TMPDIR}${SNMP_ENV_SEPARATOR}${SNMP_TMP_PERSISTENTDIR}"
translate_path SNMPCONFPATH
export SNMPCONFPATH
SNMP_PERSISTENT_DIR=$SNMP_TMP_PERSISTENTDIR
translate_path SNMP_PERSISTENT_DIR
export SNMP_PERSISTENT_DIR
#SNMP_PERSISTENT_FILE="$SNMP_TMP_PERSISTENTDIR/persistent-store.conf"
#export SNMP_PERSISTENT_FILE

## Setup default flags and ports iff not done
if [ "x$SNMP_FLAGS" = "x" ]; then
    SNMP_FLAGS="-d"
fi
if test -x /bin/netstat ; then
    NETSTAT=/bin/netstat
elif test -x /usr/bin/netstat ; then
    NETSTAT=/usr/bin/netstat
elif test -x /usr/sbin/netstat ; then
    # e.g. Tru64 Unix
    NETSTAT=/usr/sbin/netstat
elif test -x /usr/etc/netstat ; then
    # e.g. IRIX
    NETSTAT=/usr/etc/netstat
elif test -x /cygdrive/c/windows/system32/netstat ; then
    # Cygwin
    NETSTAT=/cygdrive/c/windows/system32/netstat
elif test -x /c/Windows/System32/netstat ; then
    # MinGW + MSYS
    NETSTAT=/c/Windows/System32/netstat
else
    NETSTAT=""
fi

if [ "x$OSTYPE" = "xmsys" ]; then
    # Obtain the MSYS installation path from the !C: environment variable,
    # remove surrounding single quotes and convert backslashes into forward
    # slashes.
    MSYS_PATH="$(set \
                 | sed -n 's|^\!C:='"'"'\(.*\)'"'"'$|\1|p' | sed 's|\\|/|g')"
    MSYS_SH="$MSYS_PATH/sh.exe"
fi

PROBE_FOR_PORT() {
    BASE_PORT=$1
    MAX_RETRIES=10
    if test -x "$NETSTAT" ; then
        if test -z "$RANDOM"; then
            RANDOM=2
        fi
        while :
        do
            BASE_PORT=`expr $BASE_PORT + \( $RANDOM % 100 \)`
            IN_USE=`$NETSTAT -a -n 2>/dev/null | grep "[\.:]$BASE_PORT "`
            if [ $? -ne 0 ]; then
                echo "$BASE_PORT"
                break
            fi
            MAX_RETRIES=`expr $MAX_RETRIES - 1`
            if [ $MAX_RETRIES -eq 0 ]; then
                echo "ERROR: Could not find available port." >&2
                echo "NOPORT"
                exit 255
            fi
        done
    fi
}

if [ "x$SNMP_SNMPD_PORT" = "x" ]; then
    SNMP_SNMPD_PORT=`PROBE_FOR_PORT 8765`
fi
if [ "x$SNMP_SNMPTRAPD_PORT" = "x" ]; then
    SNMP_SNMPTRAPD_PORT=`PROBE_FOR_PORT 5678`
fi
if [ "x$SNMP_AGENTX_PORT" = "x" ]; then
    SNMP_AGENTX_PORT=`PROBE_FOR_PORT 7676`
fi
if [ "x$SNMP_TRANSPORT_SPEC" = "x" ];then
	SNMP_TRANSPORT_SPEC="udp"
fi
if [ "x$SNMP_TEST_DEST" = "x" -a $SNMP_TRANSPORT_SPEC != "unix" ];then
	SNMP_TEST_DEST="127.0.0.1:"
fi
export SNMP_FLAGS SNMP_SNMPD_PORT SNMP_SNMPTRAPD_PORT

# Make sure the agent doesn't parse any config file but what we give it.  
# this is mainly to protect against a broken agent that doesn't
# properly handle combinations of -c and -C.  (since I've broke it before).
#SNMPCONFPATH="$SNMP_TMPDIR/does-not-exist"
#export SNMPCONFPATH

fi # Only allow ourselves to be eval'ed once
