#include "HB_PlayerState.h"

#include "Inventory/StashComponent.h"
#include "Core/DataManagement/StashStorageSubsystem.h"
#include "Engine/GameInstance.h"
#include "AbilitySystemComponent.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/Job/HBJobData.h"
#include "Net/UnrealNetwork.h"

namespace
{
	FText GetActiveUpgradeDescription(const FName SkillId, const int32 NextLevel)
	{
		if (SkillId == TEXT("Warrior.ChargeSlash"))
		{
			if (NextLevel == 1) return FText::FromString(TEXT("피해 배율이 1.4배로 증가합니다."));
			if (NextLevel == 2) return FText::FromString(TEXT("돌진 거리와 공격 판정 범위가 증가합니다."));
			if (NextLevel == 3) return FText::FromString(TEXT("피해 배율이 1.8배가 되고 쿨타임이 4.5초로 감소합니다."));
		}
		if (SkillId == TEXT("Warrior.CombatRoll"))
		{
			if (NextLevel == 1) return FText::FromString(TEXT("구르기 이동 거리가 20% 증가합니다."));
			if (NextLevel == 2) return FText::FromString(TEXT("쿨타임이 2.75초로 감소합니다."));
			if (NextLevel == 3) return FText::FromString(TEXT("쿨타임이 2.5초로 감소합니다."));
		}
		if (SkillId == TEXT("Warrior.Execute"))
		{
			if (NextLevel == 1) return FText::FromString(TEXT("체력 35% 이하인 적에게 사용할 수 있습니다."));
			if (NextLevel == 2) return FText::FromString(TEXT("피해 배율이 3배로 증가합니다."));
			if (NextLevel == 3) return FText::FromString(TEXT("체력 40% 이하인 적에게 사용할 수 있습니다."));
		}
		if (SkillId == TEXT("Warrior.BattleCry"))
		{
			if (NextLevel == 1) return FText::FromString(TEXT("버프 지속시간이 7초로 증가합니다."));
			if (NextLevel == 2) return FText::FromString(TEXT("공격력 증가량이 30%로 증가합니다."));
			if (NextLevel == 3) return FText::FromString(TEXT("버프가 유지되는 동안 방어력이 20 증가합니다."));
		}
		if (SkillId == TEXT("Warrior.ChainPull"))
		{
			if (NextLevel == 1) return FText::FromString(TEXT("사거리가 900으로 증가합니다."));
			if (NextLevel == 2) return FText::FromString(TEXT("적을 당기는 힘이 1400으로 증가합니다."));
			if (NextLevel == 3) return FText::FromString(TEXT("쿨타임이 8초로 감소합니다."));
		}
		if (SkillId == TEXT("Warrior.BloodThirst"))
		{
			if (NextLevel == 1) return FText::FromString(TEXT("피해 흡혈량이 25%로 증가합니다."));
			if (NextLevel == 2) return FText::FromString(TEXT("지속시간이 7초로 증가합니다."));
			if (NextLevel == 3) return FText::FromString(TEXT("흡혈량이 30%로 증가하고 쿨타임이 13초로 감소합니다."));
		}

		return FText::Format(
			NSLOCTEXT("HBProgression", "GenericActiveUpgrade", "스킬이 Lv.{0}로 강화됩니다."),
			FText::AsNumber(NextLevel));
	}
}

AHB_PlayerState::AHB_PlayerState()
{
#pragma region NetworkSettings
	bReplicates = true;
#pragma endregion 
	
#pragma region AbilitySystemComponent
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
#pragma endregion 
	
#pragma region AttributeSet
	AttributeSet = CreateDefaultSubobject<UHB_PlayerAttributeSet>(TEXT("AttributeSet"));
#pragma endregion

#pragma region Stash
	Stash = CreateDefaultSubobject<UStashComponent>(TEXT("Stash"));
#pragma endregion
}

