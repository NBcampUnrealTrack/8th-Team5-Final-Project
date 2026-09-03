#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "HB_PlayerState.generated.h"


class UHB_PlayerAttributeSet;
class UStashComponent;
class UTexture2D;

UENUM(BlueprintType)
enum class EHBUpgradeCandidateType : uint8
{
	ActiveSkill,
	NewPassive
};

USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBUpgradeCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName CandidateId;

	UPROPERTY(BlueprintReadOnly)
	EHBUpgradeCandidateType Type = EHBUpgradeCandidateType::ActiveSkill;

	UPROPERTY(BlueprintReadOnly)
	FName TargetSkillId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText Description;

	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly)
	int32 CurrentLevel = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 NextLevel = 0;
};

class UInventoryComponent;
class UEquipmentComponent;
class UQuickSlotComponent;
class UShopComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMPChanged, float, CurrentMP, float, MaxMP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLevelChanged, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerExperienceChanged, int32, CurrentExperience, int32, ExperienceToNextLevel, float, ExperiencePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamIdChanged, int32, NewTeamId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillUpgradesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeCandidatesChanged);

UCLASS()
class HEAVENBREACH_API AHB_PlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()
	

public:
	AHB_PlayerState();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma region MatchProgression
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression|Match")
	void AddExperience(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression|Match")
	void ResetMatchProgression();

	UFUNCTION(BlueprintPure, Category = "Progression|Match")
	int32 GetPlayerLevel() const { return PlayerLevel; }

	UFUNCTION(BlueprintPure, Category = "Progression|Match")
	int32 GetCurrentExperience() const { return CurrentExperience; }

	UFUNCTION(BlueprintPure, Category = "Progression|Match")
	int32 GetExperienceToNextLevel() const;

	UFUNCTION(BlueprintPure, Category = "Progression|Match")
	float GetExperiencePercent() const;

	UPROPERTY(BlueprintAssignable, Category = "Progression|Match")
	FOnPlayerLevelChanged OnPlayerLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "Progression|Match")
	FOnPlayerExperienceChanged OnPlayerExperienceChanged;

	UFUNCTION(BlueprintPure, Category = "Progression|Skills")
	int32 GetSkillUpgradeLevel(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category = "Progression|Skills")
	bool CanUpgradeSkill(FName SkillId) const;

	/** Spends one pending level-up choice to upgrade an equipped Q/E skill. */
	UFUNCTION(BlueprintCallable, Category = "Progression|Skills")
	void RequestUpgradeSkill(FName SkillId);

	UFUNCTION(BlueprintPure, Category = "Progression|Skills")
	int32 GetPendingUpgradeChoices() const { return PendingUpgradeChoices; }

	UPROPERTY(BlueprintAssignable, Category = "Progression|Skills")
	FOnSkillUpgradesChanged OnSkillUpgradesChanged;

	UFUNCTION(BlueprintPure, Category = "Progression|Skills")
	TArray<FHBUpgradeCandidate> GetCurrentUpgradeCandidates() const { return CurrentUpgradeCandidates; }

	/** Selects one of the candidate IDs currently offered by the server. */
	UFUNCTION(BlueprintCallable, Category = "Progression|Skills")
	void RequestSelectUpgrade(FName CandidateId);

	UPROPERTY(BlueprintAssignable, Category = "Progression|Skills")
	FOnUpgradeCandidatesChanged OnUpgradeCandidatesChanged;

private:
	UPROPERTY(ReplicatedUsing = OnRep_PlayerLevel)
	int32 PlayerLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	int32 CurrentExperience = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Progression|Match", meta = (ClampMin = "1"))
	int32 MaxPlayerLevel = 10;

	UPROPERTY(EditDefaultsOnly, Category = "Progression|Match", meta = (ClampMin = "1"))
	int32 BaseExperienceToNextLevel = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Progression|Match", meta = (ClampMin = "0"))
	int32 ExperienceIncreasePerLevel = 50;

	UPROPERTY(EditDefaultsOnly, Category = "Progression|Skills", meta = (ClampMin = "1"))
	int32 MaxSkillUpgradeLevel = 3;

	UPROPERTY(ReplicatedUsing = OnRep_SkillUpgrades)
	int32 SkillQUpgradeLevel = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SkillUpgrades)
	int32 SkillEUpgradeLevel = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SkillUpgrades)
	int32 PendingUpgradeChoices = 0;

	UPROPERTY(ReplicatedUsing = OnRep_UpgradeCandidates)
	TArray<FHBUpgradeCandidate> CurrentUpgradeCandidates;

	UFUNCTION(Server, Reliable)
	void ServerRequestUpgradeSkill(FName SkillId);

	void ApplySkillUpgrade(FName SkillId);
	void GenerateUpgradeCandidates();

	UFUNCTION(Server, Reliable)
	void ServerRequestSelectUpgrade(FName CandidateId);

	void ApplyUpgradeCandidate(FName CandidateId);

	UFUNCTION()
	void OnRep_SkillUpgrades();

	UFUNCTION()
	void OnRep_UpgradeCandidates();

	UFUNCTION()
	void OnRep_PlayerLevel();

	UFUNCTION()
	void OnRep_CurrentExperience();

	void BroadcastProgression();
#pragma endregion

#pragma region Team
public:
	/** Sets the team assigned by the server-side match/team system. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Team")
	void SetTeamId(int32 NewTeamId);

	UFUNCTION(BlueprintPure, Category = "Team")
	int32 GetTeamId() const { return TeamId; }

	UFUNCTION(BlueprintPure, Category = "Team")
	bool HasValidTeam() const { return TeamId != INDEX_NONE; }

	/** Unassigned players are never considered teammates. */
	UFUNCTION(BlueprintPure, Category = "Team")
	bool IsSameTeam(const AHB_PlayerState* OtherPlayerState) const;

	UPROPERTY(BlueprintAssignable, Category = "Team")
	FOnTeamIdChanged OnTeamIdChanged;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	int32 TeamId = INDEX_NONE;

	UFUNCTION()
	void OnRep_TeamId();
#pragma endregion

#pragma region AbilitySystem
public:
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override {return AbilitySystemComponent;}
private:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
#pragma endregion 
	
#pragma region AttributeSet
public:
	UHB_PlayerAttributeSet* GetPlayerAttributeSet() const {return AttributeSet;}
	
private:
	UPROPERTY()
	TObjectPtr<UHB_PlayerAttributeSet> AttributeSet;
#pragma endregion 
	
#pragma region BindingWidget

public:
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnHPChanged OnHPChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnMPChanged OnMPChanged;

protected:
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const struct FOnAttributeChangeData& Data);
	void OnManaChanged(const struct FOnAttributeChangeData& Data);
	void OnMaxManaChanged(const struct FOnAttributeChangeData& Data);

#pragma endregion

#pragma region Stash
public:
	UFUNCTION(BlueprintPure, Category = "Stash")
	UStashComponent* GetStash() const { return Stash; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UInventoryComponent* GetInventory() const { return Inventory; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UEquipmentComponent* GetEquipment() const { return Equipment; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UQuickSlotComponent* GetQuickSlot() const { return QuickSlot; }
private:
	UPROPERTY()
	TObjectPtr<UStashComponent> Stash;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> Equipment;

	UPROPERTY()
	TObjectPtr<UQuickSlotComponent> QuickSlot;
#pragma endregion

#pragma region Shop
public:
	UFUNCTION(BlueprintPure, Category = "Shop")
	UShopComponent* GetShop() const { return Shop; }

private:
	UPROPERTY()
	TObjectPtr<UShopComponent> Shop;

#pragma endregion
};
