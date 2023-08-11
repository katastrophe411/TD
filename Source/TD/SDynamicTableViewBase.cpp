﻿#include "SDynamicTableViewBase.h"
#include "Widgets/Views/STableViewBase.h"
#include "Rendering/DrawElements.h"
#include "Types/SlateConstants.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/Overscroll.h"
#include "Widgets/Views/STableRow.h"
#include "SDynamicListPanel.h"

// #include UE_INLINE_GENERATED_CPP_BY_NAME(SDynamicTableViewBase)

namespace ListConstants
{
	static const float OvershootMax = 150.0f;
	static const float OvershootBounceRate = 250.0f;
}

//
// FTableViewDimensions::FTableViewDimensions(EOrientation InOrientation)
// 	: Orientation(InOrientation)
// {
// }
//
//
// FTableViewDimensions::FTableViewDimensions(EOrientation InOrientation, float X, float Y)
// 	: FTableViewDimensions(InOrientation, FVector2f(X, Y))
// {
// }
//
// FTableViewDimensions::FTableViewDimensions(EOrientation InOrientation, const UE::Slate::FDeprecateVector2DParameter& Size)
// 	: FTableViewDimensions(InOrientation)
// {
// 	if (InOrientation == Orient_Vertical)
// 	{
// 		LineAxis = Size.X;
// 		ScrollAxis = Size.Y;
// 	}
// 	else
// 	{
// 		ScrollAxis = Size.X;
// 		LineAxis = Size.Y;
// 	}
// }

void SDynamicTableViewBase::ConstructChildren( const TAttribute<float>& InItemWidth, const TAttribute<float>& InItemHeight, const TAttribute<EListItemAlignment>& InItemAlignment, const TSharedPtr<SHeaderRow>& InHeaderRow, const TSharedPtr<SScrollBar>& InScrollBar, EOrientation InScrollOrientation, const FOnTableViewScrolled& InOnTableViewScrolled, const FScrollBarStyle* InScrollBarStyle, const bool bInPreventThrottling )
{
	bItemsNeedRefresh = true;
	
	HeaderRow = InHeaderRow;

	OnTableViewScrolled = InOnTableViewScrolled;

	Orientation = InHeaderRow ? Orient_Vertical : InScrollOrientation;
	UE_CLOG(InScrollOrientation != Orientation, LogSlate, Error, TEXT("STableViewBase does not support horizontal scrolling when displaying a header row"));

	ItemsPanel = SNew(SDynamicListPanel)
		.Clipping(GetClipping())
		.NumDesiredItems(this, &SDynamicTableViewBase::GetNumItemsBeingObserved)
		.ItemAlignment(InItemAlignment)
		.ListOrientation(Orientation);

	PinnedItemsPanel = SNew(SDynamicListPanel)
		.Clipping(GetClipping())
		.NumDesiredItems(this, &SDynamicTableViewBase::GetNumPinnedItems)
		.ItemAlignment(InItemAlignment)
		.ListOrientation(Orientation)
		.Visibility(this, &SDynamicTableViewBase::GetPinnedItemsVisiblity);

	TSharedPtr<SWidget> ListAndScrollbar;
	if (InScrollBar)
	{
		// The user provided us with a scrollbar; we will rely on it.
		ScrollBar = InScrollBar;
		ScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SDynamicTableViewBase::ScrollBar_OnUserScrolled));
		
		ListAndScrollbar = ItemsPanel;
	}
	else
	{
		ScrollBar = SNew(SScrollBar)
			.OnUserScrolled(this, &SDynamicTableViewBase::ScrollBar_OnUserScrolled)
			.Orientation(Orientation)
			.Style(InScrollBarStyle ? InScrollBarStyle : &FAppStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar"))
			.PreventThrottling(bInPreventThrottling);

		const FOptionalSize ScrollBarSize(16.f);

		if (Orientation == Orient_Vertical)
		{
			ListAndScrollbar = SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						PinnedItemsPanel.ToSharedRef()
					]
					+SVerticalBox::Slot()
					.FillHeight(1)
					[
						ItemsPanel.ToSharedRef()
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(ScrollBarSize)
					[
						ScrollBar.ToSharedRef()
					]
				];
		}
		else
		{
			ListAndScrollbar = SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						PinnedItemsPanel.ToSharedRef()
					]
					+SVerticalBox::Slot()
					.FillHeight(1)
					[
						ItemsPanel.ToSharedRef()
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(ScrollBarSize)
					[
						ScrollBar.ToSharedRef()
					]
				];
		}
	}

	if (InHeaderRow)
	{
		// Only associate the scrollbar if we created it.
		// If the scrollbar was passed in from outside then it won't appear under our header row so doesn't need compensating for.
		if (!InScrollBar)
		{
			InHeaderRow->SetAssociatedVerticalScrollBar(ScrollBar.ToSharedRef(), 16.f);
		}

		this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				InHeaderRow.ToSharedRef()
			]
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				ListAndScrollbar.ToSharedRef()
			]
		];
	}
	else
	{
		this->ChildSlot
		[
			ListAndScrollbar.ToSharedRef()
		];
	}
	
}


