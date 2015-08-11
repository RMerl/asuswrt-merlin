/* $Id: symbian_ua_guiSettingItemList.cpp 3550 2011-05-05 05:33:27Z nanang $ */
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
#include <avkon.hrh>
#include <avkon.rsg>
#include <eikmenup.h>
#include <aknappui.h>
#include <eikcmobs.h>
#include <barsread.h>
#include <stringloader.h>
#include <gdi.h>
#include <eikedwin.h>
#include <eikenv.h>
#include <eikseced.h>
#include <aknpopupfieldtext.h>
#include <eikappui.h>
#include <aknviewappui.h>
#include <akntextsettingpage.h> 
#include <symbian_ua_gui.rsg>
// ]]] end generated region [Generated System Includes]

// [[[ begin generated region: do not modify [Generated User Includes]
#include "symbian_ua_guiSettingItemList.h"
#include "Symbian_ua_guiSettingItemListSettings.h"
#include "symbian_ua_guiSettingItemList.hrh"
#include "symbian_ua_gui.hrh"
#include "symbian_ua_guiSettingItemListView.h"
// ]]] end generated region [Generated User Includes]


#include <s32stor.h>
#include <s32file.h>


// [[[ begin generated region: do not modify [Generated Constants]
// ]]] end generated region [Generated Constants]


_LIT(KtxDicFileName			,"settings.ini" );
 
const TInt KRegistrar		= 2;
const TInt KUsername		= 3;
const TInt KPassword		= 4;
const TInt KStunServer		= 5;
const TInt KSrtp			= 6;
const TInt KIce				= 7;

/**
 * Construct the CSymbian_ua_guiSettingItemList instance
 * @param aCommandObserver command observer
 */ 
CSymbian_ua_guiSettingItemList::CSymbian_ua_guiSettingItemList( 
		TSymbian_ua_guiSettingItemListSettings& aSettings, 
		MEikCommandObserver* aCommandObserver )
	: iSettings( aSettings ), iCommandObserver( aCommandObserver )
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}
/** 
 * Destroy any instance variables
 */
CSymbian_ua_guiSettingItemList::~CSymbian_ua_guiSettingItemList()
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}

/**
 * Handle system notification that the container's size has changed.
 */
void CSymbian_ua_guiSettingItemList::SizeChanged()
	{
	if ( ListBox() ) 
		{
		ListBox()->SetRect( Rect() );
		}
	}

/**
 * Create one setting item at a time, identified by id.
 * CAknSettingItemList calls this method and takes ownership
 * of the returned value.  The CAknSettingItem object owns
 * a reference to the underlying data, which EditItemL() uses
 * to edit and store the value.
 */
CAknSettingItem* CSymbian_ua_guiSettingItemList::CreateSettingItemL( TInt aId )
	{
	switch ( aId )
		{
	// [[[ begin generated region: do not modify [Initializers]
		case ESymbian_ua_guiSettingItemListViewEd_registrar:
			{			
			CAknTextSettingItem* item = new ( ELeave ) 
				CAknTextSettingItem( 
					aId,
					iSettings.Ed_registrar() );
			item->SetSettingPageFlags(CAknTextSettingPage::EZeroLengthAllowed);
			return item;
			}
		case ESymbian_ua_guiSettingItemListViewEd_user:
			{			
			CAknTextSettingItem* item = new ( ELeave ) 
				CAknTextSettingItem( 
					aId,
					iSettings.Ed_user() );
			item->SetSettingPageFlags(CAknTextSettingPage::EZeroLengthAllowed);
			return item;
			}
		case ESymbian_ua_guiSettingItemListViewEd_password:
			{			
			CAknPasswordSettingItem* item = new ( ELeave ) 
				CAknPasswordSettingItem( 
					aId,
					CAknPasswordSettingItem::EAlpha,
					iSettings.Ed_password() );
			item->SetSettingPageFlags(CAknTextSettingPage::EZeroLengthAllowed);
			return item;
			}
		case ESymbian_ua_guiSettingItemListViewB_srtp:
			{			
			CAknBinaryPopupSettingItem* item = new ( ELeave ) 
				CAknBinaryPopupSettingItem( 
					aId,
					iSettings.B_srtp() );
			item->SetHidden( ETrue ); 
			return item;
			}
		case ESymbian_ua_guiSettingItemListViewB_ice:
			{			
			CAknBinaryPopupSettingItem* item = new ( ELeave ) 
				CAknBinaryPopupSettingItem( 
					aId,
					iSettings.B_ice() );
			item->SetHidden( ETrue ); 
			return item;
			}
		case ESymbian_ua_guiSettingItemListViewEd_stun_server:
			{			
			CAknTextSettingItem* item = new ( ELeave ) 
				CAknTextSettingItem( 
					aId,
					iSettings.Ed_stun_server() );
			item->SetHidden( ETrue ); 
			return item;
			}
	// ]]] end generated region [Initializers]
	
		}
		
	return NULL;
	}
	
