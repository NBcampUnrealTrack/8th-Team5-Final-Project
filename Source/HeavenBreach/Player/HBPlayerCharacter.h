#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "Item/ItemEffectRules.h"
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
struct FHBJobSkillData;
struct FHBStartingLoadout;
class USkeletalMesh;
class UStaticMesh;
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

	// 패링을 시도한다. 방패가 유일한 전제조건이고 직업은 보지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void TryParry();

	// R 을 뗀 신호. 방어태세가 이걸 받아 종료한다.
	// 패링만 돌고 있으면 받는 쪽이 없어 무해하다.
	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void ReleaseParryInput();

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

	// ================ 디버그 ================

	/**
	 * 콘솔에서 아이템 하나를 만들어 인벤토리에 넣고 바로 장착한다.
	 * PIE 에서 무기 없이 스킬을 못 쓰는 상황을 넘기려고 둔 것이다.
	 *
	 * 쓰는 법: PIE 중 ` 로 콘솔을 열고  DebugEquip WPN_001
	 *   DT_Weapon  WPN_001~004(검) / BOW_001~004 / DAG_001~004
	 *   DT_Shield  SHD_001~004
	 *
	 * 슬롯을 인자로 받지 않는 이유: UEquipmentComponent::CanEquip 이 행 데이터의
	 * EquipSlotType 을 읽어 판정한다. 아이템이 자기 자리를 알고 있다.
	 *
	 * 쳐낼 조건: 로비를 거치는 테스트 경로가 안정되면 지운다.
	 */
	UFUNCTION(Exec, Category = "Player|Debug")
	void DebugEquip(FName RowName);

	UFUNCTION(Server, Reliable)
	void ServerDebugEquip(FName RowName);

	/**
	 * 장착 무기에 맞는 타격 성질 큐 태그. 맨손이면 둔타다.
	 *
	 * 왜 필요한가: 큐는 맞는 쪽이 쏘고 공격자의 ASC 태그에서 성질을 읽는다
	 * (HitCuePlay::ResolveCueTag). 그런데 ActivationOwnedTags 는 생성자에서
	 * 정해지는 상수라 런타임에 바뀌는 장착 무기를 담을 수 없다.
	 * 그래서 공격 어빌리티가 발동 중에만 이 태그를 루즈 태그로 붙인다.
	 *
	 * 쳐낼 조건: 무기마다 성질을 따로 정해야 하면 DT_Weapon 에 칸을 만들고
	 * 여기 대신 그 값을 읽는다. 그때 이 매핑은 폴백으로만 남는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Player|Combat")
	FGameplayTag GetImpactCueTag() const;

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
	/** PlayerState 의 시작 구성을 확정된 JobData 로 검증해 돌려준다. */
	bool ResolveStartingLoadout(FHBStartingLoadout& OutLoadout, FText& OutError) const;

	/**
	 * 이 플레이어가 쓸 직업을 정한다. PlayerState → BP 기본값 순이다.
	 *
	 * 왜 BP 기본값을 CDO 에서 읽나: InitializeJob 이 JobData 를 런타임에 덮으므로,
	 * 인스턴스 값을 보면 "직전에 해석한 직업" 을 기본값으로 오인한다.
	 *
	 * 쳐낼 조건: 모든 진입이 로비를 거치게 되면 BP 폴백을 제거한다.
	 */
	UHBJobData* ResolveJobForCurrentPlayer() const;
	UHBJobData* GetBlueprintDefaultJob() const;

	/** 직업을 다시 해석해 외형까지 맞춘다. 서버·클라 공용. */
	void RefreshJobFromPlayerState();

	UFUNCTION(Server, Reliable)
	void ServerTryActivateJobSkill(FName SkillId);

	UFUNCTION(Server, Reliable)
	void ServerApplyLoadout(FName SkillQId, FName SkillEId, const TArray<FName>& PassiveSkillIds);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Job")
	TObjectPtr<UHBJobData> JobData;

	/**
	 * 외형을 이미 적용한 직업.
	 *
	 * 왜 필요한가: RefreshJobFromPlayerState 가 PossessedBy 와 OnRep_PlayerState
	 * 양쪽에서 불린다. 같은 직업으로 메시를 두 번 갈아끼우지 않으려고 둔다.
	 */
	UPROPERTY()
	TObjectPtr<UHBJobData> AppliedAppearanceJob;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player|Loadout")
	FName SelectedSkillQId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player|Loadout")
	FName SelectedSkillEId;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Player|Loadout")
	TArray<FName> SelectedPassiveSkillIds;

	TArray<FGameplayAbilitySpecHandle> LoadoutAbilityHandles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/**
	 * 1인칭 카메라가 따라가는 뼈. 소켓이 아니라 뼈 이름이다.
	 * 예전에는 neck_02 의 FirstPersonCameraSocket 이었는데, 목은 공격 중에 거의 안 움직여
	 * 화면 흔들림이 없었다. 생성자에서 붙이므로 BP 에서 바꿔도 부착 위치는 안 바뀐다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Camera")
	FName FirstPersonCameraSocketName = TEXT("head");
	
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
	
	virtual void ApplyMeleeHit_Implementation(AActor* Instigator, float RawDamage, const FHitResult& Hit, EHBDamageSource Source) override;
	
#pragma endregion 
	
#pragma region CombatSystem
public:
	//TODO:: 데이터테이블, 구조체로 변경
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetAttackPower() const;

	// 장비의 스킬 피해 옵션 배율. 1.0 이 기본.
	// 스킬 타격이 피해를 넣기 전에 곱한다. 기본 공격은 안 곱한다.
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetSkillDamageMultiplier() const;

	// 체력 회복은 전부 여기를 지난다. 장비의 회복 효과 옵션을 한 곳에서 곱한다.
	//
	// 왜 필요한가: 포션 · 흡혈 · 전투 회복이 각자 SetPlayer_HP 를 불렀다.
	// 회복 효과 옵션을 세 곳에 따로 넣으면 네 번째 회복이 생길 때 빠진다.
	// 새 회복을 만들면 SetPlayer_HP 대신 이걸 부른다.
	void ReceiveHeal(float Amount);

	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetAttackRange() const;
	
	// 정면 판정 각도. 패링과 방어태세가 같은 값을 쓴다.
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetBlockAngle() const;

	// 보조 슬롯에 방패가 있는가. 패링의 유일한 전제조건이다.
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	bool HasShieldEquipped() const;

	// 데미지 처리. 패시브나 액티브등 데미지보너스 포함
	float ModifyOutgoingDamage(AActor* Target, float Damage);
	// 
	void NotifyDamageDealt(AActor* Target, float Damage, bool bTargetKilled);
	// 마나 비용의 유일한 통로. 패시브,증강의 소비 감소를 여기 한 곳에서만 처리한다.
	float ModifyManaCost(FGameplayTag SourceTag, float Cost) const;

	// 쿨타임 연산의 유일한 통로. 마나와 같은 모양이다.
	float ModifyCooldown(FGameplayTag SourceTag, float Duration) const;

	// 증강 보유 여부. 출혈, 방패 강타, 마나 0 처럼 켜고 끄는 것에 쓴다.
	// 답은 PlayerState 의 TakenAugments 가 갖고 있다. 여기는 통로다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAugment(FGameplayTag AugmentTag) const;

	// 태그 하나로 증강 정보를 되찾는다. UI 가 툴팁이나 목록을 그릴 때 쓴다.
	// 직업 카탈로그 전체를 뒤진다. 장착을 풀어도 고른 기록은 남아야 하기 때문이다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool FindAugmentInfo(FGameplayTag AugmentTag, FHBSkillAugment& Out) const;

	// 지금까지 고른 증강 전부. 이름, 설명, 아이콘, 수치가 다 들어 있다.
	UFUNCTION(BlueprintPure, Category = "Augment")
	TArray<FHBSkillAugment> GetTakenAugmentInfos() const;

	// [폐기] float GetAugmentValue(FGameplayTag AugmentTag) const;
	//
	// 증강의 수치를 캐릭터에 묻던 함수. 항상 1.0 을 돌려주는 스텁이었다.
	// 없앤 이유: 캐릭터는 어느 스킬의 증강인지 모른다. 같은 이름의 증강이라도
	// 스킬마다 수치가 다르므로 캐릭터가 답할 수 있는 질문이 아니었다.
	// 지금은 스킬 GA 가 자기 Augments 배열에서 읽는다.
	//   UHBPlayerSkillGABase::GetAugmentParam(항목태그, 기본값)

	// 비용만큼 마나가 있는가. 클라도 읽어서 즉시 막는다.
	bool HasEnoughMana(float Cost) const;

	// 달리기의 초당 마나. 전역 보정 뒤 전투 호흡(SprintManaCostMultiplier)까지 곱한 값.
	float GetSprintManaCostPerSecond() const;

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
	// 흔들림 클래스는 하나이고, 세기는 아래 배율로 가른다.
	//
	// 왜 클래스를 셋으로 안 나눴나: 요구가 "세기를 조절 가능하게, 스태거 > 피격 > 방어"
	// 였고 그건 배율만으로 충족된다. 클래스를 셋으로 두면 BP 3개(BP_HBPlayerCharacter,
	// BP_HB_Warrior, BP_HB_Warrior_Figure)에 각각 꽂아야 하고, 하나라도 비면
	// 그 상황만 흔들리지 않는다.
	//
	// 쳐낼 조건: 상황마다 흔들림의 '결'이 달라야 하면(주기, 감쇠, 회전축)
	// 배율로는 안 된다. 그때 클래스를 상황별로 나눈다.
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	TSubclassOf<UCameraShakeBase> CS_HitReact;

	// 세기 순서: 스태거 > 피격 > 방어. 기본값이 곧 그 순서다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Shake", meta=(ClampMin="0.0"))
	float ShakeScale_Stagger = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Shake", meta=(ClampMin="0.0"))
	float ShakeScale_Hit = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Shake", meta=(ClampMin="0.0"))
	float ShakeScale_Guard = 0.25f;

	/**
	 * 방패를 들어올리는 동작. 패링과 방어태세가 같은 것을 쓴다.
	 *
	 * 왜 캐릭터에 두나: 두 어빌리티(GA_PlayerArry / GA_GuardStance)가 같은 몽타주를
	 * 쓰는데 둘 다 BP 없이 C++ 로 부여된다. 각자 들면 경로가 두 곳에 박히고
	 * 한쪽만 바꾸는 사고가 난다. 값의 주인을 여기 하나로 둔다.
	 *
	 * 기본값을 C++ 에서 박는 이유: 이 캐릭터 BP 가 셋(BP_HBPlayerCharacter,
	 * BP_HB_Warrior, BP_HB_Warrior_Figure)이라 BP 에서 지정하면 세 곳에 꽂아야 하고
	 * 하나만 비면 그 캐릭터만 동작이 안 나온다. 같은 파일의 ModularBodySourceMesh 가
	 * 이미 같은 방식이다.
	 *
	 * 임시 애셋이다. 설계상 패링은 "방패를 휘두르는" 동작인데 지금 있는 것은
	 * 구 방어가 쓰던 "들어올리는" 루프다.
	 *
	 * 쳐낼 조건: 전용 패링 동작이 만들어지면 패링과 방어태세의 몽타주를 나누고,
	 * 그때는 GA 별 BP 를 만들어 거기서 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Combat")
	TSoftObjectPtr<UAnimMontage> GuardMontage;

public:
	// 패링·방어태세 GA 가 부른다. 둘 다 캐릭터 밖이라 public 이어야 한다.
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	UAnimMontage* GetGuardMontage() const;

	// 경직 쉐이크는 GA_PlayerStagger 가 쏜다. 경직이 확정된 시점을 그쪽만 알기 때문이다.
	UFUNCTION(BlueprintPure, Category="Player|Combat|Shake")
	TSubclassOf<UCameraShakeBase> GetHitShakeClass() const { return CS_HitReact; }

	UFUNCTION(BlueprintPure, Category="Player|Combat|Shake")
	float GetStaggerShakeScale() const { return ShakeScale_Stagger; }

protected:
	
	/****BaseAbilities Stat Value***/
	// TODO:: Temp변수 데이터테이블로 변경 or AttributeData변경
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseAttackPower = 10.f;
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseAttackRange = 150.f;
	
	// 정면 판정에만 쓴다. 감소율은 없앴다 — 막으면 0, 못 막으면 전부다.
	UPROPERTY(EditDefaultsOnly,Category="Player|Combat")
	float BaseBlockingAngle = 120.f;

	// 방어구 내구도 감소량 = Clamp(FinalDamage * Ratio, Min, Max). 세게 맞을수록 더 닳는다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float ArmorDurabilityLossRatio = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float ArmorDurabilityLossMin = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float ArmorDurabilityLossMax = 10.f;

	// 방패 내구도 감소량 = Clamp(RawDamage * Ratio, Min, Max). 막아도 충격은 방패가 흡수한다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float ShieldDurabilityLossRatio = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float ShieldDurabilityLossMin = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float ShieldDurabilityLossMax = 15.f;

	// 주무기 내구도 감소량 = Clamp(적에게 실제로 들어간 Damage * Ratio, Min, Max).
	// 공격이 적중해야만(ResolveCreatureHit -> NotifyDamageDealt) 닳는다 - 헛스윙은 닳지 않는다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float WeaponDurabilityLossRatio = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float WeaponDurabilityLossMin = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Durability", meta=(ClampMin="0.0"))
	float WeaponDurabilityLossMax = 10.f;

	/** Development-only visualization: Warrior=red, Archer=green, Mage=blue. */
	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Debug")
	bool bDrawBasicAttackDebug = true;

	UPROPERTY(EditDefaultsOnly, Category="Player|Combat|Debug", meta=(ClampMin="0.05"))
	float BasicAttackDebugDuration = 1.f;