bool SDynamicTableViewBase::SupportsKeyboardFocus() const
{
	// The ListView is focusable.
	return true;
}


void SDynamicTableViewBase::OnFocusLost( const FFocusEvent& InFocusEvent )
{
	bShowSoftwareCursor = false;
}

void SDynamicTableViewBase::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SCompoundWidget::OnMouseCaptureLost(CaptureLostEvent);

	bShowSoftwareCursor = false;
}

EActiveTimerReturnType SDynamicTableViewBase::UpdateInertialScroll(double InCurrentTime, float InDeltaTime)
{
	bool bKeepTicking = false;
	if (ItemsPanel.IsValid())
	{
		if (IsRightClickScrolling())
		{
			bKeepTicking = true;

			// We sample for the inertial scroll on tick rather than on mouse/touch move so
			// that we still get samples even if the mouse has not moved.
			if (CanUseInertialScroll(TickScrollDelta))
			{
				InertialScrollManager.AddScrollSample(TickScrollDelta, InCurrentTime);
			}
		}
		else
		{
			InertialScrollManager.UpdateScrollVelocity(InDeltaTime);
			const float ScrollVelocity = InertialScrollManager.GetScrollVelocity();

			if (ScrollVelocity != 0.f)
			{
				if (CanUseInertialScroll(ScrollVelocity))
				{
					bKeepTicking = true;
					ScrollBy(GetTickSpaceGeometry(), ScrollVelocity * InDeltaTime, AllowOverscroll);
				}
				else
				{
					InertialScrollManager.ClearScrollVelocity();
				}
			}

			if (AllowOverscroll == EAllowOverscroll::Yes)
			{
				// If we are currently in overscroll, the list will need refreshing.
				// Do this before UpdateOverscroll, as that could cause GetOverscroll() to be 0
				if (Overscroll.GetOverscroll(GetTickSpaceGeometry()) != 0.0f)
				{
					bKeepTicking = true;
					RequestLayoutRefresh();
				}

				Overscroll.UpdateOverscroll(InDeltaTime);
			}
		}

		TickScrollDelta = 0.f;
	}

	bIsScrollingActiveTimerRegistered = bKeepTicking;
	return bKeepTicking ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

EActiveTimerReturnType SDynamicTableViewBase::EnsureTickToRefresh(double InCurrentTime, float InDeltaTime)
{
	// Actual refresh isn't implemented here as it can be needed in response to changes in the panel geometry.
	// Since that isn't known until Tick (called after this when registered), refreshing here could result in two refreshes in one frame.

	return EActiveTimerReturnType::Stop;
}

void SDynamicTableViewBase::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (ItemsPanel.IsValid())
	{
		FGeometry PanelGeometry = FindChildGeometry( AllottedGeometry, ItemsPanel.ToSharedRef() );

		bool bPanelGeometryChanged = PanelGeometryLastTick.GetLocalSize() != PanelGeometry.GetLocalSize();
		
		if (bTotalItemLengthNeedRefresh || bPanelGeometryChanged)
		{
			ComputeTotalItemsLength(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale());

			bTotalItemLengthNeedRefresh = false;
		}

		if ( bItemsNeedRefresh || bPanelGeometryChanged)
		{
			PanelGeometryLastTick = PanelGeometry;
			
			const int32 NumItemsPerLine = GetNumItemsPerLine();
			const EScrollIntoViewResult ScrollIntoViewResult = ScrollIntoView(PanelGeometry);

			double TargetScrollOffset = GetTargetScrollOffset();

			if (bEnableAnimatedScrolling)
			{
				CurrentScrollOffset = FMath::FInterpTo(CurrentScrollOffset, TargetScrollOffset, (double)InDeltaTime, 12.0);
				if (FMath::IsNearlyEqual(CurrentScrollOffset, TargetScrollOffset, 0.01))
				{
					CurrentScrollOffset = TargetScrollOffset;
				}
			}
			else
			{
				CurrentScrollOffset = TargetScrollOffset;
			}

			const FReGenerateResults ReGenerateResults = ReGenerateItems( PanelGeometry );
			LastGenerateResults = ReGenerateResults;
			
			const int32 NumItemsBeingObserved = GetNumItemsBeingObserved();
			const int32 NumItemLines = NumItemsBeingObserved / NumItemsPerLine;

			const bool bEnoughRoomForAllItems = AllottedGeometry.GetLocalSize().Y >= ReGenerateResults.LengthOfGeneratedItems;
			
			const double InitialDesiredOffset = DesiredScrollOffset;
			// const bool bEnoughRoomForAllItems = ReGenerateResults.ExactNumLinesOnScreen >= NumItemLines;
			if (bEnoughRoomForAllItems)
			{
				// We can show all the items, so make sure there is no scrolling.
				SetScrollOffset(0.0);
				CurrentScrollOffset = TargetScrollOffset = DesiredScrollOffset;
			}
			else if (ReGenerateResults.bGeneratedPastLastItem)
			{
				SetScrollOffset(FMath::Max(0.0, ReGenerateResults.NewScrollOffset));
				CurrentScrollOffset = TargetScrollOffset = DesiredScrollOffset;
			}
			
			ItemsPanel->SetFirstLineScrollOffset(GetFirstLineScrollOffset());

			if (AllowOverscroll == EAllowOverscroll::Yes)
			{
				const float OverscrollAmount = Overscroll.GetOverscroll(GetTickSpaceGeometry());
				ItemsPanel->SetOverscrollAmount( OverscrollAmount );
			}

			UpdateSelectionSet();

			const double TotalItemsLength = GetTotalItemsLength();
			
			// Update scrollbar
			if (NumItemsBeingObserved > 0)
			{
				if (ReGenerateResults.ExactNumLinesOnScreen < 1.0f)
				{
					// We are be observing a single row which is larger than the available visible area, so we should calculate thumb size based on that
					const double VisibleSizeFraction = AllottedGeometry.GetLocalSize().Y / ReGenerateResults.LengthOfGeneratedItems;
					const double ThumbSizeFraction = FMath::Min(VisibleSizeFraction, 1.0);
					const double OffsetFraction = CurrentScrollOffset / TotalItemsLength;
					// const double OffsetFraction = CurrentScrollOffset / NumItemsBeingObserved;
					ScrollBar->SetState( OffsetFraction, ThumbSizeFraction );
				}
				else
				{
					// The thumb size is whatever fraction of the items we are currently seeing (including partially seen items).
					// e.g. if we are seeing 0.5 of the first generated widget and 0.75 of the last widget, that's 1.25 widgets.
					const double ThumbSizeFraction = AllottedGeometry.GetLocalSize().Y / TotalItemsLength;
					auto T = (TotalItemsLength - AllottedGeometry.GetLocalSize().Y);
					const double OffsetFraction = CurrentScrollOffset / TotalItemsLength;
					// const double OffsetFraction = CurrentScrollOffset / NumItemsBeingObserved;
					ScrollBar->SetState( OffsetFraction, ThumbSizeFraction );
				}
			}
			else
			{
				const double ThumbSizeFraction = 1;
				const double OffsetFraction = 0;
				ScrollBar->SetState( OffsetFraction, ThumbSizeFraction );
			}

			bWasAtEndOfList = (ScrollBar->DistanceFromBottom() < SMALL_NUMBER);

			bItemsNeedRefresh = false;
			ItemsPanel->SetRefreshPending(false);

			Invalidate(EInvalidateWidget::ChildOrder);
			
			if (ScrollIntoViewResult == EScrollIntoViewResult::Success)
			{
				// Notify as soon as we've made a widget for the item, even if we still have scrolling to do
				NotifyItemScrolledIntoView();
			}

			if (ScrollIntoViewResult == EScrollIntoViewResult::Deferred || CurrentScrollOffset != TargetScrollOffset)
			{
				// Either we haven't made the item yet or we still have scrolling to do, so we'll need another refresh next frame
				// We call this rather than just leave bItemsNeedRefresh as true to ensure that EnsureTickToRefresh is registered
				RequestLayoutRefresh();
			}
			else if (CurrentScrollOffset == TargetScrollOffset)
			{
				NotifyFinishedScrolling();
			}
		}
	}
}


