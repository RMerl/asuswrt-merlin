/* $Id: symbian_ua_guiDocument.h 3550 2011-05-05 05:33:27Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef SYMBIAN_UA_GUIDOCUMENT_H
#define SYMBIAN_UA_GUIDOCUMENT_H

#include <akndoc.h>
		
class CEikAppUi;

/**
* @class	Csymbian_ua_guiDocument symbian_ua_guiDocument.h
* @brief	A CAknDocument-derived class is required by the S60 application 
*           framework. It is responsible for creating the AppUi object. 
*/
class Csymbian_ua_guiDocument : public CAknDocument
	{
public: 
	// constructor
	static Csymbian_ua_guiDocument* NewL( CEikApplication& aApp );

private: 
	// constructors
	Csymbian_ua_guiDocument( CEikApplication& aApp );
	void ConstructL();
	
public: 
	// from base class CEikDocument
	CEikAppUi* CreateAppUiL();
	};
#endif // SYMBIAN_UA_GUIDOCUMENT_H
