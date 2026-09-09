#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryTypes.h"
#include "InventoryItemWidget.generated.h"

class UInventoryComponent;
class UDragDropOperation;
class UImage;
class UTextBlock;
class UBorder;
class UWidgetAnimation;
class UMaterialInstanceDynamic;
class USizeBox;

/**
 * Per-item input layer used to start inventory drags from the item's geometry
 * instead of from the geometry of the entire inventory widget.
 */
UCLASS()
class HEAVENBREACH_API UInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeItem(
		UInventoryComponent* InSourceInventory,
		const FItemInstance& InItem,
		FIntPoint InItemSize,
		float InCellSize);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;

	UFUNCTION()
	void HandleDragFinished(UDragDropOperation* Operation);

private:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_ItemBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_ItemIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<USizeBox> SizeBox_Icon;

	// 1x2, 1x3처럼 정사각형이 아닌(길쭉한) 아이템은 아이콘 아트 자체에 여백이 있어서
	// 칸을 다 못 채우고 짧아 보이는 경우가 많다. 그런 아이템에만 이 배율만큼 아이콘
	// 박스를 살짝 키워서 보정한다. 1.0이면 보정 없음. 값은 디자이너가 눈으로 보면서
	// WBP 클래스 디폴트에서 바로 조정 가능하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1.0", ClampMax = "2.0", AllowPrivateAccess = "true"))
	float ElongatedIconOverscale = 1.15f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_FallbackName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_StackCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_ItemGlow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_ItemFrame;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_FavoriteStar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_GradeFrame;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_FavoritePulse;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BackgroundMID;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> SourceInventory;

	FItemInstance Item;
	FIntPoint ItemSize = FIntPoint(1, 1);
	FVector2D GrabPixelOffset = FVector2D::ZeroVector;
	float CellSize = 64.0f;
	bool bSplitRequested = false;
};