private:
	// 타격 연출을 쏜다. 막힘·패링이면 그에 맞는 큐로, 아니면 무기 성질 판정에 맡긴다.
	//
	// 왜 맞는 쪽이 쏘나: 막았는지는 여기서만 안다. 때리는 쪽은 무기 성질밖에 모른다.
	// 쳐낼 조건: 큐 종류가 더 늘면 결과 → 큐 매핑을 데이터로 뺀다.
	void PlayHitCue(AActor* InstigatorActor, const FHitResult& Hit, float FinalDamage, float BlockMultiplier, bool bParried) const;

	// 보조 슬롯의 방패 데이터. 없으면 nullptr.
	// 방패 조회가 두 곳에서 필요해서 한 곳으로 모았다.
	const struct FShieldData* GetEquippedShieldData() const;

	// 스킬 목록 하나를 훑어 증강 태그에 해당하는 항목을 찾는다.
	// GA 의 CDO 를 읽으므로 인스턴스가 없어도 되고, 그래서 static 이다.
	static bool FindAugmentInCatalog(const TArray<FHBJobSkillData>& Catalog, FGameplayTag AugmentTag, FHBSkillAugment& Out);

	// 패링에 당한 상대를 경직시킨다.
	// 작동방식: 상대 ASC 에서 Ability.HitReact 를 발동시킨다.
	// 적은 BP_GA_HitReact 가, 플레이어는 GA_PlayerStagger 가 그 태그로 잡힌다.
	// 슈퍼아머·사망 차단은 각 GA 의 ActivationBlockedTags 가 하므로 여기서 다시 보지 않는다.
	// 쳐낼 조건: 경직의 세기를 패링마다 다르게 하려면 태그가 아니라 GE 를 직접 걸어야 한다.
	void ApplyStaggerTo(AActor* Target) const;

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
	float ManaRegenDelay = 2.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float ManaRegenPerSecond = 10.f;

	// 일반공격이 마나를 안 쓰게 되며 5 에서 절반으로 줄였다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float SprintManaCostPerSecond = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float BlockManaCostPerSecond = 4.f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.05"))
	float ManaResourceTickInterval = 0.25f;

	FTimerHandle ManaResourceTimer;
	double LastManaActivityTime = 0.0;

	// ===== 체력 재생 =====
	// 서버 틱. (기본 재생 + 장비 +) × 배율 만큼 ReceiveHeal 로 채운다. 마나 회복과 같은 모양이다.
	void UpdateHealthRegen();

	// 기본 초당 체력 재생. 0 이면 장비 · 버프(Player_HealthRegenFlat)로만 찬다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float HealthRegenPerSecond = 0.f;

	// 맞은 뒤 재생이 멈추는 시간(초). 0 이면 멈추지 않는다.
	// 마나의 ManaRegenDelay 와 같은 역할. 기준 시각은 ApplyMeleeHit 가 기록하는 LastDamageReceivedTime.
	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.0"))
	float HealthRegenDelayAfterHit = 0.f;

	// 재생을 나눠 주는 간격(초). 작을수록 체력 바가 부드럽게 찬다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Resource", meta=(ClampMin="0.05"))
	float HealthRegenTickInterval = 0.25f;

	FTimerHandle HealthRegenTimer;

	// ===== 아이템 동작 효과 =====
	// 규칙은 Item/ItemEffectRules 에 있다. 캐릭터는 틱을 돌리고 상태만 든다.

	// 오라 · 조건 동안 · 주기 를 도는 서버 틱.
	void TickItemEffects();

	// 아이템 동작 효과를 확인하는 간격(초). 거리 · 체력 조건이 바뀌고 반영되기까지 최대 이만큼 늦는다.
	// 주기 효과의 쌓인 시간도 이 값만큼씩 늘어난다.
	UPROPERTY(EditDefaultsOnly, Category="Player|ItemEffect", meta=(ClampMin="0.05"))
	float ItemEffectTickInterval = 0.25f;

	FTimerHandle ItemEffectTimer;

	// 내가 건 핸들과 쌓인 시간. 죽을 때 · EndPlay 때 전부 거둔다.
	FItemEffectRuntime ItemEffectRuntime;