void AHB_PlayerState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ResetMatchProgression();
	}

	if (AbilitySystemComponent && AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_HPAttribute())
			.AddUObject(this, &AHB_PlayerState::OnHealthChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute())
			.AddUObject(this, &AHB_PlayerState::OnMaxHealthChanged);
	}

	if (HasAuthority() && Stash)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UStashStorageSubsystem* StorageSubsystem = GameInstance->GetSubsystem<UStashStorageSubsystem>())
			{
				Stash->SetStorage(StorageSubsystem->GetStorage());
				Stash->LoadStash(TEXT("TestAccount"));
			}
		}
	}
}

void AHB_PlayerState::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (OnHPChanged.IsBound() && AttributeSet)
	{
		OnHPChanged.Broadcast(Data.NewValue, AttributeSet->GetPlayer_MaxHP());
	}
}

void AHB_PlayerState::OnMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	if (OnHPChanged.IsBound() && AttributeSet)
	{
		OnHPChanged.Broadcast(AttributeSet->GetPlayer_HP(), Data.NewValue);
	}
}

void AHB_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHB_PlayerState, PlayerLevel);
	DOREPLIFETIME(AHB_PlayerState, CurrentExperience);
	DOREPLIFETIME(AHB_PlayerState, TeamId);
	DOREPLIFETIME(AHB_PlayerState, SkillQUpgradeLevel);
	DOREPLIFETIME(AHB_PlayerState, SkillEUpgradeLevel);
	DOREPLIFETIME(AHB_PlayerState, PendingUpgradeChoices);
	DOREPLIFETIME(AHB_PlayerState, CurrentUpgradeCandidates);
}

void AHB_PlayerState::AddExperience(const int32 Amount)
{
	if (!HasAuthority() || Amount <= 0 || PlayerLevel >= MaxPlayerLevel)
	{
		return;
	}

	const int32 PreviousLevel = PlayerLevel;
	CurrentExperience += Amount;
	while (PlayerLevel < MaxPlayerLevel)
	{
		const int32 RequiredExperience = GetExperienceToNextLevel();
		if (CurrentExperience < RequiredExperience)
		{
			break;
		}

		CurrentExperience -= RequiredExperience;
		++PlayerLevel;
	}

	if (PlayerLevel >= MaxPlayerLevel)
	{
		CurrentExperience = 0;
	}

	PendingUpgradeChoices += PlayerLevel - PreviousLevel;

	BroadcastProgression();
	if (PlayerLevel != PreviousLevel)
	{
		OnSkillUpgradesChanged.Broadcast();
		GenerateUpgradeCandidates();
	}
	ForceNetUpdate();
}

void AHB_PlayerState::ResetMatchProgression()
{
	if (!HasAuthority())
	{
		return;
	}

	PlayerLevel = 1;
	CurrentExperience = 0;
	SkillQUpgradeLevel = 0;
	SkillEUpgradeLevel = 0;
	PendingUpgradeChoices = 0;
	CurrentUpgradeCandidates.Reset();
	BroadcastProgression();
	OnSkillUpgradesChanged.Broadcast();
	OnUpgradeCandidatesChanged.Broadcast();
	ForceNetUpdate();
}

int32 AHB_PlayerState::GetExperienceToNextLevel() const
{
	if (PlayerLevel >= MaxPlayerLevel)
	{
		return 0;
	}

	return BaseExperienceToNextLevel + (PlayerLevel - 1) * ExperienceIncreasePerLevel;
}

float AHB_PlayerState::GetExperiencePercent() const
{
	const int32 RequiredExperience = GetExperienceToNextLevel();
	return RequiredExperience > 0
		? FMath::Clamp(static_cast<float>(CurrentExperience) / static_cast<float>(RequiredExperience), 0.0f, 1.0f)
		: 1.0f;
}

void AHB_PlayerState::OnRep_PlayerLevel()
{
	BroadcastProgression();
}

void AHB_PlayerState::OnRep_CurrentExperience()
{
	BroadcastProgression();
}

void AHB_PlayerState::BroadcastProgression()
{
	OnPlayerLevelChanged.Broadcast(PlayerLevel);
	OnPlayerExperienceChanged.Broadcast(CurrentExperience, GetExperienceToNextLevel(), GetExperiencePercent());
}

