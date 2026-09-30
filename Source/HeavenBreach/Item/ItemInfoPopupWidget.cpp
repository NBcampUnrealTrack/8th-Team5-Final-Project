#include "Item/ItemInfoPopupWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"

namespace
{
	FText GetGradeDisplayText(EItemGrade InGrade)
	{
		switch (InGrade)
		{
		case EItemGrade::Rare:   return FText::FromString(TEXT("레어"));
		case EItemGrade::Epic:   return FText::FromString(TEXT("에픽"));
		case EItemGrade::Legend: return FText::FromString(TEXT("전설"));
		case EItemGrade::Normal:
		default:                 return FText::FromString(TEXT("일반"));
		}
	}

	// 등급 색상은 더 이상 이 파일에서 따로 정의하지 않고, 인벤토리 슬롯/장비창에서
	// 이미 쓰고 있는 GetGradeTopColor / GetGradeBottomColor(InventoryTypes.h)를 그대로 재사용한다.
	// 이렇게 해야 팝업의 톤이 인벤토리 칸/장비창 색깔과 정확히 맞아떨어진다.
	FLinearColor GetGradeColor(EItemGrade InGrade)
	{
		return GetGradeTopColor(InGrade);
	}

	FLinearColor GetGradeBadgeColor(EItemGrade InGrade)
	{
		return GetGradeBottomColor(InGrade);
	}

	// "로아 톤앤매너" 시안(HeavenBreach 아이템 툴팁 시안)에서 가져온 고정 색상값.
	// 등급색(GetGradeColor 등)은 그대로 두고, 시안에 있던 나머지 고정 색상만 여기서 관리한다.
	FLinearColor HexColor(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor(FColor(R, G, B, 255));
	}

	const FLinearColor RelicBenefitColor = HexColor(0x7C, 0xD9, 0x92); // #7CD992 — 이점(초록)
	const FLinearColor RelicRiskColor    = HexColor(0xE2, 0x68, 0x5F); // #E2685F — 위험(빨강)
	const FLinearColor SellValueColor    = HexColor(0xE8, 0xC4, 0x68); // #E8C468 — 판매가(골드)
	const FLinearColor SectionLabelColor = HexColor(0x8B, 0x84, 0x78); // #8B8478 — 섹션 라벨/부제(무채 톤)
	const FLinearColor MutedTextColor    = HexColor(0x6E, 0x68, 0x5E); // #6E685E — 설명/플레이스홀더(어두운 무채 톤)
	const FLinearColor DurabilityColor   = HexColor(0xB9, 0xB2, 0xA6); // #B9B2A6 — 내구도

	// 아이템 이름 전용 고정 색상. 예전엔 이름 텍스트에도 등급색(GetGradeColor)을 그대로
	// 써서 이름이 등급마다 쨍하게 물들어 있었는데, 이제는 이름은 "그냥 무채색"으로 고정하고
	// 등급 구분은 배지(GradeBadgeBorder)/아이콘 테두리(IconBorder)/카드 링 색으로만 표현한다.
	// 배경(PanelBackground)이 거의 검은색이라 이름 자체를 진짜 검은색으로 두면 안 보이니,
	// "무채색으로 튀지 않는다"는 의도를 살려 밝은 오프화이트로 고정한다.
	const FLinearColor NameTextColor = HexColor(0xF0, 0xEC, 0xE2); // #F0ECE2 — 아이템 이름(등급 무관 고정)

	// 텍스트 블록의 기존 폰트(에셋/타입페이스)는 그대로 두고 크기만 바꿔서, 시안처럼
	// 이름 > 스탯/판매가 > 라벨/설명 순으로 위계가 드러나게 한다. Designer에서 폰트를
	// 하나하나 바꿀 필요 없이 코드 한 곳에서 전체 크기 밸런스를 관리할 수 있다.
	void ApplyFontSize(UTextBlock* InTextBlock, int32 InSize)
	{
		if (InTextBlock == nullptr)
		{
			return;
		}
		FSlateFontInfo Font = InTextBlock->GetFont();
		Font.Size = InSize;
		InTextBlock->SetFont(Font);
	}
}

void UItemInfoPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UItemInfoPopupWidget::NativeOnFocusLost(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnFocusLost(InFocusEvent);
	if (!HasFocusedDescendants())
	{
		RemoveFromParent();
	}
}

