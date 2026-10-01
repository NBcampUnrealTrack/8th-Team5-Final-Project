#include "UI/HUD/HB_SpectateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

TSharedRef<SWidget> UHB_SpectateWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		Label->SetJustification(ETextJustify::Center);

		UCanvasPanelSlot* LabelSlot = Root->AddChildToCanvas(Label);
		LabelSlot->SetAnchors(FAnchors(0.5f, 0.85f));
		LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LabelSlot->SetAutoSize(true);
	}

	return Super::RebuildWidget();
}

void UHB_SpectateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!Label)
	{
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Target = PC ? Cast<APawn>(PC->GetViewTarget()) : nullptr;
	const APlayerState* TargetState = Target ? Target->GetPlayerState() : nullptr;

	const FString Name = TargetState ? TargetState->GetPlayerName() : TEXT("-");
	Label->SetText(FText::FromString(FString::Printf(TEXT("관전 중: %s\n좌클릭: 다음 팀원"), *Name)));
}