void SDynamicTableViewBase::ScrollBar_OnUserScrolled( float InScrollOffsetFraction )
{
	const float ContentSize = GetTotalItemsLength();
	const float ScrollPanelSize = GetCachedGeometry().GetLocalSize().Y;

	//// Clamp to max scroll offset
	const float ClampedScrollOffset = FMath::Min(InScrollOffsetFraction * ContentSize, ContentSize - ScrollPanelSize);

	ScrollTo(ClampedScrollOffset);
}

FReply SDynamicTableViewBase::OnPreviewMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (MouseEvent.IsTouchEvent())
	{
		// Clear any inertia 
		this->InertialScrollManager.ClearScrollVelocity();
		// We have started a new interaction; track how far the user has moved since they put their finger down.
		AmountScrolledWhileRightMouseDown = 0;

		PressedScreenSpacePosition = MouseEvent.GetScreenSpacePosition();

		// Someone put their finger down in this list, so they probably want to drag the list.
		bStartedTouchInteraction = true;
		return FReply::Unhandled();
	}
	else
	{
		return FReply::Unhandled();
	}
}


FReply SDynamicTableViewBase::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Zero the scroll velocity so the list stops immediately on mouse down, even if the user does not drag
	this->InertialScrollManager.ClearScrollVelocity();

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnRightMouseButtonDown(MouseEvent);
	}

	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ScrollBar->IsNeeded() )
	{
		AmountScrolledWhileRightMouseDown = 0;

		// NOTE: We don't bother capturing the mouse, unless the user starts dragging a few pixels (see the
		// mouse move handling here.)  This is important so that the item row has a chance to select
		// items when the right mouse button is released.  Just keep in mind that you might not get
		// an OnMouseButtonUp event for the right mouse button if the user moves off of the table before
		// they reach our scroll threshold
		return FReply::Handled();
	}
	else if ( this->HasMouseCapture() )
	{
		// Consume all mouse buttons while we are RMB-dragging.
		return FReply::Handled();
	}
	return FReply::Unhandled();			
}

