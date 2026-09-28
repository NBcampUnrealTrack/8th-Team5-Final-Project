#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/InventoryTypes.h"
#include "ItemInfoPopupWidget.generated.h"

class UTextBlock;
class UImage;
class UBorder;
class USizeBox;

UCLASS()
class HEAVENBREACH_API UItemInfoPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// InBenefitDescription / InRiskDescription 은 초록 / 빨강 칸에 그대로 찍힌다. 비어 있으면 칸이 숨는다.
	// 유물이 악세서리로 통합되어 bIsRelic 인자는 뺐다. 옵션 문장은 ItemStats::FormatLines 가 만든다.
	void SetupInfo(const FString& InDisplayName, EItemGrade InGrade, const FString& InDescription,
		const FString& InBenefitDescription, const FString& InRiskDescription,
		int32 InSellValue, UTexture2D* InIcon, const FString& InStatText = FString(),
		bool bInCompact = false, const FString& InEquipSlotLabel = FString(),
		float InCurrentDurability = -1.f, float InMaxDurability = -1.f);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GradeText;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GradeBadgeBorder;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> IconBorder;

	// 팝업 전체를 감싸는 바깥쪽 테두리 — 등급색으로 물들여서 인벤토리 슬롯과 톤을 맞춘다.
	// Designer에서 가장 바깥 Border를 이 이름으로 바인딩하면 자동으로 등급색이 적용된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PanelBorder;

	// PanelBorder 안쪽, 실제 내용을 감싸는 어두운 배경 패널 — 등급별 어두운 톤(인벤토리 슬롯 하단색과 동일 계열).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PanelBackground;

	// 팝업 전체 크기(가로폭)를 코드에서 고정하기 위한 SizeBox — 간편/상세 모드에 따라 폭이 달라진다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USizeBox> PopupSizeBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SellValueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> IconImage;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BenefitText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RiskText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatSectionHeader;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatSectionLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StatDivider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RelicSectionHeader;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RelicSectionLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RelicDivider;

	// 각인 시스템 미구현 상태에서, 장비/악세사리에 "각인 효과: 미정" 플레이스홀더를 보여주는 텍스트.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RelicPlaceholderText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DescriptionDivider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FooterDivider;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DurabilityText;
};