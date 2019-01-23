#! /bin/sh

# Please do not modify this script, it has been moved to chutney/tools

ECHO_N="/bin/echo -n"

# Output is prefixed with the name of the script
myname=$(basename $0)

# We need to find CHUTNEY_PATH, so that we can call the version of this script
# in chutney/tools. And we want to pass any arguments to that script as well.
# So we source this script, which processes its arguments to find CHUTNEY_PATH.

# Avoid recursively sourcing this script, and don't call the chutney version
# while recursing, either
if [ "$TEST_NETWORK_RECURSING" != true ]; then
    # Process the arguments into environmental variables with this script
    # to make sure $CHUTNEY_PATH is set
    # When we switch to using test-network.sh in chutney/tools, --dry-run
    # can be removed, because this script will find chutney, then pass all
    # arguments to chutney's test-network.sh
    echo "$myname: Parsing command-line arguments to find \$CHUTNEY_PATH"
    export TEST_NETWORK_RECURSING=true
    . "$0" --dry-run "$@"

    # Call the chutney version of this script, if it exists, and we can find it
    if [ -d "$CHUTNEY_PATH" -a -x "$CHUTNEY_PATH/tools/test-network.sh" ]; then
        unset NETWORK_DRY_RUN
        echo "$myname: Calling newer chutney script \
$CHUTNEY_PATH/tools/test-network.sh"
        "$CHUTNEY_PATH/tools/test-network.sh" "$@"
        exit $?
    else
        echo "$myname: This script has moved to chutney/tools."
        echo "$myname: Please update your chutney using 'git pull'."
        # When we switch to using test-network.sh in chutney/tools, we should
        # exit with a very loud failure here
        echo "$myname: Falling back to the old tor version of the script."
    fi
fi

until [ -z "$1" ]
do
  case "$1" in
    --chutney-path)
      export CHUTNEY_PATH="$2"
      shift
    ;;
    --tor-path)
      export TOR_DIR="$2"
      shift
    ;;
    # When we switch to using test-network.sh in chutney/tools, only the
    # --chutney-path and --tor-path arguments need to be processed by this
    # script, everything else can be handled by chutney's test-network.sh
    --flavor|--flavour|--network-flavor|--network-flavour)
      export NETWORK_FLAVOUR="$2"
      shift
    ;;
    --delay|--sleep|--bootstrap-time|--time)
      export BOOTSTRAP_TIME="$2"
      shift
    ;;
    # Environmental variables used by chutney verify performance tests
    # Send this many bytes per client connection (10 KBytes)
    --data|--data-bytes|--data-byte|--bytes|--byte)
      export CHUTNEY_DATA_BYTES="$2"
      shift
    ;;
    # Make this many connections per client (1)
    # Note: If you create 7 or more connections to a hidden service from
    # a single Tor 0.2.7 client, you'll likely get a verification failure due
    # to #15937. This is fixed in 0.2.8.
    --connections|--connection|--connection-count|--count)
      export CHUTNEY_CONNECTIONS="$2"
      shift
    ;;
    # Make each client connect to each HS (0)
    # 0 means a single client connects to each HS
    # 1 means every client connects to every HS
    --hs-multi-client|--hs-multi-clients|--hs-client|--hs-clients)
      export CHUTNEY_HS_MULTI_CLIENT="$2"
      shift
      ;;
    --coverage)
      export USE_COVERAGE_BINARY=true
      ;;
    --dry-run)
      # process arguments, but don't call any other scripts
      export NETWORK_DRY_RUN=true
      ;;
    *)
      echo "$myname: Sorry, I don't know what to do with '$1'."
      echo "$myname: Maybe chutney's test-network.sh understands '$1'."
      echo "$myname: Please update your chutney using 'git pull', and set \
\$CHUTNEY_PATH"
      # continue processing arguments during a dry run
      if [ "$NETWORK_DRY_RUN" != true ]; then
          exit 2
      fi
    ;;
  esac
  shift
done

