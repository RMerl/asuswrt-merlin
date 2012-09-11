#!/bin/bash

## ABSOLUTE path to the spawn-fcgi binary
SPAWNFCGI="/home/weigon/projects/spawn-fcgi/src/spawn-fcgi"

## ABSOLUTE path to the PHP binary
FCGIPROGRAM="/usr/local/bin/php"

## TCP port to which to bind on localhost
FCGIPORT="1026"

## number of PHP children to spawn
PHP_FCGI_CHILDREN=10

## maximum number of requests a single PHP process can serve before it is restarted
PHP_FCGI_MAX_REQUESTS=1000

## IP addresses from which PHP should access server connections
FCGI_WEB_SERVER_ADDRS="127.0.0.1,192.168.2.10"

# allowed environment variables, separated by spaces
ALLOWED_ENV="ORACLE_HOME PATH USER"

## if this script is run as root, switch to the following user
USERID=wwwrun
GROUPID=wwwrun


################## no config below this line

if test x$PHP_FCGI_CHILDREN = x; then
  PHP_FCGI_CHILDREN=5
fi

export PHP_FCGI_MAX_REQUESTS
export FCGI_WEB_SERVER_ADDRS

ALLOWED_ENV="$ALLOWED_ENV PHP_FCGI_MAX_REQUESTS FCGI_WEB_SERVER_ADDRS"

if test x$UID = x0; then
  EX="$SPAWNFCGI -p $FCGIPORT -f $FCGIPROGRAM -u $USERID -g $GROUPID -C $PHP_FCGI_CHILDREN"
else
  EX="$SPAWNFCGI -p $FCGIPORT -f $FCGIPROGRAM -C $PHP_FCGI_CHILDREN"
fi

# copy the allowed environment variables
E=

for i in $ALLOWED_ENV; do
  E="$E $i=${!i}"
done

# clean the environment and set up a new one
env - $E $EX
