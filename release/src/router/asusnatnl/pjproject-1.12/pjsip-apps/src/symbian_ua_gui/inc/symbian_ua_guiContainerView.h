/* $Id: symbian_ua_guiContainerView.h 3550 2011-05-05 05:33:27Z nanang $ */
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
#ifndef SYMBIAN_UA_GUICONTAINERVIEW_H
#define SYMBIAN_UA_GUICONTAINERVIEW_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknview.h>
// ]]] end generated region [Generated Includes]


// [[[ begin [Event Handler Includes]
// ]]] end [Event Handler Includes]

// [[[ begin generated region: do not modify [Generated Forward Declarations]
class CSymbian_ua_guiContainer;
// ]]] end generated region [Generated Forward Declarations]

// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]

/**
 * Avkon view class for symbian_ua_guiContainerView. It is register with the view server
 * by the AppUi. It owns the container control.
 * @class	Csymbian_ua_guiContainerView symbian_ua_guiContainerView.h
 */
class Csymbian_ua_guiContainerView : public CAknView
	{
public:
	// constructors and destructor
	Csymbian_ua_guiContainerView();
	static Csymbian_ua_guiContainerView* NewL();
	static Csymbian_ua_guiContainerView* NewLC();        
	void ConstructL();
	virtual ~Csymbian_ua_guiContainerView();

public:
	// from base class CAknView
	TUid Id() const;
	void HandleCommandL( TInt aCommand );
	
	void PutMessage(const TDesC &msg);

protected:
	// from base class CAknView
	void DoActivateL(
		const TVwsViewId& aPrevViewId,
		TUid aCustomMessageId,
		const TDesC8& aCustomMessage );
	void DoDeactivate();
	void HandleStatusPaneSizeChange();
	
private:
	void SetupStatusPaneL();
	void CleanupStatusPane();
	// [[[ begin generated region: do not modify [Generated Methods]
public: 
	static void RunNote_errorL( const TDesC* aOverrideText = NULL );
	static void RunNote_infoL( const TDesC* aOverrideText = NULL );
	static void RunNote_warningL( const TDesC* aOverrideText = NULL );
	static TInt RunQry_accept_callL( const TDesC* aOverrideText = NULL );
	// ]]] end generated region [Generated Methods]
	
	
	// [[[ begin [Overridden Methods]
protected: 
	// ]]] end [Overridden Methods]
	
	
	// [[[ begin [User Handlers]
protected: 
	TBool CallSoftKeyPressedL( TInt aCommand );
	TBool HandleSettingMenuItemSelectedL( TInt aCommand );
	// ]]] end [User Handlers]
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	CSymbian_ua_guiContainer* iSymbian_ua_guiContainer;
	// ]]] end generated region [Generated Instance Variables]
	
	};

#endif // SYMBIAN_UA_GUICONTAINERVIEW_H			
