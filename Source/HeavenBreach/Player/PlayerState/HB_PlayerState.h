#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/PlayerState.h"
#include "Shop/ShopComponent.h"
#include "Player/Job/HBStartingLoadout.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "HB_PlayerState.generated.h"


class UHB_PlayerAttributeSet;
class UStashComponent;
class UTexture2D;
class UInventoryComponent;
class UEquipmentComponent;
class UQuickSlotComponent;
class UShopComponent;
class ILoadoutStorage;
class UCraftComponent;

struct FPlayerLoadout;

// EHBUpgradeCandidateType, FHBUpgradeCandidate 는 Combat/Ability/HBSkillAugment.h 로
// 옮겼다. 증강 후보는 증강 데이터에 속하는 개념이고 PlayerState 는 담는 그릇일 뿐이다.

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMPChanged, float, CurrentMP, float, MaxMP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerLevelChanged, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPlayerExperienceChanged, int32, CurrentExperience, int32, ExperienceToNextLevel, float, ExperiencePercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTeamIdChanged, int32, NewTeamId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillUpgradesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeCandidatesChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartingLoadoutChanged);

UCLASS()
class HEAVENBREACH_API AHB_PlayerState : public APlayerState,public IAbilitySystemInterface
{
	GENERATED_BODY()
	

public:
	AHB_PlayerState();

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void RequestSaveLoadout();

	void HandleQuickSlotChanged();

	FTimerHandle LoadoutSaveTimer;

	void HandleLoadoutSaved(bool bSuccess);
	bool bLoadoutSaveInFlight = false;
	bool bLoadoutSavePending = false;

	// ��� �׽�Ʈ
	UFUNCTION()
	void HandleStashLoadCompleted(bool bSuccess);

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bGrantTestGold = true;

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	int32 TestGoldMinimum = 5000;
	// ��� �׽�Ʈ

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

	// 이 증강을 골랐는가. 스킬 GA 가 캐릭터를 거쳐 이걸 묻는다.
	UFUNCTION(BlueprintPure, Category = "Progression|Skills")
	bool HasAugment(FGameplayTag AugmentTag) const;

	// 지금까지 고른 증강 전부. UI 가 목록을 그릴 때 쓴다.
	UFUNCTION(BlueprintPure, Category = "Progression|Skills")
	FGameplayTagContainer GetTakenAugments() const { return TakenAugments; }

	// ================ 폐기 예정 ================
	// 스킬 레벨(0~3) 모델은 증강 태그로 대체됐다. 아래 둘은 아무 일도 하지 않는다.
	//
	// 왜 남겼나: 둘 다 BlueprintPure / BlueprintCallable 이라 UI 위젯 BP 가
	// 부르고 있으면 지우는 순간 그 BP 의 컴파일이 깨진다. BP 참조 여부를
	// 확인하기 전까지는 껍데기로 둔다.
	//
	// 쳐낼 조건: 에디터의 Find in Blueprints 로 "Get Skill Upgrade Level" 과
	// "Request Upgrade Skill" 을 검색해 참조가 없으면 이 블록을 통째로 지운다.
	UFUNCTION(BlueprintPure, Category = "Progression|Skills", meta = (DeprecatedFunction, DeprecationMessage = "증강 태그로 대체됨. HasAugment 를 쓸 것"))
	int32 GetSkillUpgradeLevel(FName SkillId) const;

	UFUNCTION(BlueprintCallable, Category = "Progression|Skills", meta = (DeprecatedFunction, DeprecationMessage = "증강 태그로 대체됨. RequestSelectUpgrade 를 쓸 것"))
	void RequestUpgradeSkill(FName SkillId);
	// ================ 폐기 예정 끝 ================

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

	// 이번 판에서 고른 증강 전부. 추가만 되고 개별 제거는 없다.
	// 매치가 끝나 로비로 돌아가면 ResetMatchProgression 이 통째로 비운다.
	//
	// 왜 ASC 태그가 아니라 별도 컨테이너인가: UI 가 "내가 고른 증강 목록"을
	// 열거해야 하는데, ASC 에 넣으면 다른 상태 태그와 섞여 걸러내야 한다.
	UPROPERTY(ReplicatedUsing = OnRep_SkillUpgrades)
	FGameplayTagContainer TakenAugments;

	// 증강이 붙인 상시 GE 핸들. 개별 취소용이 아니라 매치 리셋용이다.
	// 복제하지 않는다. 서버에서만 걷으면 클라는 어트리뷰트 복제로 따라온다.
	TArray<FActiveGameplayEffectHandle> AugmentEffectHandles;

	UPROPERTY(ReplicatedUsing = OnRep_SkillUpgrades)
	int32 PendingUpgradeChoices = 0;

