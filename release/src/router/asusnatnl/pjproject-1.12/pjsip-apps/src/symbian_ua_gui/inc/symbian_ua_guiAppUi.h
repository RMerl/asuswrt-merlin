/* $Id: symbian_ua_guiAppUi.h 3550 2011-05-05 05:33:27Z nanang $ */
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
#ifndef SYMBIAN_UA_GUIAPPUI_H
#define SYMBIAN_UA_GUIAPPUI_H

// [[[ begin generated region: do not modify [Generated Includes]
#include <aknviewappui.h>
#include <aknwaitdialog.h>
// ]]] end generated region [Generated Includes]

// [[[ begin generated region: do not modify [Generated Forward Declarations]
class Csymbian_ua_guiContainerView;
class Csymbian_ua_guiSettingItemListView;
// ]]] end generated region [Generated Forward Declarations]

/**
 * @class	Csymbian_ua_guiAppUi symbian_ua_guiAppUi.h
 * @brief The AppUi class handles application-wide aspects of the user interface, including
 *        view management and the default menu, control pane, and status pane.
 */
class Csymbian_ua_guiAppUi : public CAknViewAppUi, public CTimer
	{
public: 
	// constructor and destructor
	Csymbian_ua_guiAppUi();
	virtual ~Csymbian_ua_guiAppUi();
	void ConstructL();

public:
	// from CCoeAppUi
	TKeyResponse HandleKeyEventL(
				const TKeyEvent& aKeyEvent,
				TEventCode aType );

	// from CEikAppUi
	void HandleCommandL( TInt aCommand );
	void HandleResourceChangeL( TInt aType );

	// from CAknAppUi
	void HandleViewDeactivation( 
			const TVwsViewId& aViewIdToBeDeactivated, 
			const TVwsViewId& aNewlyActivatedViewId );

private:
	void InitializeContainersL();
	// [[[ begin generated region: do not modify [Generated Methods]
public: 
	void ExecuteDlg_wait_initLD( const TDesC* aOverrideText = NULL );
	void RemoveDlg_wait_initL();
	// ]]] end generated region [Generated Methods]
	
	// [[[ begin generated region: do not modify [Generated Instance Variables]
private: 
	CAknWaitDialog* iDlg_wait_init;
	class CProgressDialogCallback;
	CProgressDialogCallback* iDlg_wait_initCallback;
	Csymbian_ua_guiContainerView* iSymbian_ua_guiContainerView;
	Csymbian_ua_guiSettingItemListView* iSymbian_ua_guiSettingItemListView;
	// ]]] end generated region [Generated Instance Variables]
	
	
	// [[[ begin [User Handlers]
protected: 
	void HandleSymbian_ua_guiAppUiApplicationSpecificEventL( 
			TInt aType, 
			const TWsEvent& anEvent );
	void HandleDlg_wait_initCanceledL( CAknProgressDialog* aDialog );
	// ]]] end [User Handlers]
	
	
	// [[[ begin [Overridden Methods]
protected: 
	void HandleApplicationSpecificEventL( 
			TInt aType, 
			const TWsEvent& anEvent );
	// ]]] end [Overridden Methods]
	
	virtual void RunL();
	
	// [[[ begin [MProgressDialogCallback support]
private: 
	typedef void ( Csymbian_ua_guiAppUi::*ProgressDialogEventHandler )( 
			CAknProgressDialog* aProgressDialog );
	
	/**
	 * This is a helper class for progress/wait dialog callbacks. It routes the dialog's
	 * cancel notification to the handler function for the cancel event.
	 */
	class CProgressDialogCallback : public CBase, public MProgressDialogCallback
		{ 
		public:
			CProgressDialogCallback( 
					Csymbian_ua_guiAppUi* aHandlerObj, 
					CAknProgressDialog* aDialog, 
					ProgressDialogEventHandler aHandler ) :
				handlerObj( aHandlerObj ), dialog( aDialog ), handler( aHandler )
				{}
				
			void DialogDismissedL( TInt aButtonId ) 
				{
				( handlerObj->*handler )( dialog );
				}
		private:
			Csymbian_ua_guiAppUi* handlerObj;
			CAknProgressDialog* dialog;
			ProgressDialogEventHandler handler;
		};
		
	// ]]] end [MProgressDialogCallback support]
	
	};

#endif // SYMBIAN_UA_GUIAPPUI_H			
