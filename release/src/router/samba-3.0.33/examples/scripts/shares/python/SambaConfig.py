import sys, string, SambaParm
from smbparm import parm_table

######################################################################
##
##  smb.conf parser class
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


#####################################################################
## multi line Samba comment
class SambaComment:

	def __init__( self, comment ):
		self.comment = comment
		
	def Dump( self, stream, whitespace=None ):
		if not self.comment:
			return
		for line in self.comment:
			if whitespace:
				stream.write( whitespace )
			stream.write( line )
			stream.write( "\n" )
		
	
#####################################################################
## string smb.conf parms
class SambaParameter :

	## indexs into the parm table tuples
	DisplayName  = 0
	ObjectType   = 1
	DefaultValue = 2
	Scope        = 3

	## Stores a key into the parm_table and creates an
	## SambaParmXXX object to store the value
	def __init__( self, name, value, comment=None ):
		self.key = string.upper(string.strip(name))
		self.comment = None
		assert parm_table.has_key( self.key ), "Bad parameter name! [%s]" % name
		self.parm = parm_table[self.key][self.ObjectType]( value )
		if comment :
			self.comment = SambaComment( comment )
			
		#if not self.parm.valid:
		#	self.parm.SetValue( parm_table[self.key][self.DefaultValue] )

	## simple test for global or service parameter scope
	def isGlobalParm( self ) :
		return parm_table[self.key][Scope]

	## dump <the parameter to stdout
	def Dump( self, stream ):
		if self.comment:
			self.comment.Dump( stream, "\t" )
		stream.write( "\t%s = %s\n" % ( parm_table[self.key][self.DisplayName], self.parm.StringValue() ))


#####################################################################
## Class for parsing and modifying Smb.conf 
class SambaConf:
		
	def __init__( self ):
		self.services = {}
		self.valid = True
		self.services["GLOBAL"] = {}
		self.services_order = []
		
	
	## always return a non-empty line of input or None
	## if we hit EOF
	def ReadLine( self, stream ):
		result = None
		input_str = None
		
		while True:
			input_str = stream.readline()
		
			## Are we done with the file ?
			
			if len(input_str) == 0:
				return result
			
			## we need one line of valid input at least
			## continue around the loop again if the result
			## string is empty
			
			input_str = string.strip( input_str )
			if len(input_str) == 0:
				if not result:
					continue
				else:
					return result
			
			## we have > 1` character so setup the result
			if not result: 
				result = ""
			
			## Check for comments -- terminated by \n -- no continuation
			
			if input_str[0] == '#' or input_str[0] == ';' :
				result = input_str
				break
			
			## check for line continuation			
			
			if input_str[-1] == "\\" :
				result += input_str[0:-1]
				contine

			## otherwise we have a complete line
			result += input_str
			break

		return result
		
	## convert the parameter name to a form suitable as a dictionary key	
	def NormalizeParamName( self, param ):
		return string.upper( string.join(string.split(param), "") )
		
	## Open the file and parse it into a services dictionary
	## if possible
	def ReadConfig( self, filename ):
		self.filename = filename

		try:
			fconfig = open( filename, "r" )
		except IOError:
			self.valid = False
			return

		section_name = None
		
		## the most recent seen comment is stored as an array
		current_comment = []
		
		while True:
		
			str = self.ReadLine( fconfig )
			if not str:
				break

			## Check for comments
			if str[0] == '#' or str[0] == ';' :
				current_comment.append( str )
				continue

			## look for a next section name
			if str[0]=='[' and str[-1]==']' :
				section_name = str[1:-1]
				self.AddService( section_name, current_comment )
				current_comment = []
				continue

			str_list = string.split( str, "=" )

			if len(str_list) != 2 :
				continue

			if not section_name :
				print "parameter given without section name!"
				break

			param = self.NormalizeParamName( str_list[0] )
			value = string.strip(str_list[1])

			self.SetServiceOption( section_name, param, value, current_comment )
			self.dirty = False
			
			## reset the comment strinf if we have one
			current_comment = []
			
		fconfig.close()

	## Add a parameter to the global section
	def SetGlobalOption( self, param, value, comment=None ) :
		self.SetServiceOption( "GLOBAL", param, value, comment )

	## Add a parameter to a specific service
	def SetServiceOption( self, servicename, param, value, comment=None ) :
		service = string.upper(servicename)
		parm = self.NormalizeParamName(param)
		self.services[service]['_order_'].append( parm )
		self.services[service][parm] = SambaParameter( parm, value, comment )
		self.dirty = True

	## remove a service from the config file
	def DelService( self, servicename ) :
		service = string.upper(servicename)
		self.services[service] = None
		self.dirty = True
		
	## remove a service from the config file
	def AddService( self, servicename, comment=None ) :
		service = string.upper(servicename)

		self.services[service] = {}
		self.services[service]['_order_'] = []

		if ( comment ):
			self.services[service]['_comment_'] = SambaComment( comment )

		self.services_order.append( service )

		self.dirty = True
		
	def isService( self, servicename ):
		service = string.upper(servicename)
		return self.services.has_key( service )
		
	## dump a single service to stream
	def DumpService( self, stream, servicename ):
	
		## comments first 
		if self.services[servicename].has_key( '_comment_' ):
			self.services[servicename]['_comment_'].Dump( stream )
			
		## section header
		stream.write( "[%s]\n" % (servicename) )
		
		## parameter = value
		for parm in self.services[servicename]['_order_']:
			self.services[servicename][parm].Dump(stream)
	
	## dump the config to stream
	def Dump( self, stream ):
		self.DumpService( stream, "GLOBAL" )
		stream.write("\n")
		
		for section in self.services_order:
			## already handled the global section
			if section == "GLOBAL": 
				continue
				
			## check for deleted sections ##
			if not self.services[section]:
				continue
				
			self.DumpService( stream, section )
			stream.write( "\n" )
			
	## write out any changes to disk
	def Flush( self ):
		if not self.dirty: 
			return
			
		try:
			fconfig = open( self.filename, "w" )
		except IOError:
			sys.stderr.write( "ERROR!\n" ) 
			return 1
			
		self.Dump( fconfig )
		fconfig.close()
		return 0
		
	def Services( self ):
		service_list = []
		for section in self.services.keys():
			service_list.append( section )
			
		return service_list
		
	def NumServices( self ):
		return len(self.Services())
		
	def Write( self, filename ):
		self.filename = filename
		self.valid = True

		if not self.dirty:
			return
		
		self.Flush()
			
		

######################################################################
## Unit tests
######################################################################

if __name__ == "__main__" :

	x = SambaConf( )
	x.ReadConfig( sys.argv[1] )
	if not x.valid :
		print "Bad file!"
		sys.exit(1)

	x.Dump( sys.stdout )
	
	
	
## end of SambaConfig.py ######################################################
###############################################################################