FReply UItemInfoPopupWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		RemoveFromParent();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UItemInfoPopupWidget::SetupInfo(const FString& InDisplayName, EItemGrade InGrade, const FString& InDescription,
    const FString& InBenefitDescription, const FString& InRiskDescription,
    int32 InSellValue, UTexture2D* InIcon, const FString& InStatText, bool bInCompact,
    const FString& InEquipSlotLabel, float InCurrentDurability, float InMaxDurability)
{
    const FLinearColor GradeColor = GetGradeColor(InGrade);
    const FText GradeDisplayText = GetGradeDisplayText(InGrade);

    if (NameText != nullptr)
    {
        NameText->SetText(FText::FromString(InDisplayName));
        NameText->SetColorAndOpacity(FSlateColor(NameTextColor));
        ApplyFontSize(NameText, bInCompact ? 16 : 20);
    }

    if (GradeText != nullptr)
    {
        GradeText->SetText(GradeDisplayText);
        GradeText->SetColorAndOpacity(FSlateColor(GradeColor));
    }

    if (GradeBadgeBorder != nullptr)
    {
        GradeBadgeBorder->SetBrushColor(GetGradeBadgeColor(InGrade));
    }

    if (IconBorder != nullptr)
    {
        IconBorder->SetBrushColor(GradeColor);
    }

    // PanelBorder는 이제 순수 레이아웃용 여백 래퍼로만 쓴다 — 실제 "카드" 비주얼(모서리 둥글게 +
    // 등급색 테두리 링 + 다크 배경)은 전부 PanelBackground의 RoundedBox 브러시가 담당한다.
    // (예전에는 PanelBorder를 색칠하고 PanelBackground를 안쪽에 겹쳐서 "링처럼 보이게" 하는
    // 방식이었는데, 사각형 브러시라 모서리가 각지고 은은한 느낌이 안 살았다.)
    if (PanelBorder != nullptr)
    {
        // UBorder에는 (5.7 기준) 기존 브러시를 읽어오는 Getter가 없어서, 기존 Draw
        // 타입/이미지는 그대로 둔 채 SetBrushColor로 알파만 0으로 낮춰 투명하게 만든다.
        PanelBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
    }

    // 안쪽 배경: 시안(그리고 실제 인벤토리 장비창의 등급 틴트 방식)을 보면 배경 자체를
    // 등급색으로 칠하는 게 아니라, 배경은 등급과 무관하게 거의 항상 같은 짙은 다크 톤을
    // 유지하고 "등급 신호"는 모서리가 둥근 테두리 링 쪽에서 확실히 드러난다.
    // (장비창 EquipmentSlotWidget도 등급색을 Alpha 0.35짜리 옅은 틴트로만 얹지, 꽉 채운 색으로
    // 쓰지 않는다 — 같은 원리.) 배경 채우기엔 등급색을 6%만 섞어 은은한 힌트만 남기고,
    // RoundedBox의 OutlineSettings로 등급색 테두리 링 + 둥근 모서리를 한 번에 그린다.
    if (PanelBackground != nullptr)
    {
        const FLinearColor NearBlackBase = HexColor(0x17, 0x14, 0x11); // 시안의 다크 브라운/블랙 배경톤
        FLinearColor PanelBgColor = FMath::Lerp(NearBlackBase, GetGradeBadgeColor(InGrade), 0.06f);
        PanelBgColor.A = 1.0f;

        // 마찬가지로 GetBrush()가 없어서, 기존 브러시를 이어받는 대신 새 FSlateBrush를
        // 처음부터 구성한다. RoundedBox는 텍스처 없이도 그려지는 브러시라 문제 없다.
        FSlateBrush RoundedBrush;
        RoundedBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
        RoundedBrush.TintColor = FSlateColor(PanelBgColor);
        RoundedBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        RoundedBrush.OutlineSettings.CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);
        RoundedBrush.OutlineSettings.Color = FSlateColor(GradeColor);
        RoundedBrush.OutlineSettings.Width = 2.f;
        RoundedBrush.OutlineSettings.bUseBrushTransparency = false;
        PanelBackground->SetBrush(RoundedBrush);
    }

    // 팝업 전체 가로폭: 시안 기준 — 간편(호버) 380 / 상세 400.
    if (PopupSizeBox != nullptr)
    {
        PopupSizeBox->SetWidthOverride(bInCompact ? 380.f : 400.f);
    }

    // 서브타이틀: "등급 · 부위" — 상세 팝업에서만, 호버에서는 숨김
    if (SubtitleText != nullptr)
    {
        const FString GradeName = GradeDisplayText.ToString();
        FString SubtitleString;
        if (!GradeName.IsEmpty() && !InEquipSlotLabel.IsEmpty())
        {
            SubtitleString = FString::Printf(TEXT("%s 등급 · %s"), *GradeName, *InEquipSlotLabel);
        }
        else
        {
            SubtitleString = GradeName.IsEmpty() ? InEquipSlotLabel : GradeName;
        }
        SubtitleText->SetText(FText::FromString(SubtitleString));
        // "에픽 등급 · 머리"처럼 등급을 직접 언급하는 줄이라, 무채색 라벨 톤(SectionLabelColor)
        // 대신 등급색(GradeColor)을 써서 등급이 한눈에 보이게 한다.
        SubtitleText->SetColorAndOpacity(FSlateColor(GradeColor));
        SubtitleText->SetVisibility((bInCompact || SubtitleString.IsEmpty()) ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
        ApplyFontSize(SubtitleText, 12);
    }

    if (StatText != nullptr)
    {
        StatText->SetText(FText::FromString(InStatText));
        StatText->SetVisibility(InStatText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
        ApplyFontSize(StatText, 14);
    }
    if (StatSectionHeader != nullptr)
    {
        const bool bShowStat = (!bInCompact && !InStatText.IsEmpty());
        StatSectionHeader->SetVisibility(bShowStat ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (StatSectionLabel != nullptr)
        {
            StatSectionLabel->SetColorAndOpacity(FSlateColor(SectionLabelColor));
            StatSectionLabel->SetVisibility(bShowStat ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            ApplyFontSize(StatSectionLabel, 12);
        }
        if (StatDivider != nullptr)
        {
            StatDivider->SetVisibility(bShowStat ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }
    }

    // 유물이 악세서리로 통합돼서 bIsRelic 로 가르지 않는다. 설명이 있으면 보인다.
    if (BenefitText != nullptr)
    {
        if (!bInCompact && !InBenefitDescription.IsEmpty())
        {
            BenefitText->SetText(FText::FromString(InBenefitDescription));
            BenefitText->SetColorAndOpacity(FSlateColor(RelicBenefitColor));
            BenefitText->SetVisibility(ESlateVisibility::Visible);
            ApplyFontSize(BenefitText, 14);
        }
        else
        {
            BenefitText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    if (RiskText != nullptr)
    {
        if (!bInCompact && !InRiskDescription.IsEmpty())
        {
            RiskText->SetText(FText::FromString(InRiskDescription));
            RiskText->SetColorAndOpacity(FSlateColor(RelicRiskColor));
            RiskText->SetVisibility(ESlateVisibility::Visible);
            ApplyFontSize(RiskText, 14);
        }
        else
        {
            RiskText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    // 장비 슬롯이 있는 아이템(장비류/악세사리)인지 여부 — 잡템은 EquipSlotLabel이 비어있다.
    const bool bIsEquipment = !InEquipSlotLabel.IsEmpty();
    const bool bHasRelicEffect = !InBenefitDescription.IsEmpty() || !InRiskDescription.IsEmpty();

    if (RelicSectionHeader != nullptr)
    {
        // 옵션 · 각인 줄이 있을 때만 섹션을 보인다.
        // 예전엔 각인이 없어서 장비면 "미정" 자리표시를 띄웠다. 각인이 생긴 뒤로는
        // 일반 등급처럼 각인을 안 받는 장비에 "추후 업데이트 예정" 이 떠서 사실과 달라졌다.
        const bool bShowRelicSection = !bInCompact && bHasRelicEffect;
        RelicSectionHeader->SetVisibility(bShowRelicSection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (RelicSectionLabel != nullptr)
        {
            RelicSectionLabel->SetColorAndOpacity(FSlateColor(SectionLabelColor));
            RelicSectionLabel->SetVisibility(bShowRelicSection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            ApplyFontSize(RelicSectionLabel, 12);
        }
        if (RelicDivider != nullptr)
        {
            RelicDivider->SetVisibility(bShowRelicSection ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        }

        if (RelicPlaceholderText != nullptr)
        {
            // 자리표시는 더 안 쓴다. WBP 에 위젯이 남아 있어서 숨기기만 한다.
            RelicPlaceholderText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // 설명은 장비/악세사리에서만 보여준다 — 잡템은 판매가만.
    const bool bShowDescription = !bInCompact && bIsEquipment && !InDescription.IsEmpty();
    if (DescriptionText != nullptr)
    {
        DescriptionText->SetText(FText::FromString(InDescription));
        DescriptionText->SetColorAndOpacity(FSlateColor(MutedTextColor));
        DescriptionText->SetVisibility(bShowDescription ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        ApplyFontSize(DescriptionText, 12);
    }
    if (DescriptionDivider != nullptr)
    {
        DescriptionDivider->SetVisibility(bShowDescription ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (SellValueText != nullptr)
    {
        SellValueText->SetText(FText::FromString(FString::Printf(TEXT("판매가: %d"), InSellValue)));
        SellValueText->SetColorAndOpacity(FSlateColor(SellValueColor));
        ApplyFontSize(SellValueText, 15);
    }

    if (DurabilityText != nullptr)
    {
        const bool bHasDurability = bIsEquipment && (InCurrentDurability >= 0.f && InMaxDurability > 0.f);
        if (bHasDurability)
        {
            DurabilityText->SetText(FText::FromString(FString::Printf(TEXT("내구도: %d / %d"),
                FMath::RoundToInt(InCurrentDurability), FMath::RoundToInt(InMaxDurability))));
            DurabilityText->SetColorAndOpacity(FSlateColor(DurabilityColor));
            ApplyFontSize(DurabilityText, 13);
        }
        DurabilityText->SetVisibility((bInCompact || !bHasDurability) ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }
    if (FooterDivider != nullptr)
    {
        FooterDivider->SetVisibility(bInCompact ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }

    if (IconImage != nullptr)
    {
        IconImage->SetBrushFromTexture(InIcon);
    }
}