#pragma endregion 
	
#pragma region Movement
public:
	// 지금 달릴 수 있는가. 막는 상태와 마나를 한 곳에서 본다.
	//
	// 클라 예측(StartSprint)과 서버 검증(ServerSetSprinting), 그리고 달리는 도중
	// 상태가 걸렸을 때의 자동 해제(RefreshMovementSpeed) 셋이 같은 답을 봐야 한다.
	// 조건이 갈라지면 클라는 달리는데 서버는 아니라서 위치가 튄다.
	UFUNCTION(BlueprintPure, Category = "Player|Movement")
	bool CanSprint() const;

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

	// 포션 마시는 동작. 인벤토리의 사용 시작 → 끝까지 마시면 확정, 도중에 행동하면 취소.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Item")
	TObjectPtr<class UHBPotionDrinkComponent> PotionDrinkComp;

	// 상자·시체를 뒤지는 동작. 켜고 끄는 건 InteractionComponent(길게 누르기)와 컨트롤러(상자 창)가 한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Interaction")
	TObjectPtr<class UHBInteractAnimComponent> InteractAnimComp;
	
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

	/** Upper equipment replaces upper body, neck and hands; lower equipment replaces legs and feet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Modular Body")
	bool bUseModularEquipment = true;

	/** Only this source body has its body sections replaced. Other job meshes retain their appearance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Modular Body")
	TSoftObjectPtr<USkeletalMesh> ModularBodySourceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BaseBodyMeshes, Category="Player|Equipment|Temporary Test")
	TSoftObjectPtr<USkeletalMesh> BaseUpperBodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BaseBodyMeshes, Category="Player|Equipment|Temporary Test")
	TSoftObjectPtr<USkeletalMesh> BaseLowerBodyMesh;

	UFUNCTION()
	void OnRep_BaseBodyMeshes();

	bool CanChangeBaseBodyMeshes() const;
	bool IsCompatibleBaseBodyMesh(const TSoftObjectPtr<USkeletalMesh>& BodyPart) const;

	bool RefreshModularEquipmentVisuals();
	void SetSourceBodySectionsVisible(bool bVisible);
	bool bModularBodyActive = false;

public:
	/** Change the unequipped upper group; equipped upper armor remains visible until removed. Call on the server in game. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Player|Equipment|Modular Body")
	bool SetBaseUpperBodyMesh(TSoftObjectPtr<USkeletalMesh> UpperBody);

	/** Change the unequipped lower group without changing upper equipment. Call on the server in game. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Player|Equipment|Modular Body")
	bool SetBaseLowerBodyMesh(TSoftObjectPtr<USkeletalMesh> LowerBody);

	/** Replace both combined body references together, preserving the currently equipped items. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Player|Equipment|Modular Body")
	bool SetBaseBodyMeshes(TSoftObjectPtr<USkeletalMesh> UpperBody, TSoftObjectPtr<USkeletalMesh> LowerBody);

protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Player|Equipment|Helmet")
	FName HairMaterialSlotName = TEXT("MAT_M_CHAMP_HAIR");

	void SetHairVisibleForHelmet(bool bVisible);



	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Underwear")
	bool bUnderwearVisible = true;

	UFUNCTION(BlueprintCallable, Category="Player|Equipment|Underwear")
	void SetUnderwearVisible(bool bVisible);

	virtual void OnConstruction(const FTransform& Transform) override;
	#pragma endregion

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName WeaponSocketName = TEXT("PrimaryWeaponSocket");
	
	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment")
	FName ShieldSocketName = TEXT("SecondWeaponSocket");
	
	virtual void BeginPlay() override;

	// 나가거나 파괴될 때 아이템 동작 효과로 파티원에게 건 것을 거둔다. 안 거두면 버프가 영원히 남는다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void RefreshEquippedVisuals();
	
	void RefreshWeaponVisual();
	void RefreshShieldVisual();
	void RefreshHelmetVisual();
	void RefreshChestVisual();
	void RefreshLegsVisual();

	// TODO: Replace this temporary visual test with item data after coordinating the equipment data format.
	#pragma region TemporaryEquipmentVisualTest
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test")
	bool bEnableEquipmentVisualTest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestHelmetMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestChestMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestLegsMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<USkeletalMesh> TestShoesMesh;

	// TODO: Remove the combined-mesh override after the Blender fitting test is complete.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	bool bUseCombinedPantsBootsTest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest && bUseCombinedPantsBootsTest"))
	TSoftObjectPtr<USkeletalMesh> TestCombinedPantsBootsMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<UStaticMesh> TestWeaponMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Temporary Test", meta=(EditCondition="bEnableEquipmentVisualTest"))
	TSoftObjectPtr<UStaticMesh> TestShieldMesh;

	UFUNCTION(BlueprintCallable, Category="Player|Equipment|Temporary Test")
	void ApplyTemporaryEquipmentVisualTest();
	#pragma endregion

#pragma endregion 

public:
	UFUNCTION(Exec, Category = "Debug")
	void TestDamage(float Amount);

	// 테스트용: 지정한 장비 슬롯 내구도를 즉시 0(파손)으로 만든다.
	// Slot 번호(EEquipSlotType 순서): 1=무기 2=방패 3=투구 4=몸통 5=바지 6=악세서리
	// 클라이언트 창 콘솔에서 쳐도 서버로 전달되어 실제로 적용된다.
	UFUNCTION(Exec, Category = "Debug")
	void TestBreakEquipment(int32 Slot);

	// 테스트용: 경험치를 즉시 지급한다. 레벨이 오르면 서버가 증강 후보를 뽑으므로
	// 적을 잡지 않고도 증강 선택 화면을 열어볼 수 있다.
	// 클라이언트 창 콘솔에서 쳐도 서버로 전달되어 실제로 적용된다.
	UFUNCTION(Exec, Category = "Debug")
	void TestAddExp(int32 Amount);

	// 테스트용: 패시브를 증강 없이 즉시 부여한다.
	//   예) TestGrantPassive Warrior.GuardStance
	//
	// 왜 남기나: 증강 UI 는 이제 RequestSelectUpgrade 까지 연결됐지만,
	// 카드는 무작위라 특정 패시브를 겨냥해 검증할 수 없다. TestAddExp 가
	// "정상 경로 전체" 를 보는 용도라면 이쪽은 "이 패시브 하나" 를 보는 용도다.
	//
	// 직업 데이터의 PassiveSkills 에 있는 ID 만 통한다. 이미 가진 것은 무시된다.
	UFUNCTION(Exec, Category = "Debug")
	void TestGrantPassive(FName PassiveSkillId);

protected:
	UFUNCTION(Server, Reliable)
	void ServerTestBreakEquipment(int32 Slot);

	UFUNCTION(Server, Reliable)
	void ServerTestAddExp(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerTestGrantPassive(FName PassiveSkillId);

	// C++에서 호출하면 블루프린트에서 실행되는 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void BP_OnPlayerStateReady();
};
