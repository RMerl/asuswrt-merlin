/* $Id: symbian_ua_guiSettingItemListView.h 3550 2011-05-05 05:33:27Z nanang $ */
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
#ifndef SYMBIAN_UA_GUISETTINGITEMLISTVIEW_H
#define SYMBIAN_UA_GUISETTINGITEMLISTVIEW_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknview.h>
#include "Symbian_ua_guiSettingItemListSettings.h"
// ]]] end generated region [Generated Includes]


// [[[ begin [Event Handler Includes]
// ]]] end [Event Handler Includes]

// [[[ begin generated region: do not modify [Generated Forward Declarations]
class CSymbian_ua_guiSettingItemList;
// ]]] end generated region [Generated Forward Declarations]

// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]

/**
 * Avkon view class for symbian_ua_guiSettingItemListView. It is register with the view server
 * by the AppUi. It owns the container control.
 * @class	Csymbian_ua_guiSettingItemListView symbian_ua_guiSettingItemListView.h
 */
class Csymbian_ua_guiSettingItemListView : public CAknView
	{
public:
	// constructors and destructor
	Csymbian_ua_guiSettingItemListView();
	static Csymbian_ua_guiSettingItemListView* NewL();
	static Csymbian_ua_guiSettingItemListView* NewLC();        
	void ConstructL();
	virtual ~Csymbian_ua_guiSettingItemListView();

public:
	// from base class CAknView
	TUid Id() const;
	void HandleCommandL( TInt aCommand );

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
	// ]]] end generated region [Generated Methods]
	
	
	// [[[ begin [Overridden Methods]
protected: 
	// ]]] end [Overridden Methods]
	
	
	// [[[ begin [User Handlers]
protected: 
	TBool HandleChangeSelectedSettingItemL( TInt aCommand );
	TBool HandleControlPaneRightSoftKeyPressedL( TInt aCommand );
	TBool HandleCancelMenuItemSelectedL( TInt aCommand );
	// ]]] end [User Handlers]
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	CSymbian_ua_guiSettingItemList* iSymbian_ua_guiSettingItemList;
	TSymbian_ua_guiSettingItemListSettings* iSettings;
	// ]]] end generated region [Generated Instance Variables]
	
	};

#endif // SYMBIAN_UA_GUISETTINGITEMLISTVIEW_H			