FReply SDynamicTableViewBase::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	if ( this->HasMouseCapture() )
	{
		// Consume all other mouse buttons while we are RMB-dragging.
		return FReply::Handled();
	}
	return FReply::Unhandled();			

}


FReply SDynamicTableViewBase::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{

		OnRightMouseButtonUp( MouseEvent );

		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		bShowSoftwareCursor = false;

		// If we have mouse capture, snap the mouse back to the closest location that is within the list's bounds
		if ( HasMouseCapture() )
		{
			FSlateRect ListScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();
			FVector2f CursorPosition = MyGeometry.LocalToAbsolute( SoftwareCursorPosition );

			FIntPoint BestPositionInList(
				FMath::RoundToInt( FMath::Clamp( CursorPosition.X, ListScreenSpaceRect.Left, ListScreenSpaceRect.Right ) ),
				FMath::RoundToInt( FMath::Clamp( CursorPosition.Y, ListScreenSpaceRect.Top, ListScreenSpaceRect.Bottom ) )
				);

			Reply.SetMousePos(BestPositionInList);
		}

		return Reply;
	}
	return FReply::Unhandled();
}


FReply SDynamicTableViewBase::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{	
	if( bEnableRightClickScrolling && MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && !MouseEvent.IsTouchEvent() )
	{
		// We only care about deltas along the scroll axis
		FTableViewDimensions CursorDeltaDimensions(Orientation, MouseEvent.GetCursorDelta());
		CursorDeltaDimensions.LineAxis = 0.f;
		
		const float ScrollByAmount = CursorDeltaDimensions.ScrollAxis / MyGeometry.Scale;

		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs( ScrollByAmount );

		// Has the mouse moved far enough with the right mouse button held down to start capturing
		// the mouse and dragging the view?
		if( IsRightClickScrolling() )
		{
			// Make sure the active timer is registered to update the inertial scroll
			if (!bIsScrollingActiveTimerRegistered)
			{
				bIsScrollingActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDynamicTableViewBase::UpdateInertialScroll));
			}

			TickScrollDelta -= ScrollByAmount;

			const float AmountScrolled = this->ScrollBy( MyGeometry, -ScrollByAmount, AllowOverscroll );

			FReply Reply = FReply::Handled();

			// The mouse moved enough that we're now dragging the view. Capture the mouse
			// so the user does not have to stay within the bounds of the list while dragging.
			if(this->HasMouseCapture() == false)
			{
				Reply.CaptureMouse( AsShared() ).UseHighPrecisionMouseMovement( AsShared() );
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
				bShowSoftwareCursor = true;
			}

			// Check if the mouse has moved.
			if( AmountScrolled != 0 )
			{
				SoftwareCursorPosition += CursorDeltaDimensions.ToVector2D();
			}

			return Reply;
		}
	}

	return FReply::Unhandled();
}


void SDynamicTableViewBase::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.IsTouchEvent() )
	{
		if ( !bStartedTouchInteraction )
		{
			// If we don't have touch capture, see if a touch event entered from a child widget.
			// If it did, begin scrolling
			if ( MyGeometry.IsUnderLocation(MouseEvent.GetLastScreenSpacePosition()) )
			{
				bStartedTouchInteraction = true;
			}
		}
	}
}


