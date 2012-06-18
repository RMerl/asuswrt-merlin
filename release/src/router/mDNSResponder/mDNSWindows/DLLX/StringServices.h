/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2009 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):
    
$Log: StringServices.h,v $
Revision 1.1  2009/05/26 04:43:54  herscher
<rdar://problem/3948252> COM component that can be used with any .NET language and VB.


*/

#ifndef _StringServices_h
#define _StringServices_h

#include <atlbase.h>
#include <vector>
#include <string>


extern BOOL
BSTRToUTF8
	(
	BSTR			inString,
	std::string	&	outString
	);


extern BOOL
UTF8ToBSTR
	(
	const char	*	inString,
	CComBSTR	&	outString
	);


extern BOOL
ByteArrayToVariant
	(
	const void	*	inArray,
	size_t			inArrayLen,
	VARIANT		*	outVariant
	);


extern BOOL
VariantToByteArray
	(
	VARIANT				*	inVariant,
	std::vector< BYTE >	&	outArray
	);


#endif