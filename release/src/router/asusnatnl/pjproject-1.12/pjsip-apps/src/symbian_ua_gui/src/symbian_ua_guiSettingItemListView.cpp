/* $Id: symbian_ua_guiSettingItemListView.cpp 3550 2011-05-05 05:33:27Z nanang $ */
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
#include <aknviewappui.h>
#include <eikmenub.h>
#include <avkon.hrh>
#include <akncontext.h>
#include <akntitle.h>
#include <stringloader.h>
#include <barsread.h>
#include <eikbtgpc.h>
#include <symbian_ua_gui.rsg>
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated User Includes]
#include "symbian_ua_gui.hrh"
#include "symbian_ua_guiSettingItemListView.h"
#include "symbian_ua_guiContainer.hrh"
#include "symbian_ua_guiSettingItemList.hrh"
#include "symbian_ua_guiSettingItemList.h"
// ]]] end generated region [Generated User Includes]

#include "symbian_ua.h"

// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]

/**
 * First phase of Symbian two-phase construction. Should not contain any
 * code that could leave.
 */
Csymbian_ua_guiSettingItemListView::Csymbian_ua_guiSettingItemListView()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}
/** 
 * The view's destructor removes the container from the control
 * stack and destroys it.
 */
Csymbian_ua_guiSettingItemListView::~Csymbian_ua_guiSettingItemListView()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}

/**
 * Symbian two-phase constructor.
 * This creates an instance then calls the second-phase constructor
 * without leaving the instance on the cleanup stack.
 * @return new instance of Csymbian_ua_guiSettingItemListView
 */
Csymbian_ua_guiSettingItemListView* Csymbian_ua_guiSettingItemListView::NewL()
	{
	Csymbian_ua_guiSettingItemListView* self = Csymbian_ua_guiSettingItemListView::NewLC();
	CleanupStack::Pop( self );
	return self;
	}

/**
 * Symbian two-phase constructor.
 * This creates an instance, pushes it on the cleanup stack,
 * then calls the second-phase constructor.
 * @return new instance of Csymbian_ua_guiSettingItemListView
 */
Csymbian_ua_guiSettingItemListView* Csymbian_ua_guiSettingItemListView::NewLC()
	{
	Csymbian_ua_guiSettingItemListView* self = new ( ELeave ) Csymbian_ua_guiSettingItemListView();
	CleanupStack::PushL( self );
	self->ConstructL();
	return self;
	}


/**
 * Second-phase constructor for view.  
 * Initialize contents from resource.
 */ 
void Csymbian_ua_guiSettingItemListView::ConstructL()
	{
	// [[[ begin generated region: do not modify [Generated Code]
	BaseConstructL( R_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_VIEW );
	// ]]] end generated region [Generated Code]
	
	// add your own initialization code here
	}
	
/**
 * @return The UID for this view
 */
TUid Csymbian_ua_guiSettingItemListView::Id() const
	{
	return TUid::Uid( ESymbian_ua_guiSettingItemListViewId );
	}

/**
 * Handle a command for this view (override)
 * @param aCommand command id to be handled
 */
void Csymbian_ua_guiSettingItemListView::HandleCommandL( TInt aCommand )
	{   
	// [[[ begin generated region: do not modify [Generated Code]
	TBool commandHandled = EFalse;
	switch ( aCommand )
		{	// code to dispatch to the AknView's menu and CBA commands is generated here
	
		case EAknSoftkeySave:
			commandHandled = HandleControlPaneRightSoftKeyPressedL( aCommand );
			break;
		case ESymbian_ua_guiSettingItemListViewMenuItem1Command:
			commandHandled = HandleChangeSelectedSettingItemL( aCommand );
			break;
		default:
			break;
		}
	
		
	if ( !commandHandled ) 
		{
	
		}
	// ]]] end generated region [Generated Code]
	
	}

/**
 *	Handles user actions during activation of the view, 
 *	such as initializing the content.
 */
void Csymbian_ua_guiSettingItemListView::DoActivateL(
		const TVwsViewId& /*aPrevViewId*/,
		TUid /*aCustomMessageId*/,
		const TDesC8& /*aCustomMessage*/ )
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	SetupStatusPaneL();
	
	CEikButtonGroupContainer* cba = AppUi()->Cba();
	if ( cba != NULL ) 
		{
		cba->MakeVisible( EFalse );
		}
	
	if ( iSymbian_ua_guiSettingItemList == NULL )
		{
		iSettings = TSymbian_ua_guiSettingItemListSettings::NewL();
		iSymbian_ua_guiSettingItemList = new ( ELeave ) CSymbian_ua_guiSettingItemList( *iSettings, this );
		iSymbian_ua_guiSettingItemList->SetMopParent( this );
		iSymbian_ua_guiSettingItemList->ConstructFromResourceL( R_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_SYMBIAN_UA_GUI_SETTING_ITEM_LIST );
		iSymbian_ua_guiSettingItemList->ActivateL();
		iSymbian_ua_guiSettingItemList->LoadSettingValuesL();
		iSymbian_ua_guiSettingItemList->LoadSettingsL();
		AppUi()->AddToStackL( *this, iSymbian_ua_guiSettingItemList );
		} 
	// ]]] end generated region [Generated Contents]
	
	}

