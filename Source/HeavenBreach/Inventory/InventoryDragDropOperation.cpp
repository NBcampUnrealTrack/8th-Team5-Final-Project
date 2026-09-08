// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/InventoryDragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

void UInventoryDragDropOperation::CreateVisual(UUserWidget* InSourceWidget, UTexture2D* IconTexture, const FVector2D& VisualSize)
{
	if (InSourceWidget == nullptr || DragVisualRoot.IsValid())
	{
		return;
	}
	SourceWidgetContext = InSourceWidget;

	UWorld* World = InSourceWidget->GetWorld();
	UGameViewportClient* GameViewport = World ? World->GetGameViewport() : nullptr;
	if (GameViewport == nullptr)
	{
		return;
	}

	if (IconTexture != nullptr)
	{
		DragVisualBrush.SetResourceObject(IconTexture);
	}
	DragVisualBrush.ImageSize = VisualSize;
	DragVisualBrush.DrawAs = ESlateBrushDrawType::Image;

	TSharedRef<SBox> NewDragBox =
		SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.WidthOverride(VisualSize.X)
		.HeightOverride(VisualSize.Y)
		[
			SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.Image(&DragVisualBrush)
				.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f))
		];

	DragVisualRoot = NewDragBox;

	// 뷰포트에 올리기 전에 이미 커서 위치로 배치해서 첫 프레임에 엉뚱한 곳에 나타나는 걸 막는다.
	UpdateVisualPosition();

	GameViewport->AddViewportWidgetContent(NewDragBox, 1000);

	// 이 Operation 자체의 이벤트에 스스로 바인딩한다.
	// OnDragged는 커서가 어디에 있든(패널 밖이든, 다른 패널이든) 매 프레임 호출되고,
	// OnDrop/OnDragCancelled는 드래그가 끝날 때 무조건 정확히 한 번 호출된다.
	OnDragged.AddDynamic(this, &UInventoryDragDropOperation::HandleDragged);
	OnDrop.AddDynamic(this, &UInventoryDragDropOperation::HandleDrop);
	OnDragCancelled.AddDynamic(this, &UInventoryDragDropOperation::HandleDragCancelled);
}

void UInventoryDragDropOperation::UpdateVisualPosition()
{
	if (!DragVisualRoot.IsValid())
	{
		return;
	}
	UUserWidget* SourceWidget = SourceWidgetContext.Get();
	if (SourceWidget == nullptr)
	{
		return;
	}
	const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(SourceWidget);
	DragVisualRoot->SetRenderTransform(FSlateRenderTransform(MousePos - GrabPixelOffset));
}

void UInventoryDragDropOperation::DestroyVisual()
{
	if (!DragVisualRoot.IsValid())
	{
		return;
	}
	UUserWidget* SourceWidget = SourceWidgetContext.Get();
	UWorld* World = SourceWidget ? SourceWidget->GetWorld() : nullptr;
	if (UGameViewportClient* GameViewport = World ? World->GetGameViewport() : nullptr)
	{
		GameViewport->RemoveViewportWidgetContent(DragVisualRoot.ToSharedRef());
	}
	DragVisualRoot.Reset();
}

void UInventoryDragDropOperation::HandleDragged(UDragDropOperation* Operation)
{
	UpdateVisualPosition();
}

void UInventoryDragDropOperation::HandleDrop(UDragDropOperation* Operation)
{
	DestroyVisual();
}

void UInventoryDragDropOperation::HandleDragCancelled(UDragDropOperation* Operation)
{
	DestroyVisual();
}