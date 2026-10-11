#include "UI/HUD/HB_SpectateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Player/HBPlayerController.h"

TSharedRef<SWidget> UHB_SpectateWidget::RebuildWidget()
{
	// 디자인 WBP 는 이미 루트가 있어서 여기를 지나간다. 루트가 없을 때(C++ 클래스를 그대로 쓸 때)만 만든다.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		FallbackLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label"));
		FallbackLabel->SetJustification(ETextJustify::Center);

		UCanvasPanelSlot* LabelSlot = Root->AddChildToCanvas(FallbackLabel);
		LabelSlot->SetAnchors(FAnchors(0.5f, 0.85f));
		LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LabelSlot->SetAutoSize(true);
	}

	return Super::RebuildWidget();
}

void UHB_SpectateWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Prev)
	{
		Button_Prev->OnClicked.AddUniqueDynamic(this, &UHB_SpectateWidget::HandlePrevClicked);
	}
	if (Button_Next)
	{
		Button_Next->OnClicked.AddUniqueDynamic(this, &UHB_SpectateWidget::HandleNextClicked);
	}
}

void UHB_SpectateWidget::HandlePrevClicked()
{
	if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
	{
		PC->RequestSpectatePrevious();
	}
}

void UHB_SpectateWidget::HandleNextClicked()
{
	if (AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer()))
	{
		PC->RequestSpectateNext();
	}
}

void UHB_SpectateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AHBPlayerController* PC = Cast<AHBPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	const APawn* Target = Cast<APawn>(PC->GetViewTarget());

	// 보고 있는 순번과 관전 가능한 인원. 컨트롤러가 다음 대상을 고르는 것과 같은 목록이다.
	TArray<APawn*> Candidates;
	PC->GetSpectateCandidates(Candidates);
	const int32 Index = Candidates.IndexOfByKey(Target);
	const int32 Count = Candidates.Num();

	// 바뀐 게 없으면 글자를 다시 쓰지 않는다.
	if (Target == LastTarget.Get() && Index == LastIndex && Count == LastCount)
	{
		return;
	}

	const bool bTargetChanged = Target != LastTarget.Get();
	LastTarget = Target;
	LastIndex = Index;
	LastCount = Count;

	const APlayerState* TargetState = Target ? Target->GetPlayerState() : nullptr;
	const FString NameString = TargetState ? TargetState->GetPlayerName() : TEXT("-");
	const FText Name = FText::FromString(NameString);

	const FText Counter = (Index != INDEX_NONE)
		? FText::Format(NSLOCTEXT("Spectate", "Counter", "{0} / {1}"), FText::AsNumber(Index + 1), FText::AsNumber(Count))
		: FText::FromString(TEXT("-"));

	if (Text_TargetName)
	{
		Text_TargetName->SetText(Name);
	}
	if (Text_Counter)
	{
		Text_Counter->SetText(Counter);
	}

	// 넘어갈 사람이 없으면(혼자 남았거나 0명) 버튼을 막는다. 눌러도 같은 사람이 나올 뿐이다.
	const bool bCanSwitch = Count > 1;
	if (Button_Prev)
	{
		Button_Prev->SetIsEnabled(bCanSwitch);
	}
	if (Button_Next)
	{
		Button_Next->SetIsEnabled(bCanSwitch);
	}

	// 디자인 WBP 가 없을 때의 기본 표시. 예전과 같은 글자다.
	if (FallbackLabel)
	{
		FallbackLabel->SetText(FText::FromString(FString::Printf(TEXT("관전 중: %s\n좌클릭: 다음 팀원"), *NameString)));
	}

	if (bTargetChanged)
	{
		OnSpectateTargetChanged(Name);
	}
}
