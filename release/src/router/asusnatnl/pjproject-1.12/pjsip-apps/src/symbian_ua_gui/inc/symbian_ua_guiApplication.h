/* $Id: symbian_ua_guiApplication.h 3550 2011-05-05 05:33:27Z nanang $ */
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
#ifndef SYMBIAN_UA_GUIAPPLICATION_H
#define SYMBIAN_UA_GUIAPPLICATION_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknapp.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Constants]
const TUid KUidsymbian_ua_guiApplication = { 0xEBD12EE4 };
// ]]] end generated region [Generated Constants]

/**
 *
 * @class	Csymbian_ua_guiApplication symbian_ua_guiApplication.h
 * @brief	A CAknApplication-derived class is required by the S60 application 
 *          framework. It is subclassed to create the application's document 
 *          object.
 */
class Csymbian_ua_guiApplication : public CAknApplication
	{
private:
	TUid AppDllUid() const;
	CApaDocument* CreateDocumentL();
	
	};
			
#endif // SYMBIAN_UA_GUIAPPLICATION_H		