/**
 * Edit the setting item identified by the given id and store
 * the changes into the store.
 * @param aIndex the index of the setting item in SettingItemArray()
 * @param aCalledFromMenu true: a menu item invoked editing, thus
 *	always show the edit page and interactively edit the item;
 *	false: change the item in place if possible, else show the edit page
 */
void CSymbian_ua_guiSettingItemList::EditItemL ( TInt aIndex, TBool aCalledFromMenu )
	{
	CAknSettingItem* item = ( *SettingItemArray() )[aIndex];
	switch ( item->Identifier() )
		{
	// [[[ begin generated region: do not modify [Editing Started Invoker]
	// ]]] end generated region [Editing Started Invoker]
	
		}
	
	CAknSettingItemList::EditItemL( aIndex, aCalledFromMenu );
	
	TBool storeValue = ETrue;
	switch ( item->Identifier() )
		{
	// [[[ begin generated region: do not modify [Editing Stopped Invoker]
	// ]]] end generated region [Editing Stopped Invoker]
	
		}
		
	if ( storeValue )
		{
		item->StoreL();
		SaveSettingValuesL();
		}	
	}
/**
 *	Handle the "Change" option on the Options menu.  This is an
 *	alternative to the Selection key that forces the settings page
 *	to come up rather than changing the value in place (if possible).
 */
void CSymbian_ua_guiSettingItemList::ChangeSelectedItemL()
	{
	if ( ListBox()->CurrentItemIndex() >= 0 )
		{
		EditItemL( ListBox()->CurrentItemIndex(), ETrue );
		}
	}

/**
 *	Load the initial contents of the setting items.  By default,
 *	the setting items are populated with the default values from
 * 	the design.  You can override those values here.
 *	<p>
 *	Note: this call alone does not update the UI.  
 *	LoadSettingsL() must be called afterwards.
 */
void CSymbian_ua_guiSettingItemList::LoadSettingValuesL()
	{
	// load values into iSettings

	TFileName path;
	TFileName pathWithoutDrive;
	CEikonEnv::Static()->FsSession().PrivatePath( pathWithoutDrive );

	// Extract drive letter into appDrive:
#ifdef __WINS__
	path.Copy( _L("c:") );
#else
	RProcess process;
	path.Copy( process.FileName().Left(2) );
#endif

	path.Append( pathWithoutDrive );
	path.Append( KtxDicFileName );
	
	TFindFile AufFolder(CCoeEnv::Static()->FsSession());
	if(KErrNone == AufFolder.FindByDir(path, KNullDesC))
	{
		CDictionaryFileStore* MyDStore = CDictionaryFileStore::OpenLC(CCoeEnv::Static()->FsSession(),AufFolder.File(), TUid::Uid(1));
		TUid FileUid;
		
		FileUid.iUid = KRegistrar;
		if(MyDStore->IsPresentL(FileUid))
		{
			RDictionaryReadStream in;
			in.OpenLC(*MyDStore,FileUid);
			in >> iSettings.Ed_registrar();
			CleanupStack::PopAndDestroy(1);// in
		}
			
		FileUid.iUid = KUsername;
		if(MyDStore->IsPresentL(FileUid))
		{
			RDictionaryReadStream in;
			in.OpenLC(*MyDStore,FileUid);
			in >> iSettings.Ed_user();
			CleanupStack::PopAndDestroy(1);// in
		}

		FileUid.iUid = KPassword;
		if(MyDStore->IsPresentL(FileUid))
		{
			RDictionaryReadStream in;
			in.OpenLC(*MyDStore,FileUid);
			in >> iSettings.Ed_password();
			CleanupStack::PopAndDestroy(1);// in
		}

		FileUid.iUid = KStunServer;
		if(MyDStore->IsPresentL(FileUid))
		{
			RDictionaryReadStream in;
			in.OpenLC(*MyDStore,FileUid);
			in >> iSettings.Ed_stun_server();
			CleanupStack::PopAndDestroy(1);// in
		}

		FileUid.iUid = KSrtp;
		if(MyDStore->IsPresentL(FileUid))
		{
			RDictionaryReadStream in;
			in.OpenLC(*MyDStore,FileUid);
			iSettings.SetB_srtp((TBool)in.ReadInt32L());
			CleanupStack::PopAndDestroy(1);// in
		}
		
		FileUid.iUid = KIce;
		if(MyDStore->IsPresentL(FileUid))
		{
			RDictionaryReadStream in;
			in.OpenLC(*MyDStore,FileUid);
			iSettings.SetB_ice((TBool)in.ReadInt32L());
			CleanupStack::PopAndDestroy(1);// in
		}

		CleanupStack::PopAndDestroy(1);// Store		
	}

	}
	
