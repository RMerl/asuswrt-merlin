/* $Id: symbian_ua_guiContainerView.cpp 3550 2011-05-05 05:33:27Z nanang $ */
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
#include <barsread.h>
#include <stringloader.h>
#include <eiklabel.h>
#include <eikenv.h>
#include <gdi.h>
#include <eikedwin.h>
#include <akncontext.h>
#include <akntitle.h>
#include <eikbtgpc.h>
#include <aknnotewrappers.h>
#include <aknquerydialog.h>
#include <symbian_ua_gui.rsg>
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated User Includes]
#include "symbian_ua_gui.hrh"
#include "symbian_ua_guiContainerView.h"
#include "symbian_ua_guiContainer.hrh"
#include "symbian_ua_guiSettingItemList.hrh"
#include "symbian_ua_guiContainer.h"
// ]]] end generated region [Generated User Includes]

#include <utf.h>
#include "symbian_ua.h"

// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]

Csymbian_ua_guiContainerView *myinstance = NULL;
_LIT(KStCall, "Call");
_LIT(KStHangUp, "Hang Up");

void on_info(const wchar_t* buf)
{
	TPtrC aBuf((const TUint16*)buf);
	
	if (myinstance)
		myinstance->PutMessage(aBuf);
}

void on_incoming_call(const wchar_t* caller_disp, const wchar_t* caller_uri)
{
	TBuf<512> buf;
	TPtrC aDisp((const TUint16*)caller_disp);
	TPtrC aUri((const TUint16*)caller_uri);
	_LIT(KFormat, "Incoming call from %S, accept?");
	
	buf.Format(KFormat, &aDisp);
	if (Csymbian_ua_guiContainerView::RunQry_accept_callL(&buf) == EAknSoftkeyYes)
	{
		CEikButtonGroupContainer* cba = CEikButtonGroupContainer::Current();
		if (cba != NULL) {
			TRAPD(result, cba->SetCommandL(ESymbian_ua_guiContainerViewControlPaneRightId, KStHangUp));
			cba->DrawDeferred();
		}
		symbian_ua_answercall();
	} else {
		symbian_ua_endcall();	
	}
}

void on_call_end(const wchar_t* reason)
{
	TPtrC aReason((const TUint16*)reason);
	
	CEikButtonGroupContainer* cba = CEikButtonGroupContainer::Current();
	if (cba != NULL) {
		TRAPD(result, cba->SetCommandL(ESymbian_ua_guiContainerViewControlPaneRightId, KStCall));
		cba->DrawDeferred();
	}
	
	Csymbian_ua_guiContainerView::RunNote_infoL(&aReason);
}

void on_reg_state(bool success)
{
	if (success)
		Csymbian_ua_guiContainerView::RunNote_infoL();
	else
		Csymbian_ua_guiContainerView::RunNote_warningL();
}

void on_unreg_state(bool success)
{
	TPtrC st_success(_L("Unregistration Success!"));
	TPtrC st_failed(_L("Unregistration Failed!"));
	
	if (success)
		Csymbian_ua_guiContainerView::RunNote_infoL(&st_success);
	else
		Csymbian_ua_guiContainerView::RunNote_warningL(&st_failed);
}

void Csymbian_ua_guiContainerView::PutMessage(const TDesC &msg)
	{
	if (!iSymbian_ua_guiContainer)
		return;
	
	CEikEdwin *obj_info = (CEikEdwin*) iSymbian_ua_guiContainer->ComponentControl(iSymbian_ua_guiContainer->EEd_info);

	obj_info->SetTextL(&msg);
	obj_info->DrawDeferred();
	}

/**
 * First phase of Symbian two-phase construction. Should not contain any
 * code that could leave.
 */
Csymbian_ua_guiContainerView::Csymbian_ua_guiContainerView()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	iSymbian_ua_guiContainer = NULL;
	// ]]] end generated region [Generated Contents]
	
	}
/** 
 * The view's destructor removes the container from the control
 * stack and destroys it.
 */
Csymbian_ua_guiContainerView::~Csymbian_ua_guiContainerView()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	delete iSymbian_ua_guiContainer;
	iSymbian_ua_guiContainer = NULL;
	// ]]] end generated region [Generated Contents]
	
	symbian_ua_set_info_callback(NULL);
	myinstance = NULL;
	}

/**
 * Symbian two-phase constructor.
 * This creates an instance then calls the second-phase constructor
 * without leaving the instance on the cleanup stack.
 * @return new instance of Csymbian_ua_guiContainerView
 */
Csymbian_ua_guiContainerView* Csymbian_ua_guiContainerView::NewL()
	{
	Csymbian_ua_guiContainerView* self = Csymbian_ua_guiContainerView::NewLC();
	CleanupStack::Pop( self );
	return self;
	}