void SDynamicTableViewBase::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	bStartedTouchInteraction = false;
	if(this->HasMouseCapture() == false)
	{
		// No longer scrolling (unless we have mouse capture)
		AmountScrolledWhileRightMouseDown = 0;
	}
}


FReply SDynamicTableViewBase::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( !MouseEvent.IsControlDown() )
	{
		// Make sure scroll velocity is cleared so it doesn't fight with the mouse wheel input
		this->InertialScrollManager.ClearScrollVelocity();

		float AmountScrolledInItems = 0.f;
		if (FixedLineScrollOffset.IsSet()) // TODO Remove??
		{
			// When we need to maintain a fixed offset, we scroll by items. This prevents the list not moving or jumping unexpectedly far on an individual scroll wheel motion.
			const double AdditionalOffset = (MouseEvent.GetWheelDelta() >= 0.f ? -1.f : 1.f) * GetGlobalScrollAmount() * WheelScrollMultiplier;
			const double NewScrollOffset = FMath::Max(0., DesiredScrollOffset + AdditionalOffset);
			AmountScrolledInItems = this->ScrollTo(NewScrollOffset);
		}
		else
		{
			// No required offset to maintain, so we scroll by units
			AmountScrolledInItems = this->ScrollBy(MyGeometry, -MouseEvent.GetWheelDelta() * GetGlobalScrollAmount() * WheelScrollMultiplier, EAllowOverscroll::No);
		}
		
		if (ConsumeMouseWheel == EConsumeMouseWheel::Always || (FMath::Abs(AmountScrolledInItems) > 0.0f && ConsumeMouseWheel != EConsumeMouseWheel::Never))
		{
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}


FReply SDynamicTableViewBase::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::End )
	{
		ScrollToBottom();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent); 
}

FCursorReply SDynamicTableViewBase::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if ( IsRightClickScrolling() && CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton) )
	{
		// We hide the native cursor as we'll be drawing the software EMouseCursor::GrabHandClosed cursor
		return FCursorReply::Cursor( EMouseCursor::None );
	}
	else
	{
		return FCursorReply::Unhandled();
	}
}

FReply SDynamicTableViewBase::OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	// See OnPreviewMouseButtonDown()
	//     if (MouseEvent.IsTouchEvent())

	return FReply::Unhandled();
}

FReply SDynamicTableViewBase::OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	if (bStartedTouchInteraction)
	{
		// We only care about deltas along the scroll axis
		FTableViewDimensions CursorDeltaDimensions(Orientation, InTouchEvent.GetCursorDelta());
		CursorDeltaDimensions.LineAxis = 0.f;

		const float ScrollByAmount = CursorDeltaDimensions.ScrollAxis / MyGeometry.Scale;

		AmountScrolledWhileRightMouseDown += FMath::Abs( ScrollByAmount );
		
		TickScrollDelta -= ScrollByAmount;

		if (FSlateApplication::Get().HasTraveledFarEnoughToTriggerDrag(InTouchEvent, PressedScreenSpacePosition, Orientation))
		{
			// Make sure the active timer is registered to update the inertial scroll
			if ( !bIsScrollingActiveTimerRegistered )
			{
				bIsScrollingActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDynamicTableViewBase::UpdateInertialScroll));
			}

			const float AmountScrolled = this->ScrollBy( MyGeometry, -ScrollByAmount, AllowOverscroll );
			if (AmountScrolled != 0)
			{
				ScrollBar->BeginScrolling();

				// The user has moved the list some amount; they are probably
				// trying to scroll. From now on, the list assumes the user is scrolling
				// until they lift their finger.
				return HasMouseCapture() ? FReply::Handled() : FReply::Handled().CaptureMouse(AsShared());
			}
		}

		return FReply::Unhandled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

FReply SDynamicTableViewBase::OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
{
	AmountScrolledWhileRightMouseDown = 0;
	bStartedTouchInteraction = false;

	ScrollBar->EndScrolling();

	if (HasMouseCapture())
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Unhandled();
	}
}


int32 SDynamicTableViewBase::GetNumGeneratedChildren() const
{
	return (ItemsPanel.IsValid())
		? ItemsPanel->GetChildren()->Num()
		: 0;
}

TSharedPtr<SHeaderRow> SDynamicTableViewBase::GetHeaderRow() const
{
	return HeaderRow;
}

bool SDynamicTableViewBase::IsRightClickScrolling() const
{
	return AmountScrolledWhileRightMouseDown >= FSlateApplication::Get().GetDragTriggerDistance() &&
		(this->ScrollBar->IsNeeded() || AllowOverscroll == EAllowOverscroll::Yes);
}

