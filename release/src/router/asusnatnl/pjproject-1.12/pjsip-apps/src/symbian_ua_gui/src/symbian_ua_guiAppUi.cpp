/* $Id: symbian_ua_guiAppUi.cpp 3550 2011-05-05 05:33:27Z nanang $ */
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
// [[[ begin generated region: do not modify [Generated System Includes]
#include <eikmenub.h>
#include <akncontext.h>
#include <akntitle.h>
#include <symbian_ua_gui.rsg>
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated User Includes]
#include "symbian_ua_guiAppUi.h"
#include "symbian_ua_gui.hrh"
#include "symbian_ua_guiContainer.hrh"
#include "symbian_ua_guiSettingItemList.hrh"
#include "symbian_ua_guiContainerView.h"
#include "symbian_ua_guiSettingItemListView.h"
// ]]] end generated region [Generated User Includes]

// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]

#include "symbian_ua.h"

/**
 * Construct the Csymbian_ua_guiAppUi instance
 */ 
Csymbian_ua_guiAppUi::Csymbian_ua_guiAppUi() : CTimer(0)
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]

	}

/** 
 * The appui's destructor removes the container from the control
 * stack and destroys it.
 */
Csymbian_ua_guiAppUi::~Csymbian_ua_guiAppUi()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	TRAPD( err_Dlg_wait_init, RemoveDlg_wait_initL() );
	// ]]] end generated region [Generated Contents]
	}

// [[[ begin generated function: do not modify
void Csymbian_ua_guiAppUi::InitializeContainersL()
	{
	iSymbian_ua_guiContainerView = Csymbian_ua_guiContainerView::NewL();
	AddViewL( iSymbian_ua_guiContainerView );
	iSymbian_ua_guiSettingItemListView = Csymbian_ua_guiSettingItemListView::NewL();
	AddViewL( iSymbian_ua_guiSettingItemListView );
	SetDefaultViewL( *iSymbian_ua_guiSettingItemListView );
	}
// ]]] end generated function

/**
 * Handle a command for this appui (override)
 * @param aCommand command id to be handled
 */
void Csymbian_ua_guiAppUi::HandleCommandL( TInt aCommand )
	{
	// [[[ begin generated region: do not modify [Generated Code]
	TBool commandHandled = EFalse;
	switch ( aCommand )
		{ // code to dispatch to the AppUi's menu and CBA commands is generated here
		default:
			break;
		}
	
		
	if ( !commandHandled ) 
		{
		if ( aCommand == EAknSoftkeyExit || aCommand == EEikCmdExit )
			{
		    	symbian_ua_destroy();
			Exit();
			}
		}
	// ]]] end generated region [Generated Code]
	
	}

/** 
 * Override of the HandleResourceChangeL virtual function
 */
void Csymbian_ua_guiAppUi::HandleResourceChangeL( TInt aType )
	{
	CAknViewAppUi::HandleResourceChangeL( aType );
	// [[[ begin generated region: do not modify [Generated Code]
	// ]]] end generated region [Generated Code]
	
	}
				
/** 
 * Override of the HandleKeyEventL virtual function
 * @return EKeyWasConsumed if event was handled, EKeyWasNotConsumed if not
 * @param aKeyEvent 
 * @param aType 
 */
TKeyResponse Csymbian_ua_guiAppUi::HandleKeyEventL(
		const TKeyEvent& aKeyEvent,
		TEventCode aType )
	{
	// The inherited HandleKeyEventL is private and cannot be called
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	return EKeyWasNotConsumed;
	}

/** 
 * Override of the HandleViewDeactivation virtual function
 *
 * @param aViewIdToBeDeactivated 
 * @param aNewlyActivatedViewId 
 */
void Csymbian_ua_guiAppUi::HandleViewDeactivation( 
		const TVwsViewId& aViewIdToBeDeactivated, 
		const TVwsViewId& aNewlyActivatedViewId )
	{
	CAknViewAppUi::HandleViewDeactivation( 
			aViewIdToBeDeactivated, 
			aNewlyActivatedViewId );
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}

