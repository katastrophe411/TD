#pragma once

#include "CoreMinimal.h"

class SDynamicListPanel : public SPanel
{
public:
	/** A ListPanel slot is very simple - it just stores a widget. */
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		FSlot()
		: TSlotBase<FSlot>()
		{}
	};

	/** Make a new ListPanel::Slot  */
	static FSlot::FSlotArguments Slot();
	
	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;
	/** Add a slot to the ListPanel */
	FScopedWidgetSlotArguments AddSlot(int32 InsertAtIndex = INDEX_NONE);

	SLATE_BEGIN_ARGS( SDynamicListPanel )
		: _NumDesiredItems(0)
		, _ItemAlignment(EListItemAlignment::EvenlyDistributed)
		, _ListOrientation(Orient_Vertical)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
			_Clipping = EWidgetClipping::ClipToBounds;
		}
		
		SLATE_SLOT_ARGUMENT(FSlot, Slots)
		SLATE_ATTRIBUTE( int32, NumDesiredItems )
		SLATE_ATTRIBUTE( EListItemAlignment, ItemAlignment )
		SLATE_ARGUMENT( EOrientation, ListOrientation )

	SLATE_END_ARGS()

	SDynamicListPanel();

	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs );

public:

	// SWidget interface
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetAllChildren() override;
	virtual FChildren* GetChildren() override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface

	/** Fraction of the first line that we should offset by to account for the current scroll amount. */
	void SetFirstLineScrollOffset(float InFirstLineScrollOffset);

	/** Set how much we should appear to have scrolled past the beginning/end of the list. */
	void SetOverscrollAmount( float InOverscrollAmount );
	
	/** Remove all the children from this panel */
	void ClearItems();

	/** Tells the list panel whether items in the list are pending a refresh */
	void SetRefreshPending( bool IsPendingRefresh );

	/** Returns true if this list panel is pending a refresh, false otherwise */
	bool IsRefreshPending() const;
	
protected:

	/** The children being arranged by this panel */
	TPanelChildren<FSlot> Children;
	
	/** Total number of items that the tree wants to visualize */
	TAttribute<int32> NumDesiredItems;
	
	/**
	 * The offset of the view area from the top of the list in item heights.
	 * Translate to physical units based on first line in list.
	 */
	float FirstLineScrollOffset = 0.f;

	/** Amount scrolled past beginning/end of list in Slate Units. */
	float OverscrollAmount = 0.f;

	/**
	 * When true, a refresh of the table view control that is using this panel is pending.
	 * Some of the widgets in this panel are associated with items that may no longer be sound data.
	 */
	bool bIsRefreshPending = false;

	/** How should be horizontally aligned? Only relevant for tile views. */
	TAttribute<EListItemAlignment> ItemAlignment;

	/** Overall orientation of the list for layout and scrolling. Only relevant for tile views. */
	EOrientation Orientation;
};
