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

	// 드래그 중 T키로 회전을 토글했을 때, 이 위젯이 드래그 미리보기(UMG DefaultDragVisual)로
	// 쓰이고 있는 복제본이라면 아이콘 회전만 갱신한다. InitializeItem 전체를 다시 부르지 않고
	// 아이콘 크기/회전 각도만 바꿔서 드래그 도중 매번 다시 세팅하는 비용을 피한다.
	void SetPreviewRotated(bool bInRotated);

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

	// 아이콘 회전과 무관하게 항상 칸 하단에 고정되어야 해서 SizeBox_Icon 밖(Overlay 직속)에 둔다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UProgressBar> ProgressBar_Durability;

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

	// 파손된 장비류 아이템에만 보이는 텍스트. UMG에서 위젯을 지워서 지금은 안 씀 ?
	// 바인딩은 남겨둬도 무해하다(매칭되는 위젯 없으면 nullptr).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Broken;

	// 파손 시 아이콘 뒤에 깔리는 반투명 빨간 배경. UMG에 같은 이름의 Image를
	// 추가하면 자동 바인딩된다(없어도 에러 안 남).
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_BrokenBG;

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