/**
 *	Save the contents of the setting items.  Note, this is called
 *	whenever an item is changed and stored to the model, so it
 *	may be called multiple times or not at all.
 */
void CSymbian_ua_guiSettingItemList::SaveSettingValuesL()
	{
	// store values from iSettings

	TFileName path;
	TFileName pathWithoutDrive;
	CEikonEnv::Static()->FsSession().PrivatePath( pathWithoutDrive );

	// Extract drive letter into appDrive:
#ifdef __WINS__
	path.Copy( _L("c:") );
#else
	RProcess process;
	path.Copy( process.FileName().Left(2) );
	
	if(path.Compare(_L("c")) || path.Compare(_L("C")))
		CEikonEnv::Static()->FsSession().CreatePrivatePath(EDriveC);
	else if(path.Compare(_L("e")) || path.Compare(_L("E")))
		CEikonEnv::Static()->FsSession().CreatePrivatePath(EDriveE);	
#endif

	path.Append( pathWithoutDrive );
	path.Append( KtxDicFileName );
	
	TFindFile AufFolder(CCoeEnv::Static()->FsSession());
	if(KErrNone == AufFolder.FindByDir(path, KNullDesC))
	{
		User::LeaveIfError(CCoeEnv::Static()->FsSession().Delete(AufFolder.File()));
	}
 
	CDictionaryFileStore* MyDStore = CDictionaryFileStore::OpenLC(CCoeEnv::Static()->FsSession(),path, TUid::Uid(1));
 
	TUid FileUid = {0x0};
		
	FileUid.iUid = KRegistrar;
	RDictionaryWriteStream out1;
	out1.AssignLC(*MyDStore,FileUid);
	out1 << iSettings.Ed_registrar();
	out1.CommitL(); 	
	CleanupStack::PopAndDestroy(1);// out2	
	
	FileUid.iUid = KUsername;
	RDictionaryWriteStream out2;
	out2.AssignLC(*MyDStore,FileUid);
	out2 << iSettings.Ed_user();
	out2.CommitL(); 	
	CleanupStack::PopAndDestroy(1);// out2	
	
	FileUid.iUid = KPassword;
	RDictionaryWriteStream out3;
	out3.AssignLC(*MyDStore,FileUid);
	out3 << iSettings.Ed_password();
	out3.CommitL(); 	
	CleanupStack::PopAndDestroy(1);// out2	
	
	FileUid.iUid = KStunServer;
	RDictionaryWriteStream out4;
	out4.AssignLC(*MyDStore,FileUid);
	out4 << iSettings.Ed_stun_server();
	out4.CommitL(); 	
	CleanupStack::PopAndDestroy(1);// out2	
	
	FileUid.iUid = KSrtp;
	RDictionaryWriteStream out5;
	out5.AssignLC(*MyDStore,FileUid);
	out5.WriteInt32L(iSettings.B_srtp());
	out5.CommitL(); 	
	CleanupStack::PopAndDestroy(1);// out1
	
	FileUid.iUid = KIce;
	RDictionaryWriteStream out6;
	out6.AssignLC(*MyDStore,FileUid);
	out6.WriteInt32L(iSettings.B_ice());
	out6.CommitL(); 	
	CleanupStack::PopAndDestroy(1);// out1
	 
	MyDStore->CommitL();
	CleanupStack::PopAndDestroy(1);// Store

	}


/** 
 * Handle global resource changes, such as scalable UI or skin events (override)
 */
void CSymbian_ua_guiSettingItemList::HandleResourceChange( TInt aType )
	{
	CAknSettingItemList::HandleResourceChange( aType );
	SetRect( iAvkonViewAppUi->View( TUid::Uid( ESymbian_ua_guiSettingItemListViewId ) )->ClientRect() );
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	}
				
/** 
 * Handle key event (override)
 * @param aKeyEvent key event
 * @param aType event code
 * @return EKeyWasConsumed if the event was handled, else EKeyWasNotConsumed
 */
TKeyResponse CSymbian_ua_guiSettingItemList::OfferKeyEventL( 
		const TKeyEvent& aKeyEvent, 
		TEventCode aType )
	{
	// [[[ begin generated region: do not modify [Generated Contents]
	// ]]] end generated region [Generated Contents]
	
	if ( aKeyEvent.iCode == EKeyLeftArrow 
		|| aKeyEvent.iCode == EKeyRightArrow )
		{
		// allow the tab control to get the arrow keys
		return EKeyWasNotConsumed;
		}
	
	return CAknSettingItemList::OfferKeyEventL( aKeyEvent, aType );
	}
				
