#include "SDynamicListPanel.h"
#include "Layout/ArrangedChildren.h"


// Used to subtract a tiny amount from the available dimension to avoid floating point precision problems when arranging children
static const float FloatingPointPrecisionOffset = 0.001f;

SDynamicListPanel::SDynamicListPanel()
	: Children(this)
{
}

void SDynamicListPanel::Construct( const FArguments& InArgs )
{
	NumDesiredItems = InArgs._NumDesiredItems;
	ItemAlignment = InArgs._ItemAlignment;
	Orientation = InArgs._ListOrientation;
	Children.AddSlots(MoveTemp(const_cast<TArray<FSlot::FSlotArguments>&>(InArgs._Slots)));
}

SDynamicListPanel::FSlot::FSlotArguments SDynamicListPanel::Slot()
{
	return FSlot::FSlotArguments(MakeUnique<FSlot>());
}
	
SDynamicListPanel::FScopedWidgetSlotArguments SDynamicListPanel::AddSlot(int32 InsertAtIndex)
{
	return FScopedWidgetSlotArguments{ MakeUnique<FSlot>(), Children, InsertAtIndex };
}

void SDynamicListPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if (Children.Num() > 0)
	{
		const FTableViewDimensions AllottedDimensions(Orientation, AllottedGeometry.GetLocalSize());
		FTableViewDimensions DimensionsSoFar(Orientation);

		// This is a normal list, arrange items in a line along the scroll axis
		float ScrollAxisOffset = 0.0f;
		const int32 NumWholeWidgetsOffset = FMath::Min(FMath::Floor(FirstLineScrollOffset), Children.Num());
		for (int32 ItemIndex = 0; ItemIndex < NumWholeWidgetsOffset; ++ItemIndex)
		{
			const FTableViewDimensions CurrentChildDimensions(Orientation, Children[ItemIndex].GetWidget()->GetDesiredSize());
			ScrollAxisOffset += CurrentChildDimensions.ScrollAxis;
		}

		const int32 FractionalWidgetIndex = NumWholeWidgetsOffset;
		if (Children.IsValidIndex(FractionalWidgetIndex))
		{
			const FTableViewDimensions OffsetFractionChildDimensions(Orientation, Children[FractionalWidgetIndex].GetWidget()->GetDesiredSize());
			ScrollAxisOffset += FMath::Frac(FirstLineScrollOffset) * OffsetFractionChildDimensions.ScrollAxis;
		}

		DimensionsSoFar.ScrollAxis = -FMath::FloorToInt(ScrollAxisOffset) - OverscrollAmount;

		for (int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex)
		{
			const FTableViewDimensions WidgetDesiredDimensions(Orientation, Children[ItemIndex].GetWidget()->GetDesiredSize());
			const bool bIsVisible = Children[ItemIndex].GetWidget()->GetVisibility().IsVisible();
				
			FTableViewDimensions FinalWidgetDimensions(Orientation);
			FinalWidgetDimensions.ScrollAxis = bIsVisible ? WidgetDesiredDimensions.ScrollAxis : 0.f;
			FinalWidgetDimensions.LineAxis = AllottedDimensions.LineAxis;

			ArrangedChildren.AddWidget(
				AllottedGeometry.MakeChild(Children[ItemIndex].GetWidget(), DimensionsSoFar.ToVector2D(), FinalWidgetDimensions.ToVector2D())
			);

			DimensionsSoFar.ScrollAxis += FinalWidgetDimensions.ScrollAxis;
		}
	}
}
	
void SDynamicListPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
}

FVector2D SDynamicListPanel::ComputeDesiredSize( float ) const
{
	FTableViewDimensions DesiredListPanelDimensions(Orientation);
	
	// Simply the sum of all the children along the scroll axis and the largest width along the line axis.
	for (int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex)
	{
		// Notice that we do not respect child Visibility - it's not useful for ListPanels.
		FTableViewDimensions ChildDimensions(Orientation, Children[ItemIndex].GetWidget()->GetDesiredSize());
		DesiredListPanelDimensions.ScrollAxis += ChildDimensions.ScrollAxis;
		DesiredListPanelDimensions.LineAxis = FMath::Max(DesiredListPanelDimensions.LineAxis, ChildDimensions.LineAxis);
	}
	
	return DesiredListPanelDimensions.ToVector2D();
}

FChildren* SDynamicListPanel::GetAllChildren()
{
	return &Children;
}

FChildren* SDynamicListPanel::GetChildren()
{
	if (bIsRefreshPending)
	{
		// When a refresh is pending it is unsafe to cache the desired sizes of our children because
		// they may be representing unsound data structures. Any delegates/attributes accessing unsound
		// data will cause a crash.
		return &FNoChildren::NoChildrenInstance;
	}
	else
	{
		return &Children;
	}
	
}

void SDynamicListPanel::SetFirstLineScrollOffset(float InFirstLineScrollOffset)
{
	FirstLineScrollOffset = InFirstLineScrollOffset;
}

void SDynamicListPanel::SetOverscrollAmount( float InOverscrollAmount )
{
	OverscrollAmount = InOverscrollAmount;
}

void SDynamicListPanel::ClearItems()
{
	Children.Empty();
}

void SDynamicListPanel::SetRefreshPending( bool IsPendingRefresh )
{
	bIsRefreshPending = IsPendingRefresh;
}

bool SDynamicListPanel::IsRefreshPending() const
{
	return bIsRefreshPending;
}