bool SDynamicTableViewBase::IsUserScrolling() const
{
	bool bUserScroll = this->ScrollBar->IsNeeded() && this->ScrollBar->IsScrolling();
	return bUserScroll || IsRightClickScrolling();
}

void SDynamicTableViewBase::RequestListRefresh()
{
	bTotalItemLengthNeedRefresh = true;

	RequestLayoutRefresh();
}

bool SDynamicTableViewBase::IsPendingRefresh() const
{
	return bItemsNeedRefresh || ItemsPanel->IsRefreshPending();
}

bool SDynamicTableViewBase::ComputeVolatility() const
{
	return BackgroundBrush.IsBound();
}

int32 SDynamicTableViewBase::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 NewLayerId = LayerId;

	const FSlateBrush* BackgroundBrushResource = BackgroundBrush.Get();
	if ( BackgroundBrushResource && BackgroundBrushResource->DrawAs != ESlateBrushDrawType::NoDrawType )
	{

		const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
		const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		FSlateDrawElement::MakeBox(
				OutDrawElements,
				++NewLayerId,
				AllottedGeometry.ToPaintGeometry(),
				BackgroundBrushResource,
				DrawEffects,
				BackgroundBrushResource->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint() 
		);
	}

	NewLayerId = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, NewLayerId, InWidgetStyle, bParentEnabled );

	if( !bShowSoftwareCursor )
	{
		return NewLayerId;
	}

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));
	const FVector2f CursorSize = Brush->ImageSize / AllottedGeometry.Scale;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++NewLayerId,
		AllottedGeometry.ToPaintGeometry(CursorSize, FSlateLayoutTransform(SoftwareCursorPosition - (CursorSize / .5f ))),
		Brush
		);

	return NewLayerId;
}

SDynamicTableViewBase::SDynamicTableViewBase( ETableViewMode::Type InTableViewMode )
	: TableViewMode( InTableViewMode )
	, bStartedTouchInteraction( false )
	, AmountScrolledWhileRightMouseDown( 0 )
	, TickScrollDelta( 0 )
	, LastGenerateResults( 0,0,0,false )
	, bWasAtEndOfList(false)
	, SelectionMode( ESelectionMode::Multi )
	, SoftwareCursorPosition( ForceInitToZero )
	, bShowSoftwareCursor( false )
	, WheelScrollMultiplier(1)
	, BackgroundBrush(FStyleDefaults::GetNoBrush())
	, bIsScrollingActiveTimerRegistered( false )
	, Overscroll()
	, AllowOverscroll(EAllowOverscroll::Yes)
	, ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
	, bItemsNeedRefresh( false )	
{
	FixedLineScrollOffset = 0.25f;
}

double SDynamicTableViewBase::GetTargetScrollOffset() const
{
	if (FixedLineScrollOffset.IsSet() && !IsRightClickScrolling() && InertialScrollManager.GetScrollVelocity() == 0.f)
	{
		const int32 NumItemsPerLine = GetNumItemsPerLine();
		const double DesiredLineOffset = FMath::FloorToDouble(DesiredScrollOffset / NumItemsPerLine) - FixedLineScrollOffset.GetValue();
		
		//return FMath::Max(0.0, FMath::CeilToDouble(DesiredScrollOffset) - AdditionalOffset);
		return FMath::Max(0.0, DesiredLineOffset * NumItemsPerLine);
	}
	return DesiredScrollOffset;
}

float SDynamicTableViewBase::ScrollBy(const FGeometry& MyGeometry, float ScrollByAmount, EAllowOverscroll InAllowOverscroll)
{
	const float ScrollMin = 0.0f;
	const float ScrollMax = FMath::Max(GetTotalItemsLength() - MyGeometry.GetLocalSize().Y, 0.f);

	if (InAllowOverscroll == EAllowOverscroll::Yes && Overscroll.ShouldApplyOverscroll(FMath::IsNearlyZero(DesiredScrollOffset), FMath::IsNearlyEqual(DesiredScrollOffset, ScrollMax), ScrollByAmount))
	{
		const float ActuallyScrolledBy = Overscroll.ScrollBy(MyGeometry, ScrollByAmount);
		if (ActuallyScrolledBy != 0.0f)
		{
			this->RequestLayoutRefresh();
			return ActuallyScrolledBy;
		}
	}

	// const float AvgItemLength = GetTotalItemsLength() / GetNumItemsBeingObserved();
	// const double ClampedScrollOffset = FMath::Clamp(DesiredScrollOffset + ScrollByAmount, ScrollMin - (AvgItemLength * 10.f), ScrollMax + (AvgItemLength * 10.f));
	return ScrollTo(DesiredScrollOffset + ScrollByAmount);
}

