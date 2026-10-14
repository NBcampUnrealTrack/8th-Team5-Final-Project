#include "UI/HUD/HBRoomNameSubsystem.h"
#include "UI/HUD/HBRoomNameWidget.h"
#include "PCGModule/HBDungeonRuntime.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	const TMap<FName, FText>& RoomNames()
	{
		static const TMap<FName, FText> Names = {
			{ TEXT("Adventure1-1"), NSLOCTEXT("RoomName", "Adventure1-1", "Overgrown Hall") },
			{ TEXT("Adventure1-2"), NSLOCTEXT("RoomName", "Adventure1-2", "Broken Bridges") },
			{ TEXT("Adventure1-3"), NSLOCTEXT("RoomName", "Adventure1-3", "Twin Galleries") },
			{ TEXT("Adventure1-4"), NSLOCTEXT("RoomName", "Adventure1-4", "Hanging Terraces") },
			{ TEXT("Adventure1-5"), NSLOCTEXT("RoomName", "Adventure1-5", "Ruined Nave") },
			{ TEXT("Adventure1-6"), NSLOCTEXT("RoomName", "Adventure1-6", "Sealed Chambers") },
			{ TEXT("Adventure1-7"), NSLOCTEXT("RoomName", "Adventure1-7", "Shattered Courts") },
			{ TEXT("Adventure1-8"), NSLOCTEXT("RoomName", "Adventure1-8", "Ruined Ramparts") },
			{ TEXT("Adventure1-9"), NSLOCTEXT("RoomName", "Adventure1-9", "Tower Courtyard") },
			{ TEXT("Adventure1-10"), NSLOCTEXT("RoomName", "Adventure1-10", "Watchtower Yard") },
			{ TEXT("Adventure1-11"), NSLOCTEXT("RoomName", "Adventure1-11", "Fallen Garden") },
			{ TEXT("Adventure2-1"), NSLOCTEXT("RoomName", "Adventure2-1", "Prison Cells") },
			{ TEXT("Adventure2-2"), NSLOCTEXT("RoomName", "Adventure2-2", "Sunken Cistern") },
			{ TEXT("Adventure2-3"), NSLOCTEXT("RoomName", "Adventure2-3", "Bone Crypt") },
			{ TEXT("Adventure2-4"), NSLOCTEXT("RoomName", "Adventure2-4", "Forgotten Archive") },
			{ TEXT("Adventure2-5"), NSLOCTEXT("RoomName", "Adventure2-5", "Iron Forge") },
			{ TEXT("Adventure2-6"), NSLOCTEXT("RoomName", "Adventure2-6", "Flooded Halls") },
			{ TEXT("Adventure2-7"), NSLOCTEXT("RoomName", "Adventure2-7", "Hollow Shrine") },
			{ TEXT("Adventure2-8"), NSLOCTEXT("RoomName", "Adventure2-8", "Ossuary") },
			{ TEXT("Adventure2-9"), NSLOCTEXT("RoomName", "Adventure2-9", "Catacombs") },
			{ TEXT("Adventure2-10"), NSLOCTEXT("RoomName", "Adventure2-10", "Abyssal Stair") },
			{ TEXT("Boss"), NSLOCTEXT("RoomName", "Boss", "Throne of the Deep") },
			{ TEXT("Passage"), NSLOCTEXT("RoomName", "Passage", "The Descent") },
			{ TEXT("Heal_1Ways"), NSLOCTEXT("RoomName", "Heal_1Ways", "Healing Spring") },
			{ TEXT("Heal_2Ways"), NSLOCTEXT("RoomName", "Heal_2Ways", "Quiet Sanctuary") },
			{ TEXT("Heal_3Ways"), NSLOCTEXT("RoomName", "Heal_3Ways", "Pilgrim's Rest") },
			{ TEXT("Enhancement_2Ways"), NSLOCTEXT("RoomName", "Enhancement_2Ways", "Runesmith's Altar") },
		};
		return Names;
	}

	const TMap<FName, FText>& KoreanRoomNames()
	{
		static const TMap<FName, FText> Names = {
			{ TEXT("Adventure1-1"), NSLOCTEXT("RoomNameKo", "Adventure1-1", "덩굴 대전") },
			{ TEXT("Adventure1-2"), NSLOCTEXT("RoomNameKo", "Adventure1-2", "끊어진 다리") },
			{ TEXT("Adventure1-3"), NSLOCTEXT("RoomNameKo", "Adventure1-3", "쌍둥이 회랑") },
			{ TEXT("Adventure1-4"), NSLOCTEXT("RoomNameKo", "Adventure1-4", "계단 테라스") },
			{ TEXT("Adventure1-5"), NSLOCTEXT("RoomNameKo", "Adventure1-5", "무너진 신전") },
			{ TEXT("Adventure1-6"), NSLOCTEXT("RoomNameKo", "Adventure1-6", "봉인된 방") },
			{ TEXT("Adventure1-7"), NSLOCTEXT("RoomNameKo", "Adventure1-7", "부서진 뜰") },
			{ TEXT("Adventure1-8"), NSLOCTEXT("RoomNameKo", "Adventure1-8", "허물어진 성벽") },
			{ TEXT("Adventure1-9"), NSLOCTEXT("RoomNameKo", "Adventure1-9", "탑의 안뜰") },
			{ TEXT("Adventure1-10"), NSLOCTEXT("RoomNameKo", "Adventure1-10", "망루 마당") },
			{ TEXT("Adventure1-11"), NSLOCTEXT("RoomNameKo", "Adventure1-11", "잊힌 정원") },
			{ TEXT("Adventure2-1"), NSLOCTEXT("RoomNameKo", "Adventure2-1", "감옥") },
			{ TEXT("Adventure2-2"), NSLOCTEXT("RoomNameKo", "Adventure2-2", "가라앉은 저수조") },
			{ TEXT("Adventure2-3"), NSLOCTEXT("RoomNameKo", "Adventure2-3", "해골 묘실") },
			{ TEXT("Adventure2-4"), NSLOCTEXT("RoomNameKo", "Adventure2-4", "잊힌 서고") },
			{ TEXT("Adventure2-5"), NSLOCTEXT("RoomNameKo", "Adventure2-5", "철의 대장간") },
			{ TEXT("Adventure2-6"), NSLOCTEXT("RoomNameKo", "Adventure2-6", "잠긴 회랑") },
			{ TEXT("Adventure2-7"), NSLOCTEXT("RoomNameKo", "Adventure2-7", "텅 빈 성소") },
			{ TEXT("Adventure2-8"), NSLOCTEXT("RoomNameKo", "Adventure2-8", "납골당") },
			{ TEXT("Adventure2-9"), NSLOCTEXT("RoomNameKo", "Adventure2-9", "지하 묘지") },
			{ TEXT("Adventure2-10"), NSLOCTEXT("RoomNameKo", "Adventure2-10", "심연의 계단") },
			{ TEXT("Boss"), NSLOCTEXT("RoomNameKo", "Boss", "심연의 왕좌") },
			{ TEXT("Passage"), NSLOCTEXT("RoomNameKo", "Passage", "하강로") },
			{ TEXT("Heal_1Ways"), NSLOCTEXT("RoomNameKo", "Heal_1Ways", "치유의 샘") },
			{ TEXT("Heal_2Ways"), NSLOCTEXT("RoomNameKo", "Heal_2Ways", "고요한 안식처") },
			{ TEXT("Heal_3Ways"), NSLOCTEXT("RoomNameKo", "Heal_3Ways", "순례자의 쉼터") },
			{ TEXT("Enhancement_2Ways"), NSLOCTEXT("RoomNameKo", "Enhancement_2Ways", "룬 대장장이의 제단") },
		};
		return Names;
	}

	FText KoreanRoomName(FName MapName)
	{
		if (const FText* Found = KoreanRoomNames().Find(MapName))
		{
			return *Found;
		}
		return UHBRoomNameSubsystem::GetRoomDisplayName(MapName);
	}
}

