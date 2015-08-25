/* $Id: symbian_ua_guiDocument.cpp 3550 2011-05-05 05:33:27Z nanang $ */
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
// [[[ begin generated region: do not modify [Generated User Includes]
#include "symbian_ua_guiDocument.h"
#include "symbian_ua_guiAppUi.h"
// ]]] end generated region [Generated User Includes]

/**
 * @brief Constructs the document class for the application.
 * @param anApplication the application instance
 */
Csymbian_ua_guiDocument::Csymbian_ua_guiDocument( CEikApplication& anApplication )
	: CAknDocument( anApplication )
	{
	}

/**
 * @brief Completes the second phase of Symbian object construction. 
 * Put initialization code that could leave here.  
 */ 
void Csymbian_ua_guiDocument::ConstructL()
	{
	}
	
/**
 * Symbian OS two-phase constructor.
 *
 * Creates an instance of Csymbian_ua_guiDocument, constructs it, and
 * returns it.
 *
 * @param aApp the application instance
 * @return the new Csymbian_ua_guiDocument
 */
Csymbian_ua_guiDocument* Csymbian_ua_guiDocument::NewL( CEikApplication& aApp )
	{
	Csymbian_ua_guiDocument* self = new ( ELeave ) Csymbian_ua_guiDocument( aApp );
	CleanupStack::PushL( self );
	self->ConstructL();
	CleanupStack::Pop( self );
	return self;
	}

/**
 * @brief Creates the application UI object for this document.
 * @return the new instance
 */	
CEikAppUi* Csymbian_ua_guiDocument::CreateAppUiL()
	{
	return new ( ELeave ) Csymbian_ua_guiAppUi;
	}
				