/**
 * Symbian two-phase constructor.
 * This creates an instance, pushes it on the cleanup stack,
 * then calls the second-phase constructor.
 * @return new instance of Csymbian_ua_guiContainerView
 */
Csymbian_ua_guiContainerView* Csymbian_ua_guiContainerView::NewLC()
	{
	Csymbian_ua_guiContainerView* self = new ( ELeave ) Csymbian_ua_guiContainerView();
	CleanupStack::PushL( self );
	self->ConstructL();
	return self;
	}


/**
 * Second-phase constructor for view.  
 * Initialize contents from resource.
 */ 
void Csymbian_ua_guiContainerView::ConstructL()
	{
	// [[[ begin generated region: do not modify [Generated Code]
	BaseConstructL( R_SYMBIAN_UA_GUI_CONTAINER_SYMBIAN_UA_GUI_CONTAINER_VIEW );
	// ]]] end generated region [Generated Code]
	
	// add your own initialization code here
	symbian_ua_info_cb_t cb;
	Mem::FillZ(&cb, sizeof(cb));

	cb.on_info = &on_info;
	cb.on_incoming_call = &on_incoming_call;
	cb.on_reg_state = &on_reg_state;
	cb.on_unreg_state = &on_unreg_state;
	cb.on_call_end = &on_call_end;
	
	symbian_ua_set_info_callback(&cb);
	myinstance = this;
	}
	
/**
 * @return The UID for this view
 */
TUid Csymbian_ua_guiContainerView::Id() const
	{
	return TUid::Uid( ESymbian_ua_guiContainerViewId );
	}

/**
 * Handle a command for this view (override)
 * @param aCommand command id to be handled
 */
void Csymbian_ua_guiContainerView::HandleCommandL( TInt aCommand )
	{   
	// [[[ begin generated region: do not modify [Generated Code]
	TBool commandHandled = EFalse;
	switch ( aCommand )
		{	// code to dispatch to the AknView's menu and CBA commands is generated here
	
		case ESymbian_ua_guiContainerViewControlPaneRightId:
			commandHandled = CallSoftKeyPressedL( aCommand );
			break;
		case ESymbian_ua_guiContainerViewSettingMenuItemCommand:
			commandHandled = HandleSettingMenuItemSelectedL( aCommand );
			break;
		default:
			break;
		}
	
		
	if ( !commandHandled ) 
		{
	
		if ( aCommand == ESymbian_ua_guiContainerViewControlPaneRightId )
			{
			AppUi()->HandleCommandL( EEikCmdExit );
			}
	
		}
	// ]]] end generated region [Generated Code]
	
	}

/**
 *	Handles user actions during activation of the view, 
 *	such as initializing the content.
 */
void Csymbian_ua_guiContainerView::DoActivateL(
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
	
	if ( iSymbian_ua_guiContainer == NULL )
		{
		iSymbian_ua_guiContainer = CSymbian_ua_guiContainer::NewL( ClientRect(), NULL, this );
		iSymbian_ua_guiContainer->SetMopParent( this );
		AppUi()->AddToStackL( *this, iSymbian_ua_guiContainer );
		} 
	// ]]] end generated region [Generated Contents]
	
	cba = CEikButtonGroupContainer::Current();
	if (cba != NULL) {
		if (symbian_ua_anycall())
			cba->SetCommandL(ESymbian_ua_guiContainerViewControlPaneRightId, KStHangUp);
		else
			cba->SetCommandL(ESymbian_ua_guiContainerViewControlPaneRightId, KStCall);
	}
	
	}

/**
 */
void Csymbian_ua_guiContainerView::DoDeactivate()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	CleanupStatusPane();
	
	CEikButtonGroupContainer* cba = AppUi()->Cba();
	if ( cba != NULL ) 
		{
		cba->MakeVisible( ETrue );
		cba->DrawDeferred();
		}
	
	if ( iSymbian_ua_guiContainer != NULL )
		{
		AppUi()->RemoveFromViewStack( *this, iSymbian_ua_guiContainer );
		delete iSymbian_ua_guiContainer;
		iSymbian_ua_guiContainer = NULL;
		}
	// ]]] end generated region [Generated Contents]
	
	}

// [[[ begin generated function: do not modify
void Csymbian_ua_guiContainerView::SetupStatusPaneL()
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
		iEikonEnv->CreateResourceReaderLC( reader, R_SYMBIAN_UA_GUI_CONTAINER_TITLE_RESOURCE );
		title->SetFromResourceL( reader );
		CleanupStack::PopAndDestroy(); // reader internal state
		}
				
	}
// ]]] end generated function

// [[[ begin generated function: do not modify
void Csymbian_ua_guiContainerView::CleanupStatusPane()
	{
	}
// ]]] end generated function

/** 
 * Handle status pane size change for this view (override)
 */