	UPROPERTY(ReplicatedUsing = OnRep_UpgradeCandidates)
	TArray<FHBUpgradeCandidate> CurrentUpgradeCandidates;

	// 후보를 뽑을 때 쓰는 조각들. 람다를 쓰지 않으려고 함수로 나눴다.
	void GenerateUpgradeCandidates();
	void CollectMissingPassives(TArray<FHBUpgradeCandidate>& OutPool) const;
	void CollectSkillAugments(TArray<FHBUpgradeCandidate>& OutPool, FName SkillId) const;

	// 증강 하나를 실제로 부여한다. 태그를 넣고 GrantEffect 가 있으면 붙인다.
	void ApplyAugment(FName TargetSkillId, FGameplayTag AugmentTag);

	// 매치가 끝날 때 증강 GE 를 전부 걷는다.
	void RemoveAllAugmentEffects();

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

#pragma region StartingLoadout
public:
	/** 로비에서 확정한 시작 구성. 매치 캐릭터가 이걸 읽어 스킬을 부여한다. */
	UFUNCTION(BlueprintPure, Category = "Loadout|Skills")
	FHBStartingLoadout GetStartingLoadout() const { return StartingLoadout; }

	/** 고른 직업. UI 가 스킬 목록을 그릴 때 이걸로 카탈로그를 꺼낸다. */
	UFUNCTION(BlueprintPure, Category = "Loadout|Job")
	UHBJobData* GetSelectedJob() const { return StartingLoadout.Job; }

	/**
	 * 서버 전용. 검증을 통과해야만 확정된다.
	 *
	 * 왜 서버만인가: 이 값이 그대로 매치의 GA 부여로 이어진다. 클라가 직접
	 * 쓰게 두면 아무 스킬이나 장착할 수 있다.
	 *
	 * 작동방식: ValidateStartingLoadout 을 통과하고, 기존 값과 다를 때만
	 * 바꾸고 알린다. 같은 값이면 복제도 알림도 일으키지 않는다.
	 */
	bool TrySetStartingLoadout(const FHBStartingLoadout& InLoadout, FText& OutError);

	UPROPERTY(BlueprintAssignable, Category = "Loadout|Skills")
	FOnStartingLoadoutChanged OnStartingLoadoutChanged;

	/**
	 * 심리스 트래블로 새 PlayerState 가 만들어질 때 시작 구성을 넘긴다.
	 *
	 * 왜 필요한가: 로비→매치는 ServerTravel + bUseSeamlessTravel 이라 객체가
	 * 새로 생긴다. 로비는 AHBLobbyPlayerState, 매치는 AHB_PlayerState 로
	 * 클래스까지 다르다. 이 함수를 안 거치는 필드는 전부 사라진다.
	 * 프로젝트에 CopyProperties 오버라이드가 하나도 없어서, 지금까지 로비의
	 * 어떤 선택도 매치에 도달하지 못했다.
	 *
	 * 쳐낼 조건: 성장 정보까지 다음 판으로 넘기는 기획이 생기면 여기에 더한다.
	 * 지금은 시작 구성만 넘긴다.
	 */
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

protected:
	/**
	 * 서버 확정과 클라 OnRep 이 함께 지나는 한 지점.
	 *
	 * 왜 가상인가: 로비 자식이 여기에 직업 변경 알림을 덧붙인다. 자식이
	 * BeginPlay 에서 부모 델리게이트를 자가 구독하는 것보다 순서가 확실하다.
	 */
	virtual void HandleStartingLoadoutChanged();

private:
	UPROPERTY(ReplicatedUsing = OnRep_StartingLoadout)
	FHBStartingLoadout StartingLoadout;

	UFUNCTION()
	void OnRep_StartingLoadout();
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

	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void SaveLoadout();
private:
	UPROPERTY()
	TObjectPtr<UStashComponent> Stash;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UEquipmentComponent> Equipment;

	UPROPERTY()
	TObjectPtr<UQuickSlotComponent> QuickSlot;

	void LoadLoadout();
	void HandleLoadoutLoaded(bool bSuccess, const FPlayerLoadout& InLoadout);

	TSharedPtr<ILoadoutStorage> LoadoutStorage;
	FString LoadoutAccountId;
	FString ResolveAccountId() const;
#pragma endregion

#pragma region Shop
public:
	UFUNCTION(BlueprintPure, Category = "Shop")
	UShopComponent* GetShop() const { return Shop; }

private:
	UPROPERTY()
	TObjectPtr<UShopComponent> Shop;

#pragma endregion

#pragma region Craft
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Craft")
	TObjectPtr<UCraftComponent> Craft;

	UFUNCTION(BlueprintPure, Category = "Craft")
	UCraftComponent* GetCraft() const { return Craft; }
#pragma endregion
};
