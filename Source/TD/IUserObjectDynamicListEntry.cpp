#include "IUserObjectDynamicListEntry.h"

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "DynamicListView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IUserObjectDynamicListEntry)

TMap<TWeakObjectPtr<const UUserWidget>, TWeakPtr<const IObjectDynamicTableRow>> IObjectDynamicTableRow::ObjectRowsByUserWidget;

UUserObjectDynamicListEntry::UUserObjectDynamicListEntry(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void IUserObjectDynamicListEntry::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	Execute_OnListItemObjectSet(Cast<UObject>(this), ListItemObject);
}

UObject* IUserObjectDynamicListEntry::GetListItemObjectInternal() const
{
	return UUserObjectListEntryLibrary::GetListItemObject(Cast<UUserWidget>(const_cast<IUserObjectDynamicListEntry*>(this)));
}

void IUserObjectDynamicListEntry::SetListItemObject(UUserWidget& ListEntryWidget, UObject* ListItemObject)
{
	if (IUserObjectDynamicListEntry* NativeImplementation = Cast<IUserObjectDynamicListEntry>(&ListEntryWidget))
	{
		NativeImplementation->NativeOnListItemObjectSet(ListItemObject);
	}
	else if (ListEntryWidget.Implements<UUserObjectDynamicListEntry>())
	{
		Execute_OnListItemObjectSet(&ListEntryWidget, ListItemObject);
	}
}

UObject* UUserObjectDynamicListEntryLibrary::GetListItemObject(TScriptInterface<IUserObjectDynamicListEntry> UserObjectListEntry)
{
	if (UUserWidget* EntryWidget = Cast<UUserWidget>(UserObjectListEntry.GetObject()))
	{
		const UListView* OwningListView = Cast<UListView>(UUserListEntryLibrary::GetOwningListView(EntryWidget));
		if (UObject* const* ListItem = OwningListView ? OwningListView->ItemFromEntryWidget(*EntryWidget) : nullptr)
		{
			return *ListItem;
		}
	}
	return nullptr;
}

