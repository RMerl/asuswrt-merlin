#
# cfg_site_sample.py - Sample site configuration
#
# Copyright (C) 2008-2009 Teluu Inc. (http://www.teluu.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
import builder

# Your site name
SITE_NAME="Newham3"

# The URL where tests will be submitted to
URL = "http://192.168.0.2/dash/submit.php?project=PJSIP"

# Test group
GROUP = "Experimental"

# PJSIP base directory
BASE_DIR = "/root/project/pjproject"

# List of additional ccdash options
#OPTIONS = ["-o", "out.xml", "-y"]
OPTIONS = []

# What's the content of config_site.h
CONFIG_SITE = ""

# What's the content of user.mak
USER_MAK = ""

# List of regular expression of test patterns to be excluded
EXCLUDE = []

# List of regular expression of test patterns to be included (even
# if they match EXCLUDE patterns)
NOT_EXCLUDE = []
#"configure", "update", "build.*make", "build", "run.py mod_run.*100_simple"]