# optional: $TOR_DIR is the tor build directory
# it's used to find the location of tor binaries
# if it's not set:
#  - set it ro $BUILDDIR, or
#  - if $PWD looks like a tor build directory, set it to $PWD, or
#  - unset $TOR_DIR, and let chutney fall back to finding tor binaries in $PATH
if [ ! -d "$TOR_DIR" ]; then
    if [ -d "$BUILDDIR/src/or" -a -d "$BUILDDIR/src/tools" ]; then
        # Choose the build directory
        # But only if it looks like one
        echo "$myname: \$TOR_DIR not set, trying \$BUILDDIR"
        export TOR_DIR="$BUILDDIR"
    elif [ -d "$PWD/src/or" -a -d "$PWD/src/tools" ]; then
        # Guess the tor directory is the current directory
        # But only if it looks like one
        echo "$myname: \$TOR_DIR not set, trying \$PWD"
        export TOR_DIR="$PWD"
    else
        echo "$myname: no \$TOR_DIR, chutney will use \$PATH for tor binaries"
        unset TOR_DIR
    fi
fi

# mandatory: $CHUTNEY_PATH is the path to the chutney launch script
# if it's not set:
#  - if $PWD looks like a chutney directory, set it to $PWD, or
#  - set it based on $TOR_DIR, expecting chutney to be next to tor, or
#  - fail and tell the user how to clone the chutney repository
if [ ! -d "$CHUTNEY_PATH" -o ! -x "$CHUTNEY_PATH/chutney" ]; then
    if [ -x "$PWD/chutney" ]; then
        echo "$myname: \$CHUTNEY_PATH not valid, trying \$PWD"
        export CHUTNEY_PATH="$PWD"
    elif [ -d "$TOR_DIR" -a -d "$TOR_DIR/../chutney" -a \
           -x "$TOR_DIR/../chutney/chutney" ]; then
        echo "$myname: \$CHUTNEY_PATH not valid, trying \$TOR_DIR/../chutney"
        export CHUTNEY_PATH="$TOR_DIR/../chutney"
    else
        # TODO: work out how to package and install chutney,
        # so users can find it in $PATH
        echo "$myname: missing 'chutney' in \$CHUTNEY_PATH ($CHUTNEY_PATH)"
        echo "$myname: Get chutney: git clone https://git.torproject.org/\
chutney.git"
        echo "$myname: Set \$CHUTNEY_PATH to a non-standard location: export \
CHUTNEY_PATH=\`pwd\`/chutney"
        unset CHUTNEY_PATH
        exit 1
    fi
fi

# When we switch to using test-network.sh in chutney/tools, this comment and
# everything below it can be removed

# For picking up the right tor binaries.
# If these varibles aren't set, chutney looks for tor binaries in $PATH
if [ -d "$TOR_DIR" ]; then
    tor_name=tor
    tor_gencert_name=tor-gencert
    if [ "$USE_COVERAGE_BINARY" = true ]; then
        tor_name=tor-cov
    fi
    export CHUTNEY_TOR="${TOR_DIR}/src/or/${tor_name}"
    export CHUTNEY_TOR_GENCERT="${TOR_DIR}/src/tools/${tor_gencert_name}"
fi

# Set the variables for the chutney network flavour
export NETWORK_FLAVOUR=${NETWORK_FLAVOUR:-"bridges+hs"}
export CHUTNEY_NETWORK=networks/$NETWORK_FLAVOUR

# And finish up if we're doing a dry run
if [ "$NETWORK_DRY_RUN" = true ]; then
    # we can't exit here, it breaks argument processing
    return
fi

cd "$CHUTNEY_PATH"
./tools/bootstrap-network.sh $NETWORK_FLAVOUR || exit 2

# Sleep some, waiting for the network to bootstrap.
# TODO: Add chutney command 'bootstrap-status' and use that instead.
BOOTSTRAP_TIME=${BOOTSTRAP_TIME:-35}
$ECHO_N "$myname: sleeping for $BOOTSTRAP_TIME seconds"
n=$BOOTSTRAP_TIME; while [ $n -gt 0 ]; do
    sleep 1; n=$(expr $n - 1); $ECHO_N .
done; echo ""
./chutney verify $CHUTNEY_NETWORK
VERIFY_EXIT_STATUS=$?
# work around a bug/feature in make -j2 (or more)
# where make hangs if any child processes are still alive
./chutney stop $CHUTNEY_NETWORK
exit $VERIFY_EXIT_STATUS