float SDynamicTableViewBase::ScrollTo( float InScrollOffset)
{
	const float NewScrollOffset = FMath::Clamp( InScrollOffset, -10.0f, GetTotalItemsLength()+10.0f );
	float AmountScrolled = FMath::Abs( DesiredScrollOffset - NewScrollOffset );

	if (bWasAtEndOfList && NewScrollOffset >= DesiredScrollOffset)
	{
		AmountScrolled = 0;
	}

	SetScrollOffset( NewScrollOffset );

	return AmountScrolled;
}

float SDynamicTableViewBase::GetScrollOffset() const
{
	return DesiredScrollOffset;
}

void SDynamicTableViewBase::SetScrollOffset( const float InScrollOffset )
{
	const float InValidatedOffset = FMath::Max(0.0f, InScrollOffset);
	if (DesiredScrollOffset != InValidatedOffset)
	{
		DesiredScrollOffset = InValidatedOffset;
		OnTableViewScrolled.ExecuteIfBound(DesiredScrollOffset);
		RequestLayoutRefresh();
	}
}

void SDynamicTableViewBase::EndInertialScrolling()
{
	InertialScrollManager.ClearScrollVelocity();
}

void SDynamicTableViewBase::AddScrollOffset(const float InScrollOffsetDelta, bool RefreshList)
{
	if (FMath::IsNearlyEqual(InScrollOffsetDelta, 0.0f) == false)
	{
		DesiredScrollOffset += InScrollOffsetDelta;
		if (RefreshList)
		{
			OnTableViewScrolled.ExecuteIfBound(DesiredScrollOffset);
			RequestLayoutRefresh();
		}
	}
}

void SDynamicTableViewBase::SetScrollbarVisibility(const EVisibility InVisibility)
{
	if (ScrollBar)
	{
		ScrollBar->SetVisibility(InVisibility);
	}
}

EVisibility SDynamicTableViewBase::GetScrollbarVisibility() const
{
	return ScrollBar ? ScrollBar->ShouldBeVisible() : EVisibility::Collapsed;
}

bool SDynamicTableViewBase::IsScrollbarNeeded() const
{
	if (ScrollBar)
	{
		return ScrollBar->IsNeeded();
	}

	return false;
}

void SDynamicTableViewBase::SetFixedLineScrollOffset(TOptional<double> InFixedLineScrollOffset)
{
	if (FixedLineScrollOffset != InFixedLineScrollOffset)
	{
		FixedLineScrollOffset = InFixedLineScrollOffset;
		RequestLayoutRefresh();
	}
}

void SDynamicTableViewBase::SetIsScrollAnimationEnabled(bool bInEnableScrollAnimation)
{
	bEnableAnimatedScrolling = bInEnableScrollAnimation;
}

void SDynamicTableViewBase::SetAllowOverscroll(EAllowOverscroll InAllowOverscroll)
{
	AllowOverscroll = InAllowOverscroll;
}

void SDynamicTableViewBase::SetIsRightClickScrollingEnabled(const bool bInEnableRightClickScrolling)
{
	bEnableRightClickScrolling = bInEnableRightClickScrolling;
}

void SDynamicTableViewBase::SetWheelScrollMultiplier(float NewWheelScrollMultiplier)
{
	WheelScrollMultiplier = NewWheelScrollMultiplier;
}

void SDynamicTableViewBase::SetBackgroundBrush(const TAttribute<const FSlateBrush*>& InBackgroundBrush)
{
	BackgroundBrush.SetImage(*this, InBackgroundBrush);
}

void SDynamicTableViewBase::InsertWidget( const TSharedRef<ITableRow> & WidgetToInset )
{
	ItemsPanel->AddSlot(0)
	[
		WidgetToInset->AsWidget()
	];
}

void SDynamicTableViewBase::AppendWidget( const TSharedRef<ITableRow>& WidgetToAppend )
{
	ItemsPanel->AddSlot()
	[
		WidgetToAppend->AsWidget()
	];
}

void SDynamicTableViewBase::ClearWidgets()
{
	ItemsPanel->ClearItems();
}

const FChildren* SDynamicTableViewBase::GetConstructedTableItems() const
{
	return ItemsPanel->GetChildren();
}

void SDynamicTableViewBase::InsertPinnedWidget( const TSharedRef<SWidget> & WidgetToInset )
{
	PinnedItemsPanel->AddSlot(0)
	[
		WidgetToInset
	];
}

void SDynamicTableViewBase::AppendPinnedWidget( const TSharedRef<SWidget>& WidgetToAppend )
{
	PinnedItemsPanel->AddSlot()
	[
		WidgetToAppend
	];
}

void SDynamicTableViewBase::ClearPinnedWidgets()
{
	PinnedItemsPanel->ClearItems();
}