int32 AHB_PlayerState::GetSkillUpgradeLevel(const FName SkillId) const
{
	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	if (!Character)
	{
		return 0;
	}

	if (Character->GetSelectedSkillQId() == SkillId)
		return FMath::Clamp(SkillQUpgradeLevel, 0, MaxSkillUpgradeLevel);
	if (Character->GetSelectedSkillEId() == SkillId)
		return FMath::Clamp(SkillEUpgradeLevel, 0, MaxSkillUpgradeLevel);
	return 0;
}

bool AHB_PlayerState::CanUpgradeSkill(const FName SkillId) const
{
	const AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	const bool bIsEquipped = Character
		&& (Character->GetSelectedSkillQId() == SkillId
			|| Character->GetSelectedSkillEId() == SkillId);
	return PendingUpgradeChoices > 0
		&& bIsEquipped
		&& GetSkillUpgradeLevel(SkillId) < MaxSkillUpgradeLevel;
}

void AHB_PlayerState::RequestUpgradeSkill(const FName SkillId)
{
	if (HasAuthority())
	{
		ApplySkillUpgrade(SkillId);
	}
	else
	{
		ServerRequestUpgradeSkill(SkillId);
	}
}

void AHB_PlayerState::ServerRequestUpgradeSkill_Implementation(const FName SkillId)
{
	ApplySkillUpgrade(SkillId);
}

void AHB_PlayerState::ApplySkillUpgrade(const FName SkillId)
{
	if (!HasAuthority() || !CanUpgradeSkill(SkillId))
	{
		return;
	}

	AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	if (!Character)
	{
		return;
	}

	if (Character->GetSelectedSkillQId() == SkillId)
	{
		SkillQUpgradeLevel = FMath::Min(SkillQUpgradeLevel + 1, MaxSkillUpgradeLevel);
	}
	else if (Character->GetSelectedSkillEId() == SkillId)
	{
		SkillEUpgradeLevel = FMath::Min(SkillEUpgradeLevel + 1, MaxSkillUpgradeLevel);
	}
	else
	{
		return;
	}
	--PendingUpgradeChoices;
	OnSkillUpgradesChanged.Broadcast();
	GenerateUpgradeCandidates();
	ForceNetUpdate();
}

void AHB_PlayerState::OnRep_SkillUpgrades()
{
	OnSkillUpgradesChanged.Broadcast();
}

void AHB_PlayerState::RequestSelectUpgrade(const FName CandidateId)
{
	if (HasAuthority())
	{
		ApplyUpgradeCandidate(CandidateId);
	}
	else
	{
		ServerRequestSelectUpgrade(CandidateId);
	}
}

void AHB_PlayerState::ServerRequestSelectUpgrade_Implementation(const FName CandidateId)
{
	ApplyUpgradeCandidate(CandidateId);
}

void AHB_PlayerState::ApplyUpgradeCandidate(const FName CandidateId)
{
	if (!HasAuthority() || PendingUpgradeChoices <= 0)
	{
		return;
	}

	const FHBUpgradeCandidate* Candidate = CurrentUpgradeCandidates.FindByPredicate(
		[CandidateId](const FHBUpgradeCandidate& Entry)
		{
			return Entry.CandidateId == CandidateId;
		});
	if (!Candidate)
	{
		return;
	}

	if (Candidate->Type == EHBUpgradeCandidateType::ActiveSkill)
	{
		ApplySkillUpgrade(Candidate->TargetSkillId);
		return;
	}

	AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	if (Character && Character->AddPassiveSkillFromUpgrade(Candidate->TargetSkillId))
	{
		--PendingUpgradeChoices;
		OnSkillUpgradesChanged.Broadcast();
		GenerateUpgradeCandidates();
		ForceNetUpdate();
	}
}

