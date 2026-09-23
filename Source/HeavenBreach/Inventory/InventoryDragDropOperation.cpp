// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/InventoryDragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SizeBox.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Inventory/InventoryItemWidget.h"
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

	// VisualSize는 현재(회전 반영된) 점유 크기이므로, 고정값인 BaseVisualSize는 "회전 전" 크기로 되돌려 저장한다.
	BaseVisualSize = bRotated ? FVector2D(VisualSize.Y, VisualSize.X) : VisualSize;

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
	// 브러시 크기는 항상 BaseVisualSize(고정된 "원래" 크기)로만 잡는다(SetRotated에서도 재조정 안 함).
	DragVisualBrush.ImageSize = BaseVisualSize;
	DragVisualBrush.DrawAs = ESlateBrushDrawType::Image;

	TSharedRef<SImage> NewDragImage =
		SNew(SImage)
		.Visibility(EVisibility::HitTestInvisible)
		.Image(&DragVisualBrush)
		.ColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.7f));
	NewDragImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	if (bRotated)
	{
		NewDragImage->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(90.0f))));
	}
	DragImageWidget = NewDragImage;

	// Center 정렬이어야 회전 피벗이 항상 박스 중심에 맞는다(Left/Top이면 회전축이 어긋난다).
	TSharedRef<SBox> NewDragBox =
		SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.WidthOverride(VisualSize.X)
		.HeightOverride(VisualSize.Y)
		[
			NewDragImage
		];

	DragBoxWidget = NewDragBox;
	DragVisualRoot = NewDragBox;

	UpdateVisualPosition();

	GameViewport->AddViewportWidgetContent(NewDragBox, 1000);

	// OnDragged/OnDrop/OnDragCancelled 콜백을 등록해 드래그 위치 갱신과 드롭/취소 시 정리를 처리한다.
	OnDragged.AddDynamic(this, &UInventoryDragDropOperation::HandleDragged);
	OnDrop.AddDynamic(this, &UInventoryDragDropOperation::HandleDrop);
	OnDragCancelled.AddDynamic(this, &UInventoryDragDropOperation::HandleDragCancelled);
}

void UInventoryDragDropOperation::SetDefaultVisual(USizeBox* InVisualBox, UInventoryItemWidget* InVisualItemWidget, const FVector2D& InBaseSize)
{
	DefaultVisualBox = InVisualBox;
	DefaultVisualItemWidget = InVisualItemWidget;

	// CreateVisual()이 이미 BaseVisualSize를 세팅했다면 덮어쓰지 않는다(값이 꼬이면 SetRotated가 틀어짐).
	if (BaseVisualSize.IsZero())
	{
		BaseVisualSize = bRotated ? FVector2D(InBaseSize.Y, InBaseSize.X) : InBaseSize;
	}
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

	// SetDefaultVisual()로 등록된 UMG 미리보기(있다면)도 같이 갱신한다.
	if (DefaultVisualBox != nullptr)
	{
		const FVector2D NewBoxSize = bRotated
			? FVector2D(BaseVisualSize.Y, BaseVisualSize.X)
			: BaseVisualSize;
		DefaultVisualBox->SetWidthOverride(NewBoxSize.X);
		DefaultVisualBox->SetHeightOverride(NewBoxSize.Y);
	}
	if (DefaultVisualItemWidget != nullptr)
	{
		DefaultVisualItemWidget->SetPreviewRotated(bRotated);
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