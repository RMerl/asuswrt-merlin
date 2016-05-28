#!/usr/bin/env python
######################################################################
##
##  Simple add/delete/change share command script for Samba
##
##  Copyright (C) Gerald Carter		       2004.
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
##
######################################################################

import sys, os
from SambaConfig import SambaConf


##                             ##
## check the command line args ##
##                             ##
delete_mode = False
if len(sys.argv) == 3:
	delete_mode = True
	print "Deleting share..."
elif len(sys.argv) == 5:
	print "Adding/Updating share..."
else:
	print "Usage: %s configfile share [path] [comments]" % sys.argv[0]
	sys.exit(1)
	
	
##                                ##
## read and parse the config file ##
##                                ##

confFile = SambaConf()

confFile.ReadConfig( sys.argv[1] )
if not confFile.valid:
	exit( 1 )
	
if delete_mode:
	if not confFile.isService( sys.argv[2] ):
		sys.stderr.write( "Asked to delete non-existent service! [%s]\n" % sys.argv[2] )
		sys.exit( 1 )
		
	confFile.DelService( sys.argv[2] )
else:
	## make the path if it doesn't exist.  Bail out if that fails
	if ( not os.path.isdir(sys.argv[3]) ):
		try:
			os.makedirs( sys.argv[3] )
			os.chmod( sys.argv[3], 0777 )
		except os.error:
			sys.exit( 1 )

	## only add a new service -- if it already exists, then 
	## just set the options
	if not confFile.isService( sys.argv[2] ):
		confFile.AddService( sys.argv[2], ['##', '## Added by modify_samba_config.py', '##']  )
	confFile.SetServiceOption( sys.argv[2], "path", sys.argv[3] )
	confFile.SetServiceOption( sys.argv[2], "comment", sys.argv[4] )
	confFile.SetServiceOption( sys.argv[2], "read only", "no" )

ret = confFile.Flush()

sys.exit( ret )

