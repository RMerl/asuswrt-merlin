/* 
   ldb database library

   Copyright (C) Simo Sorce 2005

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "convert.h"
#include "includes.h"
#include "ldb/include/includes.h"

/* Shared map for converting syntax between formats */
static const struct syntax_map syntax_map[] = {
	{ 
		.Standard_OID = "1.3.6.1.4.1.1466.115.121.1.12", 
		.AD_OID = "2.5.5.1", 
		.equality = "distinguishedNameMatch",
		.comment = "Object(DS-DN) == a DN" 
	},
	{
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.38",
		.AD_OID =  "2.5.5.2",
		.equality = "objectIdentifierMatch",
		.comment =  "OID String"
	},
	{ 
		.Standard_OID =  "1.2.840.113556.1.4.905", 
		.AD_OID =  "2.5.5.4",
		.equality = "caseIgnoreMatch",
		.substring = "caseIgnoreSubstringsMatch",
		.comment =   "Case Insensitive String" 
	},
	{
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.26",
		.AD_OID =   "2.5.5.5",
		.equality = "caseExactIA5Match",
		.comment = "Printable String"
	},
	{
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.36",
		.AD_OID =   "2.5.5.6", 
		.equality = "numericStringMatch",
		.substring = "numericStringSubstringsMatch",
		.comment = "Numeric String" 
	},
	{ 
		.Standard_OID =  "1.2.840.113556.1.4.903", 
		.AD_OID =  "2.5.5.7", 
		.equality = "distinguishedNameMatch",
		.comment = "OctetString: Binary+DN" 
	},
	{ 
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.7",
		.AD_OID =   "2.5.5.8", 
		.equality = "booleanMatch",
		.comment = "Boolean" 
	},
	{ 
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.27",
		.AD_OID =   "2.5.5.9", 
		.equality = "integerMatch",
		.comment = "Integer" 
	},
	{ 
		.Standard_OID = "1.3.6.1.4.1.1466.115.121.1.40",
		.AD_OID       = "2.5.5.10",
		.equality     = "octetStringMatch",
		.comment      =  "Octet String"
	},
	{
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.24",
		.AD_OID =   "2.5.5.11", 
		.equality = "generalizedTimeMatch",
		.comment = "Generalized Time"
	},
	{ 
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.53",
		.AD_OID =   "2.5.5.11", 
		.equality = "generalizedTimeMatch",
		.comment = "UTC Time" 
	},
	{ 
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.15",
		.AD_OID =   "2.5.5.12", 
		.equality = "caseIgnoreMatch",
		.substring = "caseIgnoreSubstringsMatch",
		.comment = "Directory String"
	},
	{
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.43",
		.AD_OID =   "2.5.5.13", 
		.comment = "Presentation Address" 
	},
	{
		.Standard_OID =   "Not Found Yet", 
		.AD_OID =  "2.5.5.14", 
		.equality = "distinguishedNameMatch",
		.comment = "OctetString: String+DN" 
	},
	{
		.Standard_OID =  "1.2.840.113556.1.4.907",
		.AD_OID =   "2.5.5.15", 
		.equality     = "octetStringMatch",
		.comment = "NT Security Descriptor"
	},
	{ 
		.Standard_OID =  "1.2.840.113556.1.4.906", 
		.AD_OID =  "2.5.5.16", 
		.equality = "integerMatch",
		.comment = "Large Integer" 
	},
	{
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.40",
		.AD_OID =   "2.5.5.17",
		.equality     = "octetStringMatch",
		.comment =  "Octet String - Security Identifier (SID)" 
	},
	{ 
		.Standard_OID =  "1.3.6.1.4.1.1466.115.121.1.26", 
		.AD_OID =  "2.5.5.5", 
		.equality = "caseExactIA5Match",
		.comment = "IA5 String" 
	},
	{	.Standard_OID = NULL
	}
};


const struct syntax_map *find_syntax_map_by_ad_oid(const char *ad_oid) 
{
	int i;
	for (i=0; syntax_map[i].Standard_OID; i++) {
		if (strcasecmp(ad_oid, syntax_map[i].AD_OID) == 0) {
			return &syntax_map[i];
		}
	}
	return NULL;
}

const struct syntax_map *find_syntax_map_by_standard_oid(const char *standard_oid) 
{
	int i;
	for (i=0; syntax_map[i].Standard_OID; i++) {
		if (strcasecmp(standard_oid, syntax_map[i].Standard_OID) == 0) {
			return &syntax_map[i];
		}
	}
	return NULL;
}
