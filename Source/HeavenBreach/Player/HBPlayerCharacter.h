#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "Combat/Interface/DamageableInterface.h"
#include "GameFramework/Character.h"
#include "HBPlayerCharacter.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class AHB_PlayerState;
class UCameraComponent;
class UInventoryComponent;
class UEquipmentComponent;
class UInteractionComponent;
class UInteractionPromptWidget;
class UHBJobData;
class USkeletalMesh;
class UStaticMesh;
struct FGameplayAbilitySpecHandle;
class UQuickSlotComponent;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerCharacter : public ACharacter,public IDamageableInterface,public IAbilitySystemInterface

{
	GENERATED_BODY()

public:
	AHBPlayerCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void Attack();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void StartBlocking();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void StopBlocking();

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void ToggleBlocking();

	UFUNCTION(BlueprintCallable, Category = "Player|Abilities")
	virtual void ActivateSkill1();

	UFUNCTION(BlueprintCallable, Category = "Player|Abilities")
	virtual void ActivateSkill2();

	UFUNCTION(BlueprintPure, Category = "Player|Job")
	UHBJobData* GetJobData() const { return JobData; }

	/** Applies IDs selected by the lobby. The server validates every ID against JobData. */
	UFUNCTION(BlueprintCallable, Category="Player|Loadout")
	void RequestApplyLoadout(FName SkillQId, FName SkillEId, const TArray<FName>& PassiveSkillIds);

	UFUNCTION(BlueprintPure, Category="Player|Loadout")
	FName GetSelectedSkillQId() const { return SelectedSkillQId; }

	UFUNCTION(BlueprintPure, Category="Player|Loadout")
	FName GetSelectedSkillEId() const { return SelectedSkillEId; }

	UFUNCTION(BlueprintPure, Category="Player|Loadout")
	TArray<FName> GetSelectedPassiveSkillIds() const { return SelectedPassiveSkillIds; }

	/** Server-only progression hook. Grants a passive without replacing the equipped Q/E skills. */
	bool AddPassiveSkillFromUpgrade(FName PassiveSkillId);

	/** UI-ready GAS cooldown query using the same stable Skill ID stored by the loadout. */
	UFUNCTION(BlueprintPure, Category="Player|Abilities")
	float GetSkillCooldownRemaining(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category="Player|Abilities")
	bool IsSkillOnCooldown(FName SkillId) const;

	UFUNCTION(BlueprintPure, Category="Player|Abilities")
	int32 GetSkillUpgradeLevel(FName SkillId) const;

	/** Resets the mana regeneration delay after an action consumes mana. Server authoritative. */
	void NotifyManaActivity();

	UFUNCTION(BlueprintPure, Category = "Player|Inventory")
	UInventoryComponent* GetInventory() const;

	UFUNCTION(BlueprintPure, Category = "Player|Inventory")
	UEquipmentComponent* GetEquipment() const;

	UFUNCTION(BlueprintPure, Category = "Player|Inventory")
	UQuickSlotComponent* GetQuickSlot() const;

	// 장착 무기의 WeaponType을 태그로 변환한다. 올바르지 않거나 타입이 없으면 Weapon.Unarmed.
	UFUNCTION(BlueprintPure, Category = "Player|Combat")
	FGameplayTag GetEquippedWeaponTag() const;

#pragma region Interaction
public:
	virtual void PawnClientRestart() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Player|UI")
	TSubclassOf<UInteractionPromptWidget> InteractionPromptClass;

private:
	UPROPERTY()
	TObjectPtr<UInteractionPromptWidget> InteractionPromptWidgetInstance;

	void SetupInteractionPrompt();

	UFUNCTION()
	void HandleInteractableFocusChanged(AActor* NewInteractable);
#pragma endregion

private:
	UFUNCTION()
	void HandleItemUsed(FName ItemId, float HealAmount);

protected:
	virtual void OnAbilitySystemInitialized(UAbilitySystemComponent* AbilitySystem);
	void InitializeJob(UAbilitySystemComponent* AbilitySystem);
	void ApplyJobAppearance();
	FGameplayAbilitySpecHandle GrantAbilityIfMissing(
		UAbilitySystemComponent* AbilitySystem,
		TSubclassOf<UGameplayAbility> AbilityClass);
	void ApplyLoadout(FName SkillQId, FName SkillEId, TArray<FName> PassiveSkillIds);
	void TryActivateJobSkill(FName SkillId);
	TArray<FName> ResolveStartingSkillIds() const;

