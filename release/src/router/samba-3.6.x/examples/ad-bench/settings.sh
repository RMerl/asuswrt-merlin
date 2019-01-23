#!/bin/bash
# AD-Bench settings
#
# Copyright (C) 2009  Kai Blin  <kai@samba.org>
#
# This file is part of AD-Bench, an Active Directory benchmark tool
#
# AD-Bench is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AD-Bench is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with AD-Bench.  If not, see <http://www.gnu.org/licenses/>.


DATE=date
BC=bc
SED=sed
DATE_FORMATSTR="+%s.%N"

KINIT=kinit
# MIT krb < 1.6
KINIT_PARAM_OLD="--password-file=STDIN"
# MIT krb >= 1.6
KINIT_PARAM_NEW=""

KDESTROY=kdestroy
SEQ=seq

NEW_KRB5CCNAME=/tmp/ad_test_ccname

NET="${HOME}/samba/bin/net"
CONFIG_FILE=`dirname $0`/smb.conf

RUNS=`dirname $0`/runs.txt
