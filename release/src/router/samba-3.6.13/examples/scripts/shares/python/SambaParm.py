######################################################################
##
##  smb.conf parameter classes
##
##  Copyright (C) Gerald Carter		       2004.
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 3 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, see <http://www.gnu.org/licenses/>.
##
######################################################################

import string

#####################################################################
## Base class for Samba smb.conf parameters
class SambaParm :
	def __init__( self ) :
		pass

	def StringValue( self ) :
		return self.value

#####################################################################
## Boolean smb,conf parm
class SambaParmBool( SambaParm ):
	def __init__( self, value ) :
		x = string.upper(value)
		self.valid = True
		
		if x=="YES" or x=="TRUE" or x=="1":
			self.value = True
		elif x=="NO" or x=="FALSE" or x=="0":
			self.value = False
		else:
			self.valid = False
			return self

	def SetValue( self, value ) :
		x = string.upper(value)
		self.valid = True
		
		if x=="YES" or x=="TRUE" or x=="1":
			self.value = True
		elif x=="NO" or x=="FALSE" or x=="0":
			self.value = False
		else:
			self.valid = False
			return
			
	def StringValue( self ) :
		if  self.value :
			return "yes"
		else:
			return "no"
			
#####################################################################
## Boolean smb,conf parm (inverts)
class SambaParmBoolRev( SambaParmBool ) :
	def __init__( self, value ):
		SambaParmBool.__init__( self, value )
		if self.valid :
			self.value = not self.value
			

#####################################################################
## string smb.conf parms
class SambaParmString( SambaParm ):
	def __init__( self, value ):
		self.value = value
		self.valid = True



