#include "UI/Map/HB_MapWindow.h"
#include "UI/Map/HB_MapLevelEntry.h"
#include "UI/HUD/HBRoomNameSubsystem.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"

namespace HBMap
{
	struct FLevelDef
	{
		const TCHAR* Code;
		int32 Tier;
		const TCHAR* Korean;
	};

	const FLevelDef LevelDefs[] = {
		{ TEXT("Adventure1-1"), 1, TEXT("덩굴 대전") },
		{ TEXT("Adventure1-2"), 1, TEXT("끊어진 다리") },
		{ TEXT("Adventure1-3"), 1, TEXT("쌍둥이 회랑") },
		{ TEXT("Adventure1-4"), 1, TEXT("계단 테라스") },
		{ TEXT("Adventure1-5"), 1, TEXT("무너진 신전") },
		{ TEXT("Adventure1-6"), 1, TEXT("봉인된 방") },
		{ TEXT("Adventure1-7"), 1, TEXT("부서진 뜰") },
		{ TEXT("Adventure1-8"), 1, TEXT("허물어진 성벽") },
		{ TEXT("Adventure1-9"), 1, TEXT("탑의 안뜰") },
		{ TEXT("Adventure1-10"), 1, TEXT("망루 마당") },
		{ TEXT("Adventure1-11"), 1, TEXT("잊힌 정원") },
		{ TEXT("Adventure2-1"), 2, TEXT("감옥") },
		{ TEXT("Adventure2-2"), 2, TEXT("가라앉은 저수조") },
		{ TEXT("Adventure2-3"), 2, TEXT("해골 묘실") },
		{ TEXT("Adventure2-4"), 2, TEXT("잊힌 서고") },
		{ TEXT("Adventure2-5"), 2, TEXT("철의 대장간") },
		{ TEXT("Adventure2-6"), 2, TEXT("잠긴 회랑") },
		{ TEXT("Adventure2-7"), 2, TEXT("텅 빈 성소") },
		{ TEXT("Adventure2-8"), 2, TEXT("납골당") },
		{ TEXT("Adventure2-9"), 2, TEXT("지하 묘지") },
		{ TEXT("Adventure2-10"), 2, TEXT("심연의 계단") },
	};

	FString TextureKey(FName Code)
	{
		return Code.ToString().Replace(TEXT("-"), TEXT("_"));
	}
}

UHB_MapWindow::UHB_MapWindow()
{
	InputMode = EHBWidgetInputMode::Menu;
	bIsBackHandler = true;
}

void UHB_MapWindow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);

	if (Btn_Tier1) Btn_Tier1->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleTier1);
	if (Btn_Tier2) Btn_Tier2->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleTier2);
	if (Btn_Layer1) Btn_Layer1->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleLayer1);
	if (Btn_Layer2) Btn_Layer2->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleLayer2);
	if (Btn_Layer3) Btn_Layer3->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleLayer3);
	if (Btn_ShowIso) Btn_ShowIso->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleShowIso);
	if (Btn_HideIso) Btn_HideIso->OnClicked.AddUniqueDynamic(this, &UHB_MapWindow::HandleHideIso);

	Levels.Reset();
	for (const HBMap::FLevelDef& Def : HBMap::LevelDefs)
	{
		FLevelInfo Info;
		Info.Code = FName(Def.Code);
		Info.Tier = Def.Tier;
		Info.KoreanName = FText::FromString(Def.Korean);
		Info.LayerCount = 0;
		for (int32 k = 1; k <= 3; ++k)
		{
			if (LoadMapTexture(TEXT("Plan"), FString::Printf(TEXT("T_Map_%s_F%d"), *HBMap::TextureKey(Info.Code), k)))
			{
				Info.LayerCount = k;
			}
		}
		Levels.Add(Info);
	}
	BuildEntries();
}

void UHB_MapWindow::NativeOnActivated()
{
	Super::NativeOnActivated();
	CollectRunState();
	SelectedLayer = 0;
	bIsoShown = false;
	Refresh();
	SetUserFocus(GetOwningPlayer());
}

FReply UHB_MapWindow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::M)
	{
		DeactivateWidget();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FName UHB_MapWindow::GetSelectedLevel() const
{
	return Levels.IsValidIndex(SelectedLevel) ? Levels[SelectedLevel].Code : NAME_None;
}

void UHB_MapWindow::BuildEntries()
{
	Entries.Reset();
	if (!List_Levels || !LevelEntryClass)
	{
		return;
	}
	List_Levels->ClearChildren();
	for (int32 i = 0; i < Levels.Num(); ++i)
	{
		UHB_MapLevelEntry* Entry = CreateWidget<UHB_MapLevelEntry>(this, LevelEntryClass);
		if (!Entry)
		{
			continue;
		}
		Entry->Setup(i, Levels[i].Code, Levels[i].KoreanName, FHBMapEntryClicked::CreateUObject(this, &UHB_MapWindow::OnEntryClicked));
		List_Levels->AddChild(Entry);
		Entries.Add(Entry);
	}
}

UTexture2D* UHB_MapWindow::LoadMapTexture(const FString& Folder, const FString& AssetName)
{
	if (TObjectPtr<UTexture2D>* Found = TextureCache.Find(AssetName))
	{
		return *Found;
	}
	const FString Package = FString::Printf(TEXT("/Game/UI/Map/%s/%s"), *Folder, *AssetName);
	UTexture2D* Texture = nullptr;
	if (FPackageName::DoesPackageExist(Package))
	{
		Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *(Package + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet));
	}
	TextureCache.Add(AssetName, Texture);
	return Texture;
}