	UFUNCTION(Server, Reliable)
	void ServerTryActivateJobSkill(FName SkillId);

	UFUNCTION(Server, Reliable)
	void ServerApplyLoadout(FName SkillQId, FName SkillEId, const TArray<FName>& PassiveSkillIds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Job")
	TObjectPtr<UHBJobData> JobData;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player|Loadout")
	FName SelectedSkillQId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player|Loadout")
	FName SelectedSkillEId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player|Loadout")
	TArray<FName> SelectedPassiveSkillIds;

	TArray<FGameplayAbilitySpecHandle> LoadoutAbilityHandles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Socket whose animated position is followed by the first-person camera. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera")
	FName FirstPersonCameraSocketName = TEXT("FirstPersonCameraSocket");
	
#pragma region ASC
public:
	AHB_PlayerState* GetPlayerCurrentState() const
	{
		return HB_PlayerState ? HB_PlayerState : nullptr;
	}
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	void BindToPlayerStateComponents();
private:
	UPROPERTY()
	TObjectPtr<AHB_PlayerState> HB_PlayerState;

	bool bJobInitialized = false;

	bool bPlayerStateComponentsBound = false;

#pragma endregion 
	
#pragma region DamageInterface
	
public:
	virtual bool CanBeDamaged_Implementation(AActor* InstigatorActor) const override;
	
	virtual void ApplyMeleeHit_Implementation(AActor* Instigator, float RawDamage, const FHitResult& Hit) override;
	
#pragma endregion 
	
#pragma region CombatSystem
public:
	//TODO:: 데이터테이블, 구조체로 변경
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetAttackPower() const;
	
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetAttackRange() const;
	
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetBlockDamageReduction() const;
	
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetBlockAngle() const;

	// 데미지 처리. 패시브나 액티브등 데미지보너스 포함
	float ModifyOutgoingDamage(AActor* Target, float Damage);
	// 
	void NotifyDamageDealt(AActor* Target, float Damage, bool bTargetKilled);
	// 마나 비용의 유일한 통로. 패시브,증강의 소비 감소를 여기 한 곳에서만 처리한다.
	float ModifyManaCost(FGameplayTag SourceTag, float Cost) const;

	// 쿨타임 연산의 유일한 통로. 마나와 같은 모양이다.
	float ModifyCooldown(FGameplayTag SourceTag, float Duration) const;

	// 증강 보유 여부. 출혈, 방패 강타, 마나 0 처럼 켜고 끄는 것에 쓴다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAugment(FGameplayTag AugmentTag) const;

	// 증강 배율. 미획득이면 1.0 이라 그대로 곱해도 안전하다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	float GetAugmentValue(FGameplayTag AugmentTag) const;

	// 비용만큼 마나가 있는가. 클라도 읽어서 즉시 막는다.
	bool HasEnoughMana(float Cost) const;
	float GetAttackManaCost() const { return AttackManaCost; }

	// 스킬id로 해당 스킬의 남은 쿨타임 / 총 쿨타임.
	UFUNCTION(BlueprintPure, Category = "Player|Abilities")
	float GetSkillCooldownPercent(FName SkillId) const;

	// 증강,장비가 반영된 실제 마나 비용
	UFUNCTION(BlueprintPure, Category = "Player|Abilities")
	float GetSkillManaCost(FName SkillId) const;

	// 쿨타임 중이거나 마나가 부족하면 false
	UFUNCTION(BlueprintPure, Category = "Player|Abilities")
	bool CanUseSkill(FName SkillId) const;

	// 실제 차감. 서버에서만 유효하다.
	void ConsumeMana(float Cost);
	
protected:
	/****GA***/
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
	
	/****GE***/
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TSubclassOf<UGameplayEffect> GE_PlayerDamage;
	
	/****CameraShake***/
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TSubclassOf<UCameraShakeBase> CS_HitReact;
	
	/****BaseAbilities Stat Value***/
	// TODO:: Temp변수 데이터테이블로 변경 or AttributeData변경
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseAttackPower = 10.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseAttackRange = 150.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseBlockDamageReduction = 0.3f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseBlockingAngle = 120.f;

	/** Development-only visualization: Warrior=red, Archer=green, Mage=blue. */
	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Debug")
	bool bDrawBasicAttackDebug = true;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Debug", meta=(ClampMin="0.05"))
	float BasicAttackDebugDuration = 1.f;

private:
	void ResetCombatInstinct();

	TWeakObjectPtr<AActor> ConsecutiveTarget;
	int32 ConsecutiveHitCount = 0;
	TWeakObjectPtr<AActor> LastDamageInstigator;
	double LastDamageReceivedTime = -1000.0;
	FActiveGameplayEffectHandle KillingMomentumEffectHandle;
	FTimerHandle CombatInstinctTimer;

	void UpdateManaResource();
	void StopManaActionsAtZero();
	// SkillId로 쿨다운 GE 의 부여 태그 조회
	const FGameplayTagContainer* GetSkillCooldownTags(FName SkillId) const;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float ManaRegenDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float ManaRegenPerSecond = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float SprintManaCostPerSecond = 8.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float BlockManaCostPerSecond = 6.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.05"))
	float ManaResourceTickInterval = 0.25f;
	// 공격 1회당 마나.
	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin = "0.0"))
	float AttackManaCost = 10.f;

	FTimerHandle ManaResourceTimer;
	double LastManaActivityTime = 0.0;
