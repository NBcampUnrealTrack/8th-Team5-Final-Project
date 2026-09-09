// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/InventoryDragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Math/TransformCalculus2D.h"

void UInventoryDragDropOperation::CreateVisual(UUserWidget* InSourceWidget, UTexture2D* IconTexture, const FVector2D& VisualSize)
{
	if (InSourceWidget == nullptr || DragVisualRoot.IsValid())
	{
		return;
	}
	SourceWidgetContext = InSourceWidget;
	BaseVisualSize = VisualSize;

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

	TSharedRef<SImage> NewDragImage =
		SNew(SImage)
		.Visibility(EVisibility::HitTestInvisible)
		.Image(&DragVisualBrush)
		.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f));
	NewDragImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	DragImageWidget = NewDragImage;

	TSharedRef<SBox> NewDragBox =
		SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.WidthOverride(VisualSize.X)
		.HeightOverride(VisualSize.Y)
		[
			NewDragImage
		];

	DragBoxWidget = NewDragBox;
	DragVisualRoot = NewDragBox;

	// bRotated는 NativeOnDragDetected에서 CreateVisual() 호출 전에 이미 세팅돼 있다.
	// 그런데 이 함수는 지금까지 그 값을 무시하고 항상 회전 안 된 모양으로만 비주얼을
	// 만들고 있었다 (회전 반영은 SetRotated()가 T키로 토글될 때만 일어남). 그래서
	// 이미 회전된 상태로 집은 아이템은 드래그 시작 시점엔 무조건 회전 안 된 모양으로
	// 보이는 버그가 있었다. 여기서 초기 상태를 한 번 맞춰준다.
	if (bRotated)
	{
		const FVector2D RotatedBoxSize(VisualSize.Y, VisualSize.X);
		NewDragBox->SetWidthOverride(RotatedBoxSize.X);
		NewDragBox->SetHeightOverride(RotatedBoxSize.Y);
		NewDragImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(90.0f))));
	}

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

void UInventoryDragDropOperation::SetRotated(bool bInRotated)
{
	if (bRotated == bInRotated)
	{
		return;
	}
	bRotated = bInRotated;

	if (DragBoxWidget.IsValid())
	{
		const FVector2D NewBoxSize = bRotated
			? FVector2D(BaseVisualSize.Y, BaseVisualSize.X)
			: BaseVisualSize;
		DragBoxWidget->SetWidthOverride(NewBoxSize.X);
		DragBoxWidget->SetHeightOverride(NewBoxSize.Y);
	}

	if (DragImageWidget.IsValid())
	{
		const float AngleDegrees = bRotated ? 90.0f : 0.0f;
		DragImageWidget->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(AngleDegrees))));
	}

	UpdateVisualPosition();
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