#include "DynamicListView.h"
#include "Components/ListView.h"
#include "Widgets/Views/SListView.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Styling/UMGCoreStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DynamicListView)

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UListView

static FTableViewStyle* DefaultListViewStyle = nullptr;
static FScrollBarStyle* DefaultListViewScrollBarStyle = nullptr;

#if WITH_EDITOR
static FTableViewStyle* EditorListViewStyle = nullptr;
static FScrollBarStyle* EditorListViewScrollBarStyle = nullptr;
#endif 

UDynamicListView::UDynamicListView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, Orientation(EOrientation::Orient_Vertical)
{
	if (DefaultListViewStyle == nullptr)
	{
		DefaultListViewStyle = new FTableViewStyle(FUMGCoreStyle::Get().GetWidgetStyle<FTableViewStyle>("ListView"));

		// Unlink UMG default colors.
		DefaultListViewStyle->UnlinkColors();
	}

	if (DefaultListViewScrollBarStyle == nullptr)
	{
		DefaultListViewScrollBarStyle = new FScrollBarStyle(FUMGCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar"));

		// Unlink UMG default colors.
		DefaultListViewScrollBarStyle->UnlinkColors();
	}

	WidgetStyle = *DefaultListViewStyle;
	ScrollBarStyle = *DefaultListViewScrollBarStyle;

#if WITH_EDITOR 
	if (EditorListViewStyle == nullptr)
	{
		EditorListViewStyle = new FTableViewStyle(FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("ListView"));

		// Unlink UMG default colors.
		EditorListViewStyle->UnlinkColors();
	}

	if (EditorListViewScrollBarStyle == nullptr)
	{
		EditorListViewScrollBarStyle = new FScrollBarStyle(FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar"));

		// Unlink UMG default colors.
		EditorListViewScrollBarStyle->UnlinkColors();
	}

	if (IsEditorWidget())
	{
		WidgetStyle = *EditorListViewStyle;
		ScrollBarStyle = *EditorListViewScrollBarStyle;
	}
#endif // WITH_EDITOR
}

void UDynamicListView::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyListView.Reset();
}

#if WITH_EDITOR
void UDynamicListView::OnRefreshDesignerItems()
{
	RefreshDesignerItems<UObject*>(ListItems, [this] () {return NewObject<UDynamicListViewDesignerPreviewItem>(this); });
}
#endif

void UDynamicListView::AddItem(UObject* Item)
{
	if (Item == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot add null item into ListView."), ELogVerbosity::Warning, "NullListViewItem");
		return;
	}

	if (ListItems.Contains(Item))
	{
		FFrame::KismetExecutionMessage(TEXT("Cannot add duplicate item into ListView."), ELogVerbosity::Warning, "DuplicateListViewItem");
		return;
	}

	ListItems.Add(Item);

	const TArray<UObject*> Added = { Item };
	const TArray<UObject*> Removed;
	OnItemsChanged(Added, Removed);

	RequestRefresh();
}

void UDynamicListView::RemoveItem(UObject* Item)
{
	ListItems.Remove(Item);

	const TArray<UObject*> Added;
	const TArray<UObject*> Removed = { Item };
	OnItemsChanged(Added, Removed);

	RequestRefresh();
}

UObject* UDynamicListView::GetItemAt(int32 Index) const
{
	return ListItems.IsValidIndex(Index) ? ListItems[Index] : nullptr;
}

int32 UDynamicListView::GetNumItems() const
{
	return ListItems.Num();
}

int32 UDynamicListView::GetIndexForItem(const UObject* Item) const
{
	return ListItems.IndexOfByKey(Item);
}

void UDynamicListView::ClearListItems()
{
	const TArray<UObject*> Added;
	const TArray<UObject*> Removed = MoveTemp(ListItems);

	ListItems.Reset();

	OnItemsChanged(Added, Removed);

	RequestRefresh();
}

void UDynamicListView::SetSelectionMode(TEnumAsByte<ESelectionMode::Type> InSelectionMode)
{
	SelectionMode = InSelectionMode;
	if (MyListView)
	{
		MyListView->SetSelectionMode(InSelectionMode);
	}
}

int32 UDynamicListView::BP_GetNumItemsSelected() const
{
	return GetNumItemsSelected();
}

void UDynamicListView::BP_SetListItems(const TArray<UObject*>& InListItems)
{
	SetListItems(InListItems);
}

UObject* UDynamicListView::BP_GetSelectedItem() const
{
	return GetSelectedItem();
}

void UDynamicListView::HandleOnEntryInitializedInternal(UObject* Item, const TSharedRef<ITableRow>& TableRow)
{
	BP_OnEntryInitialized.Broadcast(Item, GetEntryWidgetFromItem(Item));
}

bool UDynamicListView::BP_GetSelectedItems(TArray<UObject*>& Items) const
{
	return GetSelectedItems(Items) > 0;
}

bool UDynamicListView::BP_IsItemVisible(UObject* Item) const
{
	return IsItemVisible(Item);
}

void UDynamicListView::BP_NavigateToItem(UObject* Item)
{
	if (Item)
	{
		RequestNavigateToItem(Item);
	}
}

void UDynamicListView::NavigateToIndex(int32 Index)
{
	RequestNavigateToItem(GetItemAt(Index));
}

void UDynamicListView::BP_ScrollItemIntoView(UObject* Item)
{
	if (Item)
	{
		RequestScrollItemIntoView(Item);
	}
}

void UDynamicListView::ScrollIndexIntoView(int32 Index)
{
	BP_ScrollItemIntoView(GetItemAt(Index));
}

void UDynamicListView::BP_CancelScrollIntoView()
{
	if (MyListView.IsValid())
	{
		MyListView->CancelScrollIntoView();
	}
}

bool UDynamicListView::IsRefreshPending() const
{
	if (MyListView.IsValid())
	{
		return MyListView->IsPendingRefresh();
	}
	return false;
}

void UDynamicListView::BP_SetSelectedItem(UObject* Item)
{
	if (MyListView.IsValid())
	{
		MyListView->SetSelection(Item, ESelectInfo::Direct);
	}
}

void UDynamicListView::SetSelectedItem(const UObject* Item)
{
	ITypedUMGDynamicListView<UObject*>::SetSelectedItem(const_cast<UObject*>(Item));
}

void UDynamicListView::SetSelectedIndex(int32 Index)
{
	SetSelectedItem(GetItemAt(Index));
}

void UDynamicListView::BP_SetItemSelection(UObject* Item, bool bSelected)
{
	SetItemSelection(Item, bSelected);
}

void UDynamicListView::BP_ClearSelection()
{
	ClearSelection();
}

void UDynamicListView::OnItemsChanged(const TArray<UObject*>& AddedItems, const TArray<UObject*>& RemovedItems)
{
	// Allow subclasses to do special things when objects are added or removed from the list.

	// Keep track of references to Actors and make sure to release them when Actors are about to be removed
	for (UObject* AddedItem : AddedItems)
	{
		if (AActor* AddedActor = Cast<AActor>(AddedItem))
		{
			AddedActor->OnEndPlay.AddDynamic(this, &UDynamicListView::OnListItemEndPlayed);
		}
		else if (AActor* AddedItemOuterActor = AddedItem->GetTypedOuter<AActor>())
		{
			// Unique so that we don't spam events for shared actor outers but this also means we can't
			// unsubscribe when processing RemovedItems
			AddedItemOuterActor->OnEndPlay.AddUniqueDynamic(this, &UDynamicListView::OnListItemOuterEndPlayed);
		}
	}
	for (UObject* RemovedItem : RemovedItems)
	{
		if (AActor* RemovedActor = Cast<AActor>(RemovedItem))
		{
			RemovedActor->OnEndPlay.RemoveDynamic(this, &UDynamicListView::OnListItemEndPlayed);
		}
	}
}

void UDynamicListView::OnListItemEndPlayed(AActor* Item, EEndPlayReason::Type EndPlayReason)
{
	RemoveItem(Item);
}

void UDynamicListView::OnListItemOuterEndPlayed(AActor* ItemOuter, EEndPlayReason::Type EndPlayReason)
{
	for (int32 ItemIndex = ListItems.Num() - 1; ItemIndex >= 0; --ItemIndex)
	{
		UObject* Item = ListItems[ItemIndex];
		if (Item->IsIn(ItemOuter))
		{
			RemoveItem(Item);
		}
	}
}

TSharedRef<SDynamicTableViewBase> UDynamicListView::RebuildListWidget()
{
	return ConstructListView<SDynamicListView>();
}

void UDynamicListView::HandleListEntryHovered(UUserWidget& EntryWidget)
{
	if (UObject* const* ListItem = ItemFromEntryWidget(EntryWidget))
	{
		OnItemIsHoveredChanged().Broadcast(*ListItem, true);
		BP_OnItemIsHoveredChanged.Broadcast(*ListItem, true);
	}
}

void UDynamicListView::HandleListEntryUnhovered(UUserWidget& EntryWidget)
{
	if (UObject* const* ListItem = ItemFromEntryWidget(EntryWidget))
	{
		OnItemIsHoveredChanged().Broadcast(*ListItem, false);
		BP_OnItemIsHoveredChanged.Broadcast(*ListItem, false);
	}
}

FMargin UDynamicListView::GetDesiredEntryPadding(UObject* Item) const
{
	if (ListItems.Num() > 0 && ListItems[0] != Item)
	{
		if (Orientation == EOrientation::Orient_Horizontal)
		{
			// For all entries after the first one, add the spacing as left padding
			return FMargin(EntrySpacing, 0.f, 0.0f, 0.f);
		}
		else
		{
			// For all entries after the first one, add the spacing as top padding
			return FMargin(0.f, EntrySpacing, 0.f, 0.f);
		}
	}

	return FMargin(0.f);
}

UUserWidget& UDynamicListView::OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<SDynamicTableViewBase>& OwnerTable)
{
	return GenerateTypedEntry(DesiredEntryClass, OwnerTable);
}

