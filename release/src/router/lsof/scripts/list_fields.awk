# $Id: list_fields.awk,v 1.3 97/09/23 09:32:38 abe Exp $
#
# list_fields.awk -- sample awk script to list lsof full field output
#		     (i.e., -F output without -0)
#
# NB: this is not particularly elegant awk; several sections were
#     replicated, perhaps unnecessarily, to produce a sample quickly
#     and simply.
#
#
# Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
# 47907.  All rights reserved.
#
# Written by Victor A. Abell
#
# This software is not subject to any license of the American Telephone
# and Telegraph Company or the Regents of the University of California.
#
# Permission is granted to anyone to use this software for any purpose on
# any computer system, and to alter it and redistribute it freely, subject
# to the following restrictions:
#
# 1. Neither the authors nor Purdue University are responsible for any
#    consequences of the use of this software.
#
# 2. The origin of this software must not be misrepresented, either by
#    explicit claim or by omission.  Credit to the authors and Purdue
#    University must appear in documentation and sources.
#
# 3. Altered versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
#
# 4. This notice may not be removed or altered.

# Clear file and process status.

BEGIN {
  fhdr = fdst = pidst = 0;
  access = dev = devch = fd = inode = lock = name = offset = "";
  proto = size = state = stream = type = "";
  cmd = login = pgrp = pid = ppid = uid = "";
}

# Start a new process.

/^p/ {
  val = substr($0, 2);
  if (pidst) {

  # Print a previously accumulated process set.

    printf "COMMAND       PID    PGRP    PPID  USER\n";
    printf "%-9.9s  %6d  %6d  %6d", cmd, pid, pgrp, ppid;
    if (login != "") { printf "  %s\n", login }
    else { printf "  %s\n", uid }
    pidst = 0;
    cmd = login = pgrp = pid = uid = "";
  }
  if (fdst) {

  # Print a previously accumulated file set.

    if (fhdr == 0) {
      printf "      FD   TYPE      DEVICE   SIZE/OFF      INODE  NAME\n";
    }
    printf "    %4.4s%1.1s%1.1s %4.4s", fd, access, lock, type;
    t = dev; if (devch != "") { t = devch }
    printf("  %10.10s", t);
    t = size; if (offset != "") { t = offset }
    printf " %10.10s", t;
    t = inode; if (proto != "") { t = proto }
    printf " %10.10s", t;
    t = stream; if (name != "") {t = name }
    printf "  %s", t;
    if (state != "") { printf " %s)\n", state } else { printf "\n" }
    access = dev = devch = fd = inode = lock = name = offset = "";
    proto = size = state = stream = type = "";
    fdst = fhdr = 0
  }

# Record a new process.

  pidst = 1;
  pid = val;
}

/^g|^c|^u|^L|^R/ {

# Save process set information.

  id = substr($0, 1, 1);
  val = substr($0, 2);
  if (id == "g") { pgrp = val; next }		# PGRP
  if (id == "c") { cmd = val; next }		# command
  if (id == "u") { uid = val; next }		# UID
  if (id == "L") { login = val; next }		# login name
  if (id == "R") { ppid = val; next }		# PPID
}

/^f|^a|^l|^t|^d|^D|^s|^o|^i|^P|^S|^T|^n/ {

# Save file set information.

  id = substr($0, 1, 1);
  val = substr($0, 2);
  if (id == "f") {
    if (pidst) {

    # Print a previously accumulated process set.

      printf "COMMAND       PID    PGRP    PPID  USER\n";
      printf "%-9.9s  %6d  %6d  %6d", cmd, pid, pgrp, ppid;
      if (login != "") { printf "  %s\n", login }
      else { printf "  %s\n", uid }
      pidst = 0;
      cmd = login = pgrp = pid = uid = "";
    }
    if (fdst) {

      # Print a previously accumulated file set.

	if (fhdr == 0) {
	  printf "      FD   TYPE      DEVICE   SIZE/OFF      INODE  NAME\n";
	}
	fhdr = 1;
	printf "    %4.4s%1.1s%1.1s %4.4s", fd, access, lock, type;
	t = dev; if (devch != "") { t = devch }
	printf("  %10.10s", t);
	t = size; if (offset != "") { t = offset }
	printf " %10.10s", t;
	t = inode; if (proto != "") { t = proto }
	printf " %10.10s", t;
	t = stream; if (name != "") {t = name }
	printf "  %s", t;
	if (state != "") { printf " %s)\n", state } else { printf "\n" }
	access = dev = devch = fd = inode = lock = name = offset = "";
	proto = size = state = stream = type = "";
    }

  # Start an new file set.

    fd = val;
    fdst = 1;
    next;
  }

# Save file set information.

  if (id == "a") { access = val; next }		# access
  if (id == "l") { lock = val; next }		# lock
  if (id == "t") { type = val; next }		# type
  if (id == "d") { devch = val; next }		# device characters
  if (id == "D") { dev = val; next }		# device major/minor numbers
  if (id == "s") { size = val; next }		# size
  if (id == "o") { offset = val; next }		# offset
  if (id == "i") { inode = val; next }		# inode number
  if (id == "P") { proto = val; next }		# protocol
  if (id == "S") { stream = val; next }		# stream name
  if (id == "T") {				# TCP/TPI state
    if (state == "") {
      state = sprintf("(%s", val);
    } else {
      state = sprintf("%s %s", state, val);
    }
    next
  }
  if (id == "n") { name = val; next }		# name, comment, etc.
}

END {
  if (pidst) {

  # Print last process set.

    printf "COMMAND       PID    PGRP    PPID  USER\n";
    printf "%-9.9s  %6d  %6d  %6d", cmd, pid, pgrp, ppid;
    if (login != "") { printf "  %s\n", login }
    else { printf "  %s\n", uid }
  }
  if (fdst) {

  # Print last file set.

    if (fhdr == 0) {
      printf "      FD   TYPE      DEVICE   SIZE/OFF      INODE  NAME\n";
    }
    printf "    %4.4s%1.1s%1.1s %4.4s", fd, access, lock, type;
    t = dev; if (devch != "") { t = devch }
    printf("  %10.10s", t);
    t = size; if (offset != "") { t = offset }
    printf " %10.10s", t;
    t = inode; if (proto != "") { t = proto }
    printf " %10.10s", t;
    t = stream; if (name != "") {t = name }
    printf "  %s", t;
    if (state != "") { printf " %s)\n", state; } else { printf "\n"; }
  }
}
