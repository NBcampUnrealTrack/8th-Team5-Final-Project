// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Ability/Magic/HBMagicVFXPreloadSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "NiagaraSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHBMagicVFXPreload, Log, All);

bool UHBMagicVFXPreloadSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UGameInstance* Instance = Cast<UGameInstance>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && Instance && !IsRunningDedicatedServer()
		&& !Instance->IsDedicatedServerInstance();
}

void UHBMagicVFXPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreparationStartedAt = FPlatformTime::Seconds();

	// Match the production CastHand/OrbImpact/ManaExplosion cues and BP_MageOrb.
	// These bounded presentation-only references survive seamless and ordinary map travel.
	const TCHAR* Paths[] = {
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ElementalOrb/NS_Mage_ElementalOrb_Cast.NS_Mage_ElementalOrb_Cast"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ElementalOrb/NS_Mage_ElementalOrb_Projectile.NS_Mage_ElementalOrb_Projectile"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ElementalOrb/NS_Mage_ElementalOrb_Impact.NS_Mage_ElementalOrb_Impact"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ManaExplosion/NS_Mage_ManaExplosion_Cast.NS_Mage_ManaExplosion_Cast"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ManaExplosion/NS_Mage_ManaExplosion_Burst.NS_Mage_ManaExplosion_Burst"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ManaExplosion/NS_Mage_ManaExplosion_Impact.NS_Mage_ManaExplosion_Impact"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ElementalOrb/BP_Test_MageOrbImpactFX.BP_Test_MageOrbImpactFX_C"),
		TEXT("/Game/Levels/test_gm/MageRetarget/VFX/ManaExplosion/BP_Test_MageManaBurstFX.BP_Test_MageManaBurstFX_C"),
		TEXT("/Game/Combat/MagicGAS/Active/BP_MageOrb.BP_MageOrb_C"),
	};
	TArray<FSoftObjectPath> AssetsToLoad;
	for (const TCHAR* Path : Paths)
	{
		AssetsToLoad.Emplace(Path);
	}
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(AssetsToLoad,
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnAssetsLoaded));
	if (!LoadHandle.IsValid())
	{
		bLoadFailed = true;
		UE_LOG(LogHBMagicVFXPreload, Warning, TEXT("Could not request mage VFX preload; existing cue loading remains available."));
	}
}

void UHBMagicVFXPreloadSubsystem::OnAssetsLoaded()
{
	if (!LoadHandle.IsValid())
	{
		return;
	}
	TArray<UObject*> Loaded;
	LoadHandle->GetLoadedAssets(Loaded);
	for (UObject* Asset : Loaded)
	{
		if (!Asset)
		{
			bLoadFailed = true;
			continue;
		}
		ResidentAssets.Add(Asset);
		if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset))
		{
			System->EnsureFullyLoaded();
#if WITH_EDITOR
			// PIE uses lazy Niagara compilation; start it here, not at the first impact.
			System->RequestCompile(false);
#endif
			PendingSystems.Add(System);
		}
	}
	LoadHandle.Reset(); // UPROPERTY references now own the cache, independently of active FX actors.
	if (PendingSystems.Num() != 6 || ResidentAssets.Num() != 9)
	{
		bLoadFailed = true;
		UE_LOG(LogHBMagicVFXPreload, Warning, TEXT("Mage VFX preload incomplete: %d systems / %d assets; expected 6 / 9."), PendingSystems.Num(), ResidentAssets.Num());
	}
	PreparationHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::PollPreparation), 0.1f);
}

bool UHBMagicVFXPreloadSubsystem::PollPreparation(float DeltaSeconds)
{
	for (int32 Index = PendingSystems.Num() - 1; Index >= 0; --Index)
	{
		UNiagaraSystem* System = PendingSystems[Index];
#if WITH_EDITOR
		// The return value means a job completed *this poll*, not that the system is ready.
		// Another engine tick can already have consumed the completed request.
		System->PollForCompilationComplete();
#endif
		if (!System->IsReadyToRun())
		{
			continue;
		}
		System->PrecachePSOs(); // Uses existing project PSO settings; no forced flush or global CVar change.
		PendingSystems.RemoveAtSwap(Index);
	}
	if (PendingSystems.IsEmpty())
	{
		bPreloadComplete = !bLoadFailed;
		UE_LOG(LogHBMagicVFXPreload, Log, TEXT("Mage VFX prepared: %d resident assets, %.2fs, complete=%d"),
			ResidentAssets.Num(), FPlatformTime::Seconds() - PreparationStartedAt, bPreloadComplete);
		PreparationHandle.Reset();
		return false;
	}
	if (FPlatformTime::Seconds() - PreparationStartedAt > 60.0)
	{
		UE_LOG(LogHBMagicVFXPreload, Warning, TEXT("Mage VFX preparation timed out (%d pending); retaining loaded assets, existing cues remain available."), PendingSystems.Num());
		PreparationHandle.Reset();
		return false;
	}
	return true;
}

void UHBMagicVFXPreloadSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(PreparationHandle);
	PreparationHandle.Reset();
	if (LoadHandle.IsValid())
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	PendingSystems.Reset();
	ResidentAssets.Reset();
	bPreloadComplete = false;
	Super::Deinitialize();
}