/**
 */
void Csymbian_ua_guiSettingItemListView::DoDeactivate()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	CleanupStatusPane();
	
	CEikButtonGroupContainer* cba = AppUi()->Cba();
	if ( cba != NULL ) 
		{
		cba->MakeVisible( ETrue );
		cba->DrawDeferred();
		}
	
	if ( iSymbian_ua_guiSettingItemList != NULL )
		{
		AppUi()->RemoveFromStack( iSymbian_ua_guiSettingItemList );
		delete iSymbian_ua_guiSettingItemList;
		iSymbian_ua_guiSettingItemList = NULL;
		delete iSettings;
		iSettings = NULL;
		}
	// ]]] end generated region [Generated Contents]
	
	}

// [[[ begin generated function: do not modify
void Csymbian_ua_guiSettingItemListView::SetupStatusPaneL()
	{
	// reset the context pane
	TUid contextPaneUid = TUid::Uid( EEikStatusPaneUidContext );
	CEikStatusPaneBase::TPaneCapabilities subPaneContext = 
		StatusPane()->PaneCapabilities( contextPaneUid );
	if ( subPaneContext.IsPresent() && subPaneContext.IsAppOwned() )
		{
		CAknContextPane* context = static_cast< CAknContextPane* > ( 
			StatusPane()->ControlL( contextPaneUid ) );
		context->SetPictureToDefaultL();
		}
	
	// setup the title pane
	TUid titlePaneUid = TUid::Uid( EEikStatusPaneUidTitle );
	CEikStatusPaneBase::TPaneCapabilities subPaneTitle = 
		StatusPane()->PaneCapabilities( titlePaneUid );
	if ( subPaneTitle.IsPresent() && subPaneTitle.IsAppOwned() )
		{
		CAknTitlePane* title = static_cast< CAknTitlePane* >( 
			StatusPane()->ControlL( titlePaneUid ) );
		TResourceReader reader;
		iEikonEnv->CreateResourceReaderLC( reader, R_SYMBIAN_UA_GUI_SETTING_ITEM_LIST_TITLE_RESOURCE );
		title->SetFromResourceL( reader );
		CleanupStack::PopAndDestroy(); // reader internal state
		}
				
	}
// ]]] end generated function

// [[[ begin generated function: do not modify
void Csymbian_ua_guiSettingItemListView::CleanupStatusPane()
	{
	}
// ]]] end generated function

/** 
 * Handle status pane size change for this view (override)
 */
void Csymbian_ua_guiSettingItemListView::HandleStatusPaneSizeChange()
	{
	CAknView::HandleStatusPaneSizeChange();
	
	// this may fail, but we're not able to propagate exceptions here
	TInt result;
	TRAP( result, SetupStatusPaneL() ); 
	}
	
/** 
 * Handle the selected event.
 * @param aCommand the command id invoked
 * @return ETrue if the command was handled, EFalse if not
 */
TBool Csymbian_ua_guiSettingItemListView::HandleChangeSelectedSettingItemL( TInt aCommand )
	{
	iSymbian_ua_guiSettingItemList->ChangeSelectedItemL();
	return ETrue;
	}
								
/** 
 * Handle the rightSoftKeyPressed event.
 * @return ETrue if the command was handled, EFalse if not
 */
TBool Csymbian_ua_guiSettingItemListView::HandleControlPaneRightSoftKeyPressedL( TInt aCommand )
	{
	TUint8 domain[256] = {0};
	TPtr8 cDomain(domain, sizeof(domain));
	TUint8 user[64] = {0};
	TPtr8 cUser(user, sizeof(user));
	TUint8 pass[64] = {0};
	TPtr8 cPass(pass, sizeof(pass));
	
	cDomain.Copy(iSettings->Ed_registrar());
	cUser.Copy(iSettings->Ed_user());
	cPass.Copy(iSettings->Ed_password());
	symbian_ua_set_account((char*)domain, (char*)user, (char*)pass, false, false);
	
	AppUi()->ActivateLocalViewL(TUid::Uid(ESymbian_ua_guiContainerViewId));
	return ETrue;
	}

/** 
 * Handle the selected event.
 * @param aCommand the command id invoked
 * @return ETrue if the command was handled, EFalse if not
 */
TBool Csymbian_ua_guiSettingItemListView::HandleCancelMenuItemSelectedL( TInt aCommand )
	{
	AppUi()->ActivateLocalViewL(TUid::Uid(ESymbian_ua_guiContainerViewId));
	return ETrue;
	}
				