void UHB_MapWindow::CollectRunState()
{
	GeneratedLevels.Reset();
	FName CurrentLevel = NAME_None;

	AHBDungeonRuntime* Dungeon = nullptr;
	for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It)
	{
		Dungeon = *It;
		break;
	}
	const APawn* Pawn = GetOwningPlayerPawn();
	if (Dungeon && Dungeon->IsReady())
	{
		for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
		{
			if (Room.bCorridor)
			{
				continue;
			}
			const FName MapName(*Room.ModuleMap.GetAssetName());
			GeneratedLevels.Add(MapName);
			if (Pawn && Room.Bounds.IsValid && Room.Bounds.IsInsideOrOn(Pawn->GetActorLocation()))
			{
				CurrentLevel = MapName;
			}
		}
	}

	int32 Pick = Levels.IndexOfByPredicate([&](const FLevelInfo& L) { return L.Code == CurrentLevel; });
	if (Pick == INDEX_NONE)
	{
		Pick = Levels.IndexOfByPredicate([&](const FLevelInfo& L) { return GeneratedLevels.Contains(L.Code); });
	}
	SelectedLevel = Levels.IsValidIndex(Pick) ? Pick : 0;
	SelectedTier = Levels.IsValidIndex(SelectedLevel) ? Levels[SelectedLevel].Tier : 1;
}

void UHB_MapWindow::TintButton(UButton* Button, bool bSelected, bool bVisible)
{
	if (!Button)
	{
		return;
	}
	Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button->SetBackgroundColor(bSelected ? SelectedTint : NormalTint);
}

void UHB_MapWindow::Refresh()
{
	TintButton(Btn_Tier1, SelectedTier == 1);
	TintButton(Btn_Tier2, SelectedTier == 2);

	for (UHB_MapLevelEntry* Entry : Entries)
	{
		const int32 Index = Levels.IndexOfByPredicate([&](const FLevelInfo& L) { return L.Code == Entry->GetLevelCode(); });
		if (!Levels.IsValidIndex(Index))
		{
			continue;
		}
		Entry->SetVisibility(Levels[Index].Tier == SelectedTier ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Entry->SetState(GeneratedLevels.Contains(Levels[Index].Code), Index == SelectedLevel);
	}

	if (Levels.IsValidIndex(SelectedLevel))
	{
		const FLevelInfo& Sel = Levels[SelectedLevel];
		SelectedLayer = FMath::Clamp(SelectedLayer, 0, FMath::Max(Sel.LayerCount - 1, 0));

		if (Text_LevelName) Text_LevelName->SetText(Sel.KoreanName);
		if (Text_LevelEnglish) Text_LevelEnglish->SetText(UHBRoomNameSubsystem::GetRoomDisplayName(Sel.Code));

		UButton* LayerButtons[] = { Btn_Layer1, Btn_Layer2, Btn_Layer3 };
		for (int32 k = 0; k < 3; ++k)
		{
			TintButton(LayerButtons[k], k == SelectedLayer, Sel.LayerCount > 1 && k < Sel.LayerCount);
		}

		const FString Key = HBMap::TextureKey(Sel.Code);
		if (Img_Plan)
		{
			UTexture2D* Plan = LoadMapTexture(TEXT("Plan"), FString::Printf(TEXT("T_Map_%s_F%d"), *Key, SelectedLayer + 1));
			Img_Plan->SetBrushFromTexture(Plan, false);
			Img_Plan->SetVisibility(Plan ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (Img_Iso)
		{
			UTexture2D* Iso = LoadMapTexture(TEXT("Iso"), FString::Printf(TEXT("T_Map_%s_ISO"), *Key));
			Img_Iso->SetBrushFromTexture(Iso, false);
		}
	}

	if (Panel_IsoShown) Panel_IsoShown->SetVisibility(bIsoShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Panel_IsoHidden) Panel_IsoHidden->SetVisibility(bIsoShown ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

	OnMapRefreshed();
}

void UHB_MapWindow::SelectTier(int32 Tier)
{
	if (SelectedTier == Tier)
	{
		return;
	}
	SelectedTier = Tier;
	int32 Pick = Levels.IndexOfByPredicate([&](const FLevelInfo& L) { return L.Tier == Tier && GeneratedLevels.Contains(L.Code); });
	if (Pick == INDEX_NONE)
	{
		Pick = Levels.IndexOfByPredicate([&](const FLevelInfo& L) { return L.Tier == Tier; });
	}
	if (Pick != INDEX_NONE)
	{
		SelectedLevel = Pick;
	}
	SelectedLayer = 0;
	Refresh();
}

void UHB_MapWindow::SelectLevel(FName LevelCode)
{
	const int32 Index = Levels.IndexOfByPredicate([&](const FLevelInfo& L) { return L.Code == LevelCode; });
	if (Index != INDEX_NONE)
	{
		OnEntryClicked(Index);
	}
}

void UHB_MapWindow::OnEntryClicked(int32 Index)
{
	SelectedLevel = Index;
	SelectedLayer = 0;
	Refresh();
}

void UHB_MapWindow::SelectLayer(int32 LayerIndex)
{
	SelectedLayer = LayerIndex;
	Refresh();
}

void UHB_MapWindow::SetIsoShown(bool bShow)
{
	bIsoShown = bShow;
	Refresh();
}

void UHB_MapWindow::HandleTier1() { SelectTier(1); }
void UHB_MapWindow::HandleTier2() { SelectTier(2); }
void UHB_MapWindow::HandleLayer1() { SelectLayer(0); }
void UHB_MapWindow::HandleLayer2() { SelectLayer(1); }
void UHB_MapWindow::HandleLayer3() { SelectLayer(2); }
void UHB_MapWindow::HandleShowIso() { SetIsoShown(true); }
void UHB_MapWindow::HandleHideIso() { SetIsoShown(false); }
