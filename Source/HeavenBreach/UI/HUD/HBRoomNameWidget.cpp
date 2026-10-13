#include "UI/HUD/HBRoomNameWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UHBRoomNameWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RoomNameRoot"));
		WidgetTree->RootWidget = Root;

		NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RoomNameText"));
		FSlateFontInfo Font = NameText->GetFont();
		Font.Size = FontSize;
		Font.LetterSpacing = 180;
		Font.OutlineSettings.OutlineSize = 2;
		Font.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		NameText->SetFont(Font);
		NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.84f, 0.6f, 1.f)));
		NameText->SetShadowOffset(FVector2D(2.f, 2.f));
		NameText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		NameText->SetJustification(ETextJustify::Center);

		if (UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(NameText))
		{
			CanvasSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.f));
			CanvasSlot->SetPosition(FVector2D(0.f, TopOffset));
			CanvasSlot->SetAutoSize(true);
		}
	}
	return Super::RebuildWidget();
}

void UHBRoomNameWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyAlpha(0.f);
}

void UHBRoomNameWidget::ShowRoomName(const FText& InName)
{
	if (!NameText || InName.IsEmpty())
	{
		return;
	}
	NameText->SetText(InName);
	const float Total = FadeInSeconds + HoldSeconds + FadeOutSeconds;
	if (bPlaying && Elapsed > FadeInSeconds && Elapsed < Total)
	{
		Elapsed = FadeInSeconds * (NameText->GetRenderOpacity());
	}
	else
	{
		Elapsed = 0.f;
	}
	bPlaying = true;
}

void UHBRoomNameWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bPlaying)
	{
		return;
	}
	Elapsed += InDeltaTime;
	float Alpha = 0.f;
	if (Elapsed < FadeInSeconds)
	{
		Alpha = Elapsed / FMath::Max(FadeInSeconds, KINDA_SMALL_NUMBER);
	}
	else if (Elapsed < FadeInSeconds + HoldSeconds)
	{
		Alpha = 1.f;
	}
	else if (Elapsed < FadeInSeconds + HoldSeconds + FadeOutSeconds)
	{
		Alpha = 1.f - (Elapsed - FadeInSeconds - HoldSeconds) / FMath::Max(FadeOutSeconds, KINDA_SMALL_NUMBER);
	}
	else
	{
		bPlaying = false;
	}
	ApplyAlpha(FMath::SmoothStep(0.f, 1.f, Alpha));
}

void UHBRoomNameWidget::ApplyAlpha(float Alpha)
{
	if (NameText)
	{
		NameText->SetRenderOpacity(Alpha);
	}
}
