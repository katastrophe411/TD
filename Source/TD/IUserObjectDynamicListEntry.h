#pragma once

#include "Blueprint/IUserListEntry.h"
#include "IUserObjectDynamicListEntry.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UUserObjectDynamicListEntry : public UUserListEntry
{
	GENERATED_UINTERFACE_BODY()
};

/**
 * 
 */
class IUserObjectDynamicListEntry : public IUserListEntry
{
	GENERATED_IINTERFACE_BODY()

public:
	/** Returns the UObject item in the owning UListView that this entry currently represents */
	template <typename ItemObjectT = UObject>
	ItemObjectT* GetListItem() const
	{
		static_assert(TIsDerivedFrom<ItemObjectT, UObject>::IsDerived, "Items represented by an ObjectListEntry are always expected to be UObjects.");
		return Cast<ItemObjectT>(GetListItemObjectInternal());
	}

protected:
	/** Follows the same pattern as the NativeOn[X] methods in UUserWidget - super calls are expected in order to route the event to BP. */
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject);
	
	/** Called when this entry is assigned a new item object to represent by the owning list view */
	UFUNCTION(BlueprintImplementableEvent, Category = ObjectListEntry)
	void OnListItemObjectSet(UObject* ListItemObject);

private:
	UObject* GetListItemObjectInternal() const;
	
	template <typename> friend class SObjectDynamicTableRow;
	static void SetListItemObject(UUserWidget& ListEntryWidget, UObject* ListItemObject);
};

/** Static library to supply "for free" functionality to widgets that implement IUserListEntry */
UCLASS()
class UUserObjectDynamicListEntryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	 * Returns the item in the owning list view that this entry is currently assigned to represent. 
	 * @param UserObjectListEntry Note: Visually not transmitted, but this defaults to "self". No need to hook up if calling internally.
	 */
	UFUNCTION(BlueprintPure, Category = UserObjectListEntry, meta = (DefaultToSelf = UserObjectListEntry))
	static UObject* GetListItemObject(TScriptInterface<IUserObjectDynamicListEntry> UserObjectListEntry);
};