void AHB_PlayerState::GenerateUpgradeCandidates()
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentUpgradeCandidates.Reset();
	AHBPlayerCharacter* Character = Cast<AHBPlayerCharacter>(GetPawn());
	UHBJobData* JobData = Character ? Character->GetJobData() : nullptr;
	if (PendingUpgradeChoices <= 0 || !Character || !JobData)
	{
		OnUpgradeCandidatesChanged.Broadcast();
		ForceNetUpdate();
		return;
	}

	TArray<FHBJobSkillData> MissingPassives;
	const TArray<FName> OwnedPassives = Character->GetSelectedPassiveSkillIds();
	for (const FHBJobSkillData& Passive : JobData->GetPassiveSkillCatalog())
	{
		if (!Passive.SkillId.IsNone() && Passive.AbilityClass
			&& !OwnedPassives.Contains(Passive.SkillId))
		{
			MissingPassives.Add(Passive);
		}
	}

	auto AddRandomPassive = [this, &MissingPassives]() -> bool
	{
		if (MissingPassives.IsEmpty())
		{
			return false;
		}

		const int32 Index = FMath::RandRange(0, MissingPassives.Num() - 1);
		const FHBJobSkillData Passive = MissingPassives[Index];
		MissingPassives.RemoveAtSwap(Index);

		FHBUpgradeCandidate& Candidate = CurrentUpgradeCandidates.AddDefaulted_GetRef();
		Candidate.CandidateId = FName(*FString::Printf(
			TEXT("Acquire.Passive.%s"), *Passive.SkillId.ToString()));
		Candidate.Type = EHBUpgradeCandidateType::NewPassive;
		Candidate.TargetSkillId = Passive.SkillId;
		Candidate.DisplayName = Passive.DisplayName;
		Candidate.Description = Passive.Description;
		Candidate.Icon = Passive.Icon;
		return true;
	};

	auto AddActiveUpgrade = [this, JobData](const FName SkillId)
	{
		if (SkillId.IsNone() || GetSkillUpgradeLevel(SkillId) >= MaxSkillUpgradeLevel)
		{
			return;
		}

		const FHBJobSkillData* Skill = JobData->FindActiveSkill(SkillId);
		if (!Skill)
		{
			return;
		}

		const int32 CurrentLevel = GetSkillUpgradeLevel(SkillId);
		FHBUpgradeCandidate& Candidate = CurrentUpgradeCandidates.AddDefaulted_GetRef();
		Candidate.CandidateId = FName(*FString::Printf(
			TEXT("Upgrade.Active.%s.%d"), *SkillId.ToString(), CurrentLevel + 1));
		Candidate.Type = EHBUpgradeCandidateType::ActiveSkill;
		Candidate.TargetSkillId = SkillId;
		Candidate.DisplayName = FText::Format(
			NSLOCTEXT("HBProgression", "ActiveUpgradeName", "{0} 강화"),
			Skill->DisplayName);
		Candidate.Description = GetActiveUpgradeDescription(SkillId, CurrentLevel + 1);
		Candidate.Icon = Skill->Icon;
		Candidate.CurrentLevel = CurrentLevel;
		Candidate.NextLevel = CurrentLevel + 1;
	};

	// Card 1 is a random unowned passive whenever one exists.
	AddRandomPassive();
	AddActiveUpgrade(Character->GetSelectedSkillQId());
	AddActiveUpgrade(Character->GetSelectedSkillEId());

	while (CurrentUpgradeCandidates.Num() < 3 && AddRandomPassive())
	{
	}

	OnUpgradeCandidatesChanged.Broadcast();
	ForceNetUpdate();
}

void AHB_PlayerState::OnRep_UpgradeCandidates()
{
	OnUpgradeCandidatesChanged.Broadcast();
}

void AHB_PlayerState::SetTeamId(const int32 NewTeamId)
{
	if (!HasAuthority() || TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
	OnTeamIdChanged.Broadcast(TeamId);
	ForceNetUpdate();
}

bool AHB_PlayerState::IsSameTeam(const AHB_PlayerState* OtherPlayerState) const
{
	return OtherPlayerState
		&& HasValidTeam()
		&& OtherPlayerState->HasValidTeam()
		&& TeamId == OtherPlayerState->TeamId;
}

void AHB_PlayerState::OnRep_TeamId()
{
	OnTeamIdChanged.Broadcast(TeamId);
}