#pragma endregion 
	
#pragma region Movement
public:
	void StartSprint();
	
	void StopSprint();

	void SetMovementSpeedMultiplier(float NewMultiplier);

	/** Called by the AttributeSet when a GAS movement modifier changes. */
	void RefreshMovementSpeedFromAttributes();
protected:
	UPROPERTY(EditDefaultsOnly,Category="Player|Movement")
	float WalkSpeed = 300.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Movement")
	float SprintSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly,Category="Player|Movement")
	float CrouchSpeed = 300.f;
	
	UPROPERTY(ReplicatedUsing= OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(ReplicatedUsing=OnRep_MovementSpeedMultiplier)
	float MovementSpeedMultiplier = 1.f;
	
	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION()
	void OnRep_MovementSpeedMultiplier();
	
	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	void RefreshMovementSpeed();
#pragma endregion 
	
#pragma region EquipmentVisual
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<UStaticMeshComponent> WeaponMeshComp;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<UStaticMeshComponent> ShieldMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
	TObjectPtr<class UMeleeTraceComponent> MeleeTraceComp;
	
	#pragma region ModularEquipmentMesh
	// Modular equipment meshes copy the character mesh pose instead of attaching to a socket.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> HelmetMeshComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> ChestMeshComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> LegsMeshComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> ShoesMeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Helmet")
	FName HairMaterialSlotName = TEXT("MAT_M_CHAMP_HAIR");

	void SetHairVisibleForHelmet(bool bVisible);
	#pragma endregion

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName WeaponSocketName = TEXT("PrimaryWeaponSocket");
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName ShieldSocketName = TEXT("SecondWeaponSocket");
	
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void RefreshEquippedVisuals();
	
	void RefreshWeaponVisual();
	void RefreshShieldVisual();
	void RefreshHelmetVisual();
	void RefreshChestVisual();
	void RefreshLegsVisual();

	// TODO: Replace this temporary visual test with item data after coordinating the equipment data format.
	#pragma region TemporaryEquipmentVisualTest
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test")
	bool bEnableEquipmentVisualTest = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestHelmetMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestChestMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestLegsMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestShoesMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<UStaticMesh> TestWeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<UStaticMesh> TestShieldMesh;

	UFUNCTION(BlueprintCallable, Category="Player|Equipment|Temporary Test")
	void ApplyTemporaryEquipmentVisualTest();
	#pragma endregion

#pragma endregion 

public:
	UFUNCTION(Exec, Category = "Debug")
	void TestDamage(float Amount);

protected:
	// C++에서 호출하면 블루프린트에서 실행되는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_OnPlayerStateReady();
};