bool UHBRoomNameSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UHBRoomNameSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(CheckTimer, this, &UHBRoomNameSubsystem::CheckRoom, 0.2f, true);
}

void UHBRoomNameSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	if (Widget.IsValid())
	{
		Widget->RemoveFromParent();
	}
	Super::Deinitialize();
}

FText UHBRoomNameSubsystem::GetRoomDisplayName(FName MapName)
{
	if (const FText* Found = RoomNames().Find(MapName))
	{
		return *Found;
	}
	return FText::GetEmpty();
}

AHBDungeonRuntime* UHBRoomNameSubsystem::FindRuntime()
{
	if (!Runtime.IsValid())
	{
		for (TActorIterator<AHBDungeonRuntime> It(GetWorld()); It; ++It)
		{
			Runtime = *It;
			break;
		}
	}
	return Runtime.Get();
}

void UHBRoomNameSubsystem::CheckRoom()
{
	AHBDungeonRuntime* Dungeon = FindRuntime();
	if (!Dungeon || !Dungeon->IsReady())
	{
		return;
	}
	if (GenerationId != Dungeon->GetGenerationId())
	{
		GenerationId = Dungeon->GetGenerationId();
		CurrentRoomId = INDEX_NONE;
		CurrentRoomName = FText::GetEmpty();
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	const APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return;
	}
	const FVector Location = Pawn->GetActorLocation();

	const FHBPrototypeRoom* Inside = nullptr;
	for (const FHBPrototypeRoom& Room : Dungeon->Rooms)
	{
		if (Room.bCorridor || !Room.Bounds.IsValid || !Room.Bounds.IsInsideOrOn(Location))
		{
			continue;
		}
		if (Room.Id == CurrentRoomId)
		{
			return;
		}
		if (!Inside)
		{
			Inside = &Room;
		}
	}
	if (!Inside)
	{
		return;
	}
	CurrentRoomId = Inside->Id;

	FName MapName = FName(*Inside->ModuleMap.GetAssetName());
	FText Name = KoreanRoomName(MapName);
	if (Name.IsEmpty())
	{
		Name = KoreanRoomName(Inside->ModuleId);
	}
	CurrentRoomName = Name;
	if (Name.IsEmpty())
	{
		return;
	}

	if (!Widget.IsValid())
	{
		UHBRoomNameWidget* NewWidget = CreateWidget<UHBRoomNameWidget>(PC, UHBRoomNameWidget::StaticClass());
		if (!NewWidget)
		{
			return;
		}
		NewWidget->AddToViewport(40);
		Widget = NewWidget;
	}
	Widget->ShowRoomName(Name);
}