/**
 * @brief Completes the second phase of Symbian object construction. 
 * Put initialization code that could leave here. 
 */ 
void Csymbian_ua_guiAppUi::ConstructL()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	BaseConstructL( EAknEnableSkin );
	InitializeContainersL();
	// ]]] end generated region [Generated Contents]

	// Create private folder
	RProcess process;
	TFileName path;
	
	path.Copy( process.FileName().Left(2) );
	
	if(path.Compare(_L("c")) || path.Compare(_L("C")))
		CEikonEnv::Static()->FsSession().CreatePrivatePath(EDriveC);
	else if(path.Compare(_L("e")) || path.Compare(_L("E")))
		CEikonEnv::Static()->FsSession().CreatePrivatePath(EDriveE);	
	
	// Init PJSUA
	if (symbian_ua_init() != 0) {
	    symbian_ua_destroy();
	    Exit();
	}
	
	ExecuteDlg_wait_initLD();

	CTimer::ConstructL();
	CActiveScheduler::Add( this );
	After(4000000);
	}

/** 
 * Override of the HandleApplicationSpecificEventL virtual function
 */
void Csymbian_ua_guiAppUi::HandleApplicationSpecificEventL( 
		TInt aType, 
		const TWsEvent& anEvent )
	{
	CAknViewAppUi::HandleApplicationSpecificEventL( aType, anEvent );
	// [[[ begin generated region: do not modify [Generated Code]
	// ]]] end generated region [Generated Code]
	
	}
				
/** 
 * Handle the applicationSpecificEvent event.
 */
void Csymbian_ua_guiAppUi::HandleSymbian_ua_guiAppUiApplicationSpecificEventL( 
		TInt /* aType */, 
		const TWsEvent& /* anEvent */ )
	{
	// TODO: implement applicationSpecificEvent event handler
	}
				
// [[[ begin generated function: do not modify
/**
 * Execute the wait dialog for dlg_wait_init. This routine returns
 * while the dialog is showing. It will be closed and destroyed when
 * RemoveDlg_wait_initL() or the user selects the Cancel soft key.
 * @param aOverrideText optional override text. When null the text configured
 * in the UI Designer is used.
 */
void Csymbian_ua_guiAppUi::ExecuteDlg_wait_initLD( const TDesC* aOverrideText )
	{
	iDlg_wait_init = new ( ELeave ) CAknWaitDialog( 
			reinterpret_cast< CEikDialog** >( &iDlg_wait_init ), EFalse );
	if ( aOverrideText != NULL )
		{
		iDlg_wait_init->SetTextL( *aOverrideText );
		}
	iDlg_wait_init->ExecuteLD( R_APPLICATION_DLG_WAIT_INIT );
	iDlg_wait_initCallback = new ( ELeave ) CProgressDialogCallback( 
		this, iDlg_wait_init, &Csymbian_ua_guiAppUi::HandleDlg_wait_initCanceledL );
	iDlg_wait_init->SetCallback( iDlg_wait_initCallback );
	}
// ]]] end generated function

// [[[ begin generated function: do not modify
/**
 * Close and dispose of the wait dialog for dlg_wait_init
 */
void Csymbian_ua_guiAppUi::RemoveDlg_wait_initL()
	{
	if ( iDlg_wait_init != NULL )
		{
		iDlg_wait_init->SetCallback( NULL );
		iDlg_wait_init->ProcessFinishedL();    // deletes the dialog
		iDlg_wait_init = NULL;
		}
	delete iDlg_wait_initCallback;
	iDlg_wait_initCallback = NULL;
	
	}
// ]]] end generated function

/** 
 * Handle the canceled event.
 */
void Csymbian_ua_guiAppUi::HandleDlg_wait_initCanceledL( CAknProgressDialog* /* aDialog */ )
	{
	// TODO: implement canceled event handler
	
	}
				
void Csymbian_ua_guiAppUi::RunL()
	{
	RemoveDlg_wait_initL();
	iSymbian_ua_guiSettingItemListView->HandleCommandL(EAknSoftkeySave);
	}