void Csymbian_ua_guiContainerView::HandleStatusPaneSizeChange()
	{
	CAknView::HandleStatusPaneSizeChange();
	
	// this may fail, but we're not able to propagate exceptions here
	TInt result;
	TRAP( result, SetupStatusPaneL() ); 
	}
	
/** 
 * Handle the rightSoftKeyPressed event.
 * @return ETrue if the command was handled, EFalse if not
 */
TBool Csymbian_ua_guiContainerView::CallSoftKeyPressedL( TInt aCommand )
	{
	CEikEdwin *obj_url = (CEikEdwin*) iSymbian_ua_guiContainer->ComponentControl(iSymbian_ua_guiContainer->EEd_url);
	CEikButtonGroupContainer* cba = CEikButtonGroupContainer::Current();
	
	if (symbian_ua_anycall()) {
		symbian_ua_endcall();
		return ETrue;
	}

	PutMessage(_L("Making call..."));
	if ( cba != NULL ) {
		cba->SetCommandL(aCommand, KStHangUp);
		cba->DrawDeferred();
	}
	

	TUint8 url[256];
	TPtr8 aUrl(url, 256);

	HBufC *buf = obj_url->GetTextInHBufL();
	CnvUtfConverter::ConvertFromUnicodeToUtf8(aUrl, *buf);
	delete buf;

	if (symbian_ua_makecall((char *)aUrl.PtrZ()) != 0) {
		PutMessage(_L("Making call failed!"));
		if ( cba != NULL ) {
			cba->SetCommandL(aCommand, KStCall);
			cba->DrawDeferred();
		}
	}
	
	return ETrue;
	}
				
/** 
 * Handle the selected event.
 * @param aCommand the command id invoked
 * @return ETrue if the command was handled, EFalse if not
 */
TBool Csymbian_ua_guiContainerView::HandleSettingMenuItemSelectedL( TInt aCommand )
	{
	AppUi()->ActivateLocalViewL(TUid::Uid(ESymbian_ua_guiSettingItemListViewId));
	return ETrue;
	}
				
// [[[ begin generated function: do not modify
/**
 * Show the popup note for note_error
 * @param aOverrideText optional override text
 */
void Csymbian_ua_guiContainerView::RunNote_errorL( const TDesC* aOverrideText )
	{
	CAknErrorNote* note = new ( ELeave ) CAknErrorNote();
	if ( aOverrideText == NULL )
		{
		HBufC* noteText = StringLoader::LoadLC( R_SYMBIAN_UA_GUI_CONTAINER_NOTE_ERROR );
		note->ExecuteLD( *noteText );
		CleanupStack::PopAndDestroy( noteText );
		}
	else
		{
		note->ExecuteLD( *aOverrideText );
		}
	}
// ]]] end generated function

// [[[ begin generated function: do not modify
/**
 * Show the popup note for note_info
 * @param aOverrideText optional override text
 */
void Csymbian_ua_guiContainerView::RunNote_infoL( const TDesC* aOverrideText )
	{
	CAknInformationNote* note = new ( ELeave ) CAknInformationNote();
	if ( aOverrideText == NULL )
		{
		HBufC* noteText = StringLoader::LoadLC( R_SYMBIAN_UA_GUI_CONTAINER_NOTE_INFO );
		note->ExecuteLD( *noteText );
		CleanupStack::PopAndDestroy( noteText );
		}
	else
		{
		note->ExecuteLD( *aOverrideText );
		}
	}
// ]]] end generated function

// [[[ begin generated function: do not modify
/**
 * Show the popup note for note_warning
 * @param aOverrideText optional override text
 */
void Csymbian_ua_guiContainerView::RunNote_warningL( const TDesC* aOverrideText )
	{
	CAknWarningNote* note = new ( ELeave ) CAknWarningNote();
	if ( aOverrideText == NULL )
		{
		HBufC* noteText = StringLoader::LoadLC( R_SYMBIAN_UA_GUI_CONTAINER_NOTE_WARNING );
		note->ExecuteLD( *noteText );
		CleanupStack::PopAndDestroy( noteText );
		}
	else
		{
		note->ExecuteLD( *aOverrideText );
		}
	}
// ]]] end generated function

// [[[ begin generated function: do not modify
/**
 * Show the popup dialog for qry_accept_call
 * @param aOverrideText optional override text
 * @return EAknSoftkeyYes (left soft key id) or 0
 */
TInt Csymbian_ua_guiContainerView::RunQry_accept_callL( const TDesC* aOverrideText )
	{
				
	CAknQueryDialog* queryDialog = CAknQueryDialog::NewL();	
	
	if ( aOverrideText != NULL )
		{
		queryDialog->SetPromptL( *aOverrideText );
		}
	return queryDialog->ExecuteLD( R_SYMBIAN_UA_GUI_CONTAINER_QRY_ACCEPT_CALL );
	}
// ]]] end generated function