float SDynamicTableViewBase::GetNumLiveWidgets() const
{
	return ItemsPanel->GetChildren()->Num();
}

int32 SDynamicTableViewBase::GetNumItemsPerLine() const
{
	return 1;
}

float SDynamicTableViewBase::GetFirstLineScrollOffset() const
{
	// FMath::Fractional() is insufficient here as it casts to int32 (too small for the integer part of a float when
	// the scroll offset is enormous), so we do a double/int64 version here.
	const double FirstLineScrollOffset = CurrentScrollOffset / GetNumItemsPerLine();
	return FirstLineScrollOffset - (int64)FirstLineScrollOffset;
}

void SDynamicTableViewBase::NavigateToWidget(const uint32 UserIndex, const TSharedPtr<SWidget>& NavigationDestination, ENavigationSource NavigationSource) const
{
	FSlateApplication::Get().NavigateToWidget(UserIndex, NavigationDestination, NavigationSource);
}

void SDynamicTableViewBase::OnRightMouseButtonUp(const FPointerEvent& MouseEvent)
{
	FVector2f SummonLocation = MouseEvent.GetScreenSpacePosition();
	const bool bShouldOpenContextMenu = !IsRightClickScrolling();
	const bool bContextMenuOpeningBound = OnContextMenuOpening.IsBound();

	if (bShouldOpenContextMenu && bContextMenuOpeningBound)
	{
		// Get the context menu content. If NULL, don't open a menu.
		TSharedPtr<SWidget> MenuContent = OnContextMenuOpening.Execute();

		if (MenuContent.IsValid())
		{
			bShowSoftwareCursor = false;

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent.ToSharedRef(), SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
	}

	AmountScrolledWhileRightMouseDown = 0;
}

float SDynamicTableViewBase::GetScrollRateInItems() const
{
	return (LastGenerateResults.LengthOfGeneratedItems != 0 && LastGenerateResults.ExactNumLinesOnScreen != 0)
		// Approximate a consistent scrolling rate based on the average item height.
		? LastGenerateResults.ExactNumLinesOnScreen / LastGenerateResults.LengthOfGeneratedItems
		// Scroll 1/2 an item at a time as a default.
		: 0.5f;
}

void SDynamicTableViewBase::RequestLayoutRefresh()
{
	if (!bItemsNeedRefresh)
	{
		bItemsNeedRefresh = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDynamicTableViewBase::EnsureTickToRefresh));
	}

	if (ItemsPanel.IsValid())
	{
		ItemsPanel->SetRefreshPending(true);
	}

	Invalidate(EInvalidateWidget::Layout);
}

void SDynamicTableViewBase::ScrollToTop()
{
	EndInertialScrolling();
	SetScrollOffset(0);
	RequestLayoutRefresh();
}

void SDynamicTableViewBase::ScrollToBottom()
{
	const float ScrollMax = GetTotalItemsLength() - GetCachedGeometry().GetLocalSize().Y;
	
	EndInertialScrolling();
	SetScrollOffset(ScrollMax);
	RequestLayoutRefresh();
}

bool SDynamicTableViewBase::IsScrolling() const
{
	return ScrollBar->IsScrolling();
}

FVector2D SDynamicTableViewBase::GetScrollDistance()
{
	return FVector2D( 0, ScrollBar->DistanceFromTop() );
}

FVector2D SDynamicTableViewBase::GetScrollDistanceRemaining()
{
	return FVector2D( 0, ScrollBar->DistanceFromBottom() );
}

TSharedRef<class SWidget> SDynamicTableViewBase::GetScrollWidget()
{
	return SharedThis(this);
}

void SDynamicTableViewBase::OnClippingChanged()
{
	ItemsPanel->SetClipping(GetClipping());
}

bool SDynamicTableViewBase::CanUseInertialScroll( float ScrollAmount ) const
{
	const auto CurrentOverscroll = Overscroll.GetOverscroll(GetTickSpaceGeometry());

	// We allow sampling for the inertial scroll if we are not in the overscroll region,
	// Or if we are scrolling outwards of the overscroll region
	return CurrentOverscroll == 0.f || FMath::Sign(CurrentOverscroll) != FMath::Sign(ScrollAmount);
}

int32 SDynamicTableViewBase::GetNumPinnedItems() const
{
	return PinnedItemsPanel->GetChildren()->Num();
}

EVisibility SDynamicTableViewBase::GetPinnedItemsVisiblity() const
{
	return PinnedItemsPanel->GetChildren()->Num() != 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

// static const TBitArray<> EmptyBitArray = TBitArray<>();
//
// const TBitArray<>& TableViewHelpers::GetEmptyBitArray()
// {
// 	return EmptyBitArray;
// }
