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
class UProgressBar;

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

	void SetPreviewRotated(bool bInRotated);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ProgressBar_Durability;

	// 포션 등 소비 아이템 전용 사용 쿨타임 진행바. SizeBox_Icon 위(Overlay)에 겹쳐 놓는다.
	// ProgressBar_Durability와 마찬가지로 위젯 Style의 Fill Type을 Radial/Linear로 바꾸는 건
	// WBP 쪽 몫이고, 코드는 Percent만 매 틱 채운다. 쿨타임은 아이템 종류(RowName)별로 독립이라
	// 같은 종류의 포션 칸들만 같이 채워지고, 다른 종류의 소비 아이템은 서로 영향을 주지 않는다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ProgressBar_Cooldown;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1.0", ClampMax = "2.0", AllowPrivateAccess = "true"))
	float ElongatedIconOverscale = 1.15f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_FallbackName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_StackCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Broken;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_BrokenBG;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_ItemGlow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_ItemFrame;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_FavoriteStar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_MaterialBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_GradeFrame;
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_FavoritePulse;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BackgroundMID;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> SourceInventory;

	FItemInstance Item;
	// InitializeItem에서 한 번만 계산해두고 NativeTick(쿨타임 진행바 갱신)에서 매 프레임 재사용 -
	// 소비 아이템이 아닌 칸은 매 틱 ResolveItemViewData를 다시 부를 필요가 없다.
	bool bIsConsumableItem = false;
	FIntPoint ItemSize = FIntPoint(1, 1);

	// 회전에 영향받지 않는 아이템의 "원래(회전 전)" 칸 크기. InitializeItem에서 ItemSize(현재
	// 회전 상태 기준 점유 크기)와 Item.bRotated를 보고 한 번만 역산해서 저장해둔다.
	// T키 드래그 프리뷰 회전(SetPreviewRotated)이 Item.bRotated만 바꾸고 ItemSize는 갱신하지
	// 않기 때문에, 아이콘 자연 크기 계산은 항상 이 값(회전과 무관하게 고정)을 써야 한다 -
	// 예전에는 ItemSize를 그 자리에서 다시 스왑해서 계산했는데, 프리뷰 회전 중에는 ItemSize가
	// 이미 낡은 값이라 아이콘이 이중으로 회전된 것처럼 비율이 깨져 보이는 버그가 있었다.
	FIntPoint NaturalGridSize = FIntPoint(1, 1);

	FVector2D GrabPixelOffset = FVector2D::ZeroVector;
	float CellSize = 64.0f;
	bool bSplitRequested = false;
};
