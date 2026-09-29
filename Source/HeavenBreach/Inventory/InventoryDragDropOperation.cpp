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

	// 주의: AddViewportWidgetContent로 뷰포트에 직접 붙는 위젯은 뷰포트 오버레이 슬롯이 기본
	// Fill이라, WidthOverride/HeightOverride를 줘도 이 박스 자체는 화면 전체 크기로 늘어난다.
	// 그래서 이 박스의 HAlign/VAlign이 "박스 안에서 화면 전체 대비 아이콘이 어디 놓이는가"를
	// 결정해버리는데, Center로 하면 (마우스 위치 + 화면 절반)만큼 아이콘이 우측 하단으로
	// 밀려나 마우스를 전혀 따라가지 않는 것처럼 보이는 심각한 회귀가 생긴다(2026-09-28 발견).
	// 그래서 반드시 Left/Top이어야 한다 - UpdateVisualPosition()의 RenderTransform 이동값이
	// 곧바로 아이콘의 좌상단 위치가 되어야 마우스 좌표와 정확히 일치한다.
	// 회전 시 피벗이 박스 중심에서 어긋나던 원래 문제는 Padding으로 따로 보정한다(아래 참고).
	TSharedRef<SBox> NewDragBox =
		SNew(SBox)
		.Visibility(EVisibility::HitTestInvisible)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.WidthOverride(VisualSize.X)
		.HeightOverride(VisualSize.Y)
		.Padding(ComputeCenteringPadding())
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

FMargin UInventoryDragDropOperation::ComputeCenteringPadding() const
{
	// 박스 정렬이 Left/Top으로 고정된 상태에서(위 CreateVisual 주석 참고), 회전 시 박스 크기가
	// (Base.Y, Base.X)로 스왑되는데 아이콘 자체는 항상 BaseVisualSize 그대로다. 아이콘을 박스의
	// (0,0)에 그냥 붙이면 회전축(아이콘 자기 중심)이 박스의 실제 중심과 어긋난다. 그래서 박스의
	// "의도된" 중심과 아이콘의 중심이 일치하도록 좌상단 Padding으로 차이만큼 밀어준다.
	const FVector2D CurrentBoxSize = bRotated
		? FVector2D(BaseVisualSize.Y, BaseVisualSize.X)
		: BaseVisualSize;
	const float PadLeft = (CurrentBoxSize.X - BaseVisualSize.X) * 0.5f;
	const float PadTop = (CurrentBoxSize.Y - BaseVisualSize.Y) * 0.5f;
	return FMargin(PadLeft, PadTop, 0.0f, 0.0f);
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
		DragBoxWidget->SetPadding(ComputeCenteringPadding());
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