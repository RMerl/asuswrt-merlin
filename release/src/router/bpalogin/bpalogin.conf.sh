#!/bin/sh
# 
# Configuration script for BPALogin
#
# Creates a new bpalogin.conf file based on the installed template, prompting
# for overrides.
#
# Relies heavily on the formatting of the bpalogin.conf file and is sensitive
# to whitespace!  Be careful when changing bpalogin.conf.
#
# Copyright 2003 William Rose <wdrose@sourceforge.net> and licensed under the
# GNU GPL, as per the rest of BPALogin.
###

CONFIG_FILE="${1:-/etc/bpalogin.conf}"
TMP_DIR="${TMPDIR:-/tmp}"
NEW_CONFIG="`mktemp $TMP_DIR/bpalogin.conf-XXXXXX`"

if ! [ -r "$CONFIG_FILE" ]
then
  echo "Usage: bpalogin.conf.sh config-file-name"
  exit 1
fi

eval `cat "$CONFIG_FILE" | \
(while read
  do
    case "$REPLY" in
      '# '*)
        # Comment line
      ;;
      
      '#'*)
        # Unspecified option
	REPLY="${REPLY#\#}"
        if [ -n "$REPLY" ]
	then
	  name="${REPLY%% *}"
	  value="${REPLY#* }"

	  echo "$name=\"$value\""
	  echo "${name}_disabled=\"yes\""
	  disabled="$disabled $name"
	fi
      ;;
      
      *)
	# Empty line or specified option
        if [ -n "$REPLY" ]
	then
	  name="${REPLY%% *}"
	  value="${REPLY#* }"

	  echo "$name=\"$value\""
	  echo "${name}_disabled=\"no\""
	  variables="$variables $name"
	fi
      ;;
    esac
  done
  echo "variables=\"${variables# }\""
  echo "disabled=\"${disabled# }\""
)`

##
# Prompt for new values for already configured variables.
#
if [ -n "$variables" ]
then
  cat <<EOF


BPALogin Configuration
----------------------

You will now be prompted for some basic details about your connection.

When prompted, the current information is displayed in square brackets.
Press Enter to accept the current information, or else type the new details
and press Enter.


EOF

  for var in $variables
  do
    echo -n "Enter $var [`eval 'echo $'$var`]: "
    if read && [ -n "$REPLY" ]
    then
      eval "$var=\"$REPLY\""
    fi
  done
fi

##
# Prompt for additional configuration details if disabled configuration
# options were detected.
if [ -n "$disabled" ]
then
  echo
  echo -n "Would you like to configure additional options? (y/n): "
  read
  case "$REPLY" in
    [Yy]*)
      cat <<EOF


Additional Configuration
------------------------

You will now be prompted for some additional details about your connection.

When prompted, the default information is displayed in square brackets.
Press Enter to use the default information in your configuration file, or
else type the new details.  If you do not wish to have any value recorded
in your configuration file, type # and press Enter.


EOF
      for var in $disabled
      do
	echo -n "Enter $var [`eval 'echo $'$var`]: "
	if read
	then
	  if [ -n "$REPLY" ]
	  then
	    if [ "$REPLY" = "#" ]
	    then
	      eval "${var}_disabled=\"yes\""
	    else
	      eval "${var}_disabled=\"no\""
	      eval "$var=\"$REPLY\""
	    fi
	  else
	    eval "${var}_disabled=\"no\""
	  fi
	fi
      done
      ;;
    *)
      ;;
  esac
fi


##
# Create the new bpalogin.conf file
cat "$CONFIG_FILE" | \
(while read;
  do
    case "$REPLY" in
      '# '*)
	# Comment line
	echo $REPLY >> "$NEW_CONFIG"
      ;;
      
      '#'*)
        # Unspecified option
	REPLY="${REPLY#\#}"
	if [ -n "$REPLY" ]
	then
	  name="${REPLY%% *}"

	  if eval "[ \"\$${name}_disabled\" = \"yes\" ]"
	  then
	    echo "#$name `eval 'echo $'$name`" >> "$NEW_CONFIG"
	  else
	    echo "$name `eval 'echo $'$name`" >> "$NEW_CONFIG"
	  fi
	else
	  echo $REPLY >> "$NEW_CONFIG"
	fi
      ;;
      
      *)
	if [ -n "$REPLY" ]
	then
	  name="${REPLY%% *}"

	  if eval "[ \"\$${name}_disabled\" = \"yes\" ]"
	  then
	    echo "#$name `eval 'echo $'$name`" >> "$NEW_CONFIG"
	  else
	    echo "$name `eval 'echo $'$name`" >> "$NEW_CONFIG"
	  fi
	else
	  echo $REPLY >> "$NEW_CONFIG"
	fi
      ;;
    esac
  done)

echo
echo "New configuration successfully saved in $NEW_CONFIG"
echo -n "Overwrite $CONFIG_FILE with this file? (y/n) "
read

case "$REPLY" in
  [Yy]*)
    if mv "$NEW_CONFIG" "$CONFIG_FILE"
    then
      echo "Your BPALogin configuration file has been updated."
    else
      echo "Action failed.  Please copy $NEW_CONFIG to $CONFIG_FILE manually."
    fi

    if ! chmod 600 "$CONFIG_FILE"
    then
      echo "Unable to change permissions for your configuration file."
      echo "Please check your configuration file is not readable by others."
    fi
    ;;
  *)
    echo "Your original BPALogin configuration file was not changed."
    ;;
esac

echo

