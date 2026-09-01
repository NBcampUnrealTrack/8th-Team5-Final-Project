#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"

#include "HB_PlayerState.generated.h"


class UHB_PlayerAttributeSet;
class UStashComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLevelChanged, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerExperienceChanged, int32, CurrentExperience, int32, ExperienceToNextLevel, float, ExperiencePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamIdChanged, int32, NewTeamId);

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

protected:
	void OnHealthChanged(const struct FOnAttributeChangeData& Data);
	void OnMaxHealthChanged(const struct FOnAttributeChangeData& Data);

#pragma endregion

#pragma region Stash
public:
	UFUNCTION(BlueprintPure, Category = "Stash")
	UStashComponent* GetStash() const { return Stash; }

private:
	UPROPERTY()
	TObjectPtr<UStashComponent> Stash;

	void TryLoadStash();

	FTimerHandle StashLoadTimerHandle;

#pragma endregion
};