void UDynamicListView::OnItemClickedInternal(UObject* ListItem)
{
	ITypedUMGDynamicListView::OnItemClickedInternal(ListItem);
	BP_OnItemClicked.Broadcast(ListItem);
}

void UDynamicListView::OnItemDoubleClickedInternal(UObject* ListItem)
{
	ITypedUMGDynamicListView::OnItemDoubleClickedInternal(ListItem);
	BP_OnItemDoubleClicked.Broadcast(ListItem);
}

void UDynamicListView::OnSelectionChangedInternal(UObject* FirstSelectedItem)
{
	ITypedUMGDynamicListView::OnSelectionChangedInternal(FirstSelectedItem);
	BP_OnItemSelectionChanged.Broadcast(FirstSelectedItem, FirstSelectedItem != nullptr);
}

void UDynamicListView::OnItemScrolledIntoViewInternal(UObject* ListItem, UUserWidget& EntryWidget)
{
	ITypedUMGDynamicListView::OnItemScrolledIntoViewInternal(ListItem, EntryWidget);
	BP_OnItemScrolledIntoView.Broadcast(ListItem, &EntryWidget);
}

void UDynamicListView::OnListViewScrolledInternal(float ItemOffset, float DistanceRemaining)
{
	ITypedUMGDynamicListView::OnListViewScrolledInternal(ItemOffset, DistanceRemaining);
	BP_OnListViewScrolled.Broadcast(ItemOffset, DistanceRemaining);
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
