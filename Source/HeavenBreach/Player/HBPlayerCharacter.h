#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "GameplayAbilitySpecHandle.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Combat/Ability/HBSkillAugment.h"
#include "Item/ItemEffectRules.h"
#include "Inventory/InventoryTypes.h"
#include "Interaction/InteractableInterface.h"
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
class UPointLightComponent;
class UParticleSystemComponent;
class UParticleSystem;
class UQuickSlotComponent;
class UDataTable;
struct FHBWeaponPresentation;
class UHBPlayerCorpseInventory;

USTRUCT()
struct FHBCorpseEquippedItem
{
	GENERATED_BODY()

	UPROPERTY()
	EEquipSlotType Slot = EEquipSlotType::None;

	UPROPERTY()
	FItemInstance Item;
};

// 시체가 된 순간의 상태. 한 구조체로 묶어야 "시체다"와 "입고 있던 장비"가 같이 도착한다.
USTRUCT()
struct FHBCorpseState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsCorpse = false;

	// 외형 전용. 실제 아이템은 시체 가방에 있다.
	UPROPERTY()
	TArray<FHBCorpseEquippedItem> EquippedSnapshot;
};

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerCharacter : public ACharacter,public IDamageableInterface,public IAbilitySystemInterface,public IInteractableInterface

{
	GENERATED_BODY()

public:
	AHBPlayerCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용. 아이템을 시체 가방으로 옮기고 래그돌이 된다. 빙의 해제 전에 불러야 PlayerState 가 붙어 있다.
	void BecomeCorpse();

	bool IsCorpse() const { return CorpseState.bIsCorpse; }

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;

	void MoveForward(float Value);
	void MoveRight(float Value);

	UFUNCTION(BlueprintCallable, Category = "Player|Combat")
	virtual void Attack();

	// 공격 키를 뗐을 때. 활을 당기는 중이면 쏜다(당긴 시간이 짧으면 취소). 근접 평타는 영향 없음.
	void StopAttack();

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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Corpse")
	TObjectPtr<UHBPlayerCorpseInventory> CorpseInventory;

	UPROPERTY(ReplicatedUsing = OnRep_CorpseState)
	FHBCorpseState CorpseState;

	UFUNCTION()
	void OnRep_CorpseState();

	void StartCorpseRagdoll();

	// 장비 외형이 볼 곳. 시체면 스냅샷, 아니면 PlayerState 의 장비.
	bool HasVisualEquipmentSource() const;
	bool GetVisualItemInSlot(EEquipSlotType Slot, FItemInstance& OutItem) const;

public:

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

	// 장착 무기의 외형 행(DT_WeaponPresentation). 장비가 바뀔 때 갱신해 둔 값이다. 없으면 nullptr.
	const FHBWeaponPresentation* GetWeaponPresentation() const { return CachedWeaponPresentation; }

	// 외형 행에 스켈레탈 메시가 있는 무기(활)를 들었을 때 보이는 메시. 아니면 숨겨져 있다.
	USkeletalMeshComponent* GetWeaponSkeletalMesh() const { return WeaponSkeletalMeshComp; }

	// 활을 당기고 있는가. 활 AnimBP · 장전 화살 · 다른 클라 화면이 이걸 본다.
	UFUNCTION(BlueprintPure, Category = "Player|Bow")
	bool IsBowDrawing() const { return bDrawingBow; }

	// 활 GA 가 서버와 소유 클라에서 부른다. 다른 클라는 복제로 받는다.
	void SetBowDrawing(bool bDrawing);

	// 활 스킬(관통·표식·회피) 동작 중인가. 활 AnimBP 가 스킬 동안 평타용 시위 보정을 끈다(기문 님 테스트의 bBowSkillActive).
	UFUNCTION(BlueprintPure, Category = "Player|Bow")
	bool IsInBowSkillPose() const { return bInBowSkillPose; }

	// 활 스킬 GA 가 서버와 소유 클라에서 부른다. 다른 클라는 복제로 받는다.
	void SetInBowSkillPose(bool bInPose);

	// 관통 사격을 당기는 중인가. 장전 화살의 관통 기류·입자(컴포넌트 태그 PiercingCharge)가 이 동안 켜진다.
	bool IsBowPiercingCharge() const { return bBowPiercingCharge; }

	// 관통 사격 GA 가 서버와 소유 클라에서 부른다. 다른 클라는 복제로 받는다.
	void SetBowPiercingCharge(bool bCharge);

	// 장전 중 손에 보이는 화살 메시. 외형 행에 없으면 nullptr.
	UFUNCTION(BlueprintPure, Category = "Player|Bow")
	UStaticMesh* GetHeldArrowMesh() const;

	// 화면 중앙(카메라)에서 조준 방향으로 쏜 트레이스의 끝. 맞은 곳이 있으면 그 지점.
	// 장전 화살 표시와 서버의 화살 발사가 같은 값을 쓴다.
	UFUNCTION(BlueprintPure, Category = "Player|Bow")
	FVector ComputeAimPoint(float MaxDistance = 10000.f) const;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
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
	void HandleItemUsed(FName ItemId, float HealAmount, float ManaAmount);

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

	// 머리 흔들림이 화면에 들어가는 세기(BP 기본값 x 설정 메뉴). 1 이면 아무것도 안 한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Camera")
	TObjectPtr<class UHBCameraMotionComponent> CameraMotionComp;

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

	// 마나 회복은 전부 여기를 지난다. ReceiveHeal 과 같은 이유 - 포션 외에 다른 마나 회복이
	// 생겨도 전부 이걸 부르면 한 곳에서만 관리하면 된다.
	void ReceiveMana(float Amount);

	UFUNCTION(BlueprintPure, Category = "Player|Combat")
	float GetAttackRange() const;
	
	// 정면 판정 각도. 패링과 방어태세가 같은 값을 쓴다.
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	float GetBlockAngle() const;

	// 보조 슬롯에 방패가 있는가. 패링의 유일한 전제조건이다.
	UFUNCTION(BlueprintPure, Category="Player|Combat")
	bool HasShieldEquipped() const;

	// 데미지 처리. 패시브나 액티브등 데미지보너스 포함
	// bCreditCurrentSkillCast: 즉발 스킬 피해면 지금 시전에 잔류 마력을 알린다. 투사체는 자기 시전 ID 로
	// 따로 알리므로 false 로 부른다(화살이 날아가는 사이 시작한 다른 시전이 공짜로 트리거되지 않게).
	float ModifyOutgoingDamage(AActor* Target, float Damage, bool bCreditCurrentSkillCast = true);

	// ---- 시전 ID (잔류 마력, 마법스킬 설계 2.2-7) ----
	// 서버 전용. 액티브 발동마다 새 ID 를 준다. 0 은 "스킬 아님(평타)" 이라 1부터.
	int32 BeginSkillCast() { return ++SkillCastCounter; }

	// 가장 최근에 시작한 시전의 ID. 즉발 피해(State.UsingSkill 중 ModifyOutgoingDamage)가 쓴다.
	int32 GetCurrentSkillCastId() const { return SkillCastCounter; }

	// 서버 전용. 이 시전이 커밋(비용·쿨다운)됐다. 즉발 피해 크레딧은 발동한 시전에만 준다
	// — 캐스팅 중(발동 전) 다른 피해가 들어가도 지금 시전이 트리거되지 않게.
	void MarkSkillCastReleased(int32 CastId) { LastReleasedSkillCastId = CastId; }

	// 서버 전용. 액티브 스킬 피해가 적에게 들어갔다. 잔류 마력이 있으면 시전 1번에 1회
	// 장착 액티브 스킬의 쿨다운 GE 를 줄인다. 투사체는 들고 다니던 ID 로 부른다.
	void NotifySkillHit(int32 CastId);
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

	// 증강의 현재 단계. 못 골랐거나 PlayerState 가 없으면 0.
	UFUNCTION(BlueprintPure, Category = "Augment")
	int32 GetAugmentTier(FGameplayTag AugmentTag) const;

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
	// 조준 입력을 받는 활성 스킬의 스펙. State.Bow.SkillAiming 이 붙어 있고
	// UHBPlayerActiveGABase::WantsAimInput() 이 true 인 활성 스펙. 없으면 nullptr.
	struct FGameplayAbilitySpec* FindActiveAimingSkillSpec() const;

	// 캐스팅 중(State.Casting)인 활성 UHBPlayerCastGABase 스펙. 우클릭 취소용. 없으면 nullptr.
	struct FGameplayAbilitySpec* FindActiveCastingSkillSpec() const;

	// 서버 전용 시전 ID 카운터. BeginSkillCast 가 올린다.
	int32 SkillCastCounter = 0;

	// 서버 전용. 마지막으로 커밋된 시전 ID. MarkSkillCastReleased 가 정한다.
	int32 LastReleasedSkillCastId = 0;

	// 잔류 마력이 이미 쿨다운을 줄인 최근 시전 ID 들(HBMagic::MaxResidualConsumed 개). 같은 시전의 두 번째 적중은 무시한다.
	TArray<int32> ResidualConsumedCastIds;

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

	// 대상을 경직시킨다. 두 곳에서 부른다 — 패링에 당한 상대, 그리고 State.Attacking.Stagger 공격에 맞은 나 자신.
	// 작동방식: 대상 ASC 에서 Ability.HitReact 를 발동시킨다.
	// 적은 BP_GA_HitReact 가, 플레이어는 GA_PlayerStagger 가 그 태그로 잡힌다.
	// 슈퍼아머·사망 차단은 각 GA 의 ActivationBlockedTags 가 하므로 여기서 다시 보지 않는다.
	// 쳐낼 조건: 경직의 세기를 패링마다 다르게 하려면 태그가 아니라 GE 를 직접 걸어야 한다.
	void ApplyStaggerTo(AActor* Target) const;

	// 공격한 쪽 ASC 에 이 공격 성질 태그(State.Attacking.*)가 붙어 있나. 공격자가 없으면 false.
	// 발사체는 날아가는 동안 공격 GA 가 끝나 태그가 없을 수 있다 — 지금은 근접 · 잡기 · 빔용.
	bool HasAttackTag(const AActor* Attacker, const FGameplayTag& Tag) const;

	void ResetCombatInstinct();

	TWeakObjectPtr<AActor> ConsecutiveTarget;

	// 활 당김 상태. 소유 클라는 GA 가 직접 세팅하므로 복제에서 뺀다(COND_SkipOwner).
	// 이름을 bBowDrawing 으로 하지 않는다: 기문 님 테스트 BP(BP_Test_BowCharacter, 이 클래스의 자식)에
	// 같은 이름의 BP 변수가 있어 충돌하면 그쪽 변수가 bBowDrawing_0 으로 바뀌어 테스트가 깨진다.
	UPROPERTY(Replicated)
	bool bDrawingBow = false;

	// 활 스킬 동작 중. bDrawingBow 와 같은 방식(COND_SkipOwner).
	// 이름을 bBowSkillActive 로 하지 않는다: BP_Test_BowCharacter(이 클래스의 자식)에 같은 이름의 BP 변수가 있다.
	UPROPERTY(Replicated)
	bool bInBowSkillPose = false;

	// 관통 사격 당김 중. bDrawingBow 와 같은 방식(COND_SkipOwner). 바뀌면 장전 화살 연출을 맞춘다.
	UPROPERTY(ReplicatedUsing = OnRep_BowPiercingCharge)
	bool bBowPiercingCharge = false;

	UFUNCTION()
	void OnRep_BowPiercingCharge();

	// 장전 화살에서 태그 PiercingCharge 가 붙은 컴포넌트를 bBowPiercingCharge 에 맞춘다(보이기 + 나이아가라 켜고 끄기).
	void ApplyBowPiercingChargeVisual();

	// 정적 무기 표시의 처음 상대 위치. 외형 행이 위치를 바꾼 무기에서 다른 무기로 바꿀 때 되돌린다.
	FTransform DefaultWeaponMeshRelative;
	bool bHasDefaultWeaponMeshRelative = false;

	// 무기에 따른 교체 전의 AnimBP(BP 기본값 또는 직업 데이터). 무기를 내려놓으면 이걸로 돌아간다.
	UPROPERTY()
	TSubclassOf<UAnimInstance> BaseAnimClass;

	// DataTable 행 포인터. 표는 에디터 애셋이라 게임 중에 행이 옮겨지지 않는다.
	const FHBWeaponPresentation* CachedWeaponPresentation = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> HeldArrowActor;
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

	// 복제되는 값이라 남의 캐릭터에서도 맞다. 발소리가 걷기·달리기를 가르는 데 쓴다.
	bool IsSprinting() const { return bIsSprinting; }

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

	// ===== 관성 =====
	// 엔진 기본값(가속·제동 2048, 마찰 8)은 0.1초 안에 붙고 멈춰 관성이 없다.
	// 아래 기본값: 걷기 속도까지 약 0.3초, 걷다 멈추면 약 0.24초, 달리다 멈추면 약 0.4초 미끄러진다.

	// 출발 가속. 작을수록 늦게 붙는다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Inertia", meta=(ClampMin="1.0"))
	float InertiaAcceleration = 1000.f;

	// 입력을 놓았을 때 고정 감속. 작을수록 멀리 미끄러진다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Inertia", meta=(ClampMin="0.0"))
	float InertiaBrakingDeceleration = 1000.f;

	// 입력을 놓았을 때 속도 비례 감속(곱하기 2 가 되어 들어간다). 빠를수록 더 세게 잡는다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Inertia", meta=(ClampMin="0.0"))
	float InertiaBrakingFriction = 1.f;

	// 방향 전환 시 옆으로 밀리는 정도. 작을수록 크게 돌아 나간다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Inertia", meta=(ClampMin="0.0"))
	float InertiaGroundFriction = 4.f;

	// 무게 배율. 1 이 기본, 클수록 느리게 붙고 멀리 미끄러진다. 위 값을 전부 이것으로 나눈다.
	// 기획서에 무게 시스템은 아직 없다. 장비 무게 같은 것이 생기면 여기에 넣는다.
	// 런타임에 바꾸게 되면 클라 예측과 어긋나지 않게 복제해야 한다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Inertia", meta=(ClampMin="0.1"))
	float MovementWeight = 1.f;

	// 위 값을 이동 컴포넌트에 넣는다. RefreshMovementSpeed 가 부른다.
	void ApplyMovementInertia(class UCharacterMovementComponent* Movement) const;

	// ----- 점프 감각 (Docs/점프-감각-설계.md) -----
	// 서버와 소유 클라가 같은 규칙으로 각자 계산한다. 복제 값은 없다.

	// 정점을 지나 내려올 때만 중력에 곱한다. 오를 때는 그대로라 점프 높이는 안 바뀐다. 둥둥 뜨는 느낌을 없앤다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="1.0"))
	float FallGravityScale = 1.6f;

	// 착지 직전 하강 속도가 이 이하면 충격 0(보통 점프 ≈ 530). 이 위로는 비례해서 커진다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="0.0", Units="cm/s"))
	float LandSoftSpeed = 600.f;

	// 이 속도 이상으로 떨어지면 충격 1(최대).
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="0.0", Units="cm/s"))
	float LandHardSpeed = 1100.f;

	/** 이 높이(cm)까지는 떨어져도 피해가 없다. 공중 최고점 − 착지 지점 */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement|FallDamage", meta = (ClampMin = "0.0", Units = "cm"))
	float FallDamageSafeHeight = 320.f;

	/** 이 높이(cm) 이상이면 최대 체력 100% 피해(즉사). 사이는 직선 */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement|FallDamage", meta = (ClampMin = "0.0", Units = "cm"))
	float FallDamageLethalHeight = 1150.f;

	/** 감소 수단을 다 겹쳐도 이 비율 이상은 줄지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement|FallDamage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FallDamageMaxReduction = 0.9f;

	// 충격 1 일 때 착지 순간 줄어드는 이동 속도 비율. 0.4 면 60% 속도에서 시작해 회복.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LandMaxSlow = 0.4f;

	// 착지 감속이 원래 속도로 돌아오는 시간.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="0.0", Units="s"))
	float LandSlowDuration = 0.35f;

	// 착지 후 이 시간 안에 다시 뛰면 점프가 낮다(연속 점프 억제). 막지는 않는다.
	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="0.0", Units="s"))
	float RejumpWindow = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category="Player|Movement|Jump", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RejumpHeightScale = 0.75f;

	// 이동 컴포넌트의 원래 값. 공중·착지 처리 동안 바꿨다가 되돌린다.
	float BaseJumpZVelocity = 0.f;
	float BaseGravityScale = 1.f;

	// 마지막 착지 시각(월드 시간)과 그때의 충격 0~1.
	double LastLandedTime = -1000.0;
	float LastLandImpact = 0.f;

	/** 서버 전용. 이번 공중 상태에서 가장 높았던 Z. 공중 시작·순간이동 때 현재 Z 로 초기화 */
	float FallApexZ = 0.f;

	// 공중이거나 착지 회복 중이면 true. 이때만 틱을 켜 둔다.
	bool NeedsJumpFeelTick() const;

	// 점프 감각 값(중력·점프 높이)을 이동 컴포넌트에 넣는다. 공중·착지 회복 중 매 틱.
	void UpdateJumpFeel();

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

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment|Torch")
	TObjectPtr<UPointLightComponent> TorchLightComp;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment|Torch")
	TObjectPtr<UParticleSystemComponent> TorchFireComp;

	// 시위가 움직이는 무기(활). 외형 행에 SkeletalMesh 가 있을 때만 보이고, 그때 WeaponMeshComp 는 비운다.
	// "Weapon" 컴포넌트 태그를 달지 않는다 — 근접 트레이스 대상이 아니다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> WeaponSkeletalMeshComp;

	// 무기별 외형·애니 표. 행 구조체 FHBWeaponPresentation. 비어 있으면 예전처럼 스태틱 무기만 보인다.
	UPROPERTY(EditDefaultsOnly, Category = "Player|Equipment")
	TObjectPtr<UDataTable> WeaponPresentationTable;

	// 장전 중 손에 보이는 화살 액터(BP_BowHeldArrow). 모든 머신이 각자 로컬로 만든다. 복제하지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Player|Bow")
	TSubclassOf<AActor> HeldArrowActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Combat")
	TObjectPtr<class UMeleeTraceComponent> MeleeTraceComp;

	// 포션 마시는 동작. 인벤토리의 사용 시작 → 끝까지 마시면 확정, 도중에 행동하면 취소.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Item")
	TObjectPtr<class UHBPotionDrinkComponent> PotionDrinkComp;

	// 상자·시체를 뒤지는 동작. 켜고 끄는 건 InteractionComponent(길게 누르기)와 컨트롤러(상자 창)가 한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Interaction")
	TObjectPtr<class UHBInteractAnimComponent> InteractAnimComp;

	// 걷기·달리기 발소리. 사운드는 BP 에서 넣는다. 비어 있으면 소리가 안 난다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Movement")
	TObjectPtr<class UHBFootstepComponent> FootstepComp;
	
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

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Player|Equipment")
	TObjectPtr<USkeletalMeshComponent> BodyUnderlayMeshComp;

	/** Upper equipment replaces upper body, neck and hands; lower equipment replaces legs and feet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Modular Body")
	bool bUseModularEquipment = true;

	/** Only this source body has its body sections replaced. Other job meshes retain their appearance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Modular Body")
	TSoftObjectPtr<USkeletalMesh> ModularBodySourceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Player|Equipment|Modular Body")
	TArray<TSoftObjectPtr<USkeletalMesh>> BodyUnderlayChestMeshes;

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

	virtual bool TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest = false, bool bNoCheck = false) override;

	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;

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

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch")
	FVector TorchFlameOffset = FVector(0.f, 0.f, 49.f);

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch", meta = (ClampMin = "0"))
	float TorchLightIntensity = 800.f;

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch")
	FLinearColor TorchLightColor = FLinearColor(1.f, 0.55f, 0.2f);

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch")
	TSoftObjectPtr<UParticleSystem> TorchFireTemplate;

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch", meta = (ClampMin = "0.01"))
	float TorchHandScale = 0.35f;

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch")
	FName TorchSocketName = TEXT("TorchSocket");

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch")
	FVector TorchHandOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly,Category="Player|Equipment|Torch")
	FRotator TorchHandRotation = FRotator::ZeroRotator;

	FTransform ShieldBaseRelativeTransform;
	bool bShieldBaseTransformCaptured = false;
	void ApplySecondaryEquipTransform(bool bTorch);
	
	virtual void BeginPlay() override;

	// 점프 감각: 공중·착지 회복 중에만 켜진다.
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;

	/** 서버 전용. 낙하 높이로 피해를 준다. 피해가 0 이면 아무것도 안 한다 */
	void ApplyFallDamage(float FallHeight);

	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	/** 서버 전용. 맞은 본인 화면에 카메라 흔들림·HP 바·피 효과를 보낸다 */
	void SendHitFeedback(float ShakeScale, bool bSuppressScreenEffect);
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	// 나가거나 파괴될 때 아이템 동작 효과로 파티원에게 건 것을 거둔다. 안 거두면 버프가 영원히 남는다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void RefreshEquippedVisuals();
	
	void RefreshWeaponVisual();

	// 외형 행의 CharacterAnimClass 로 AnimBP 를 바꾼다. 없으면 BaseAnimClass 로 되돌린다.
	void RefreshCharacterAnimClass();

	void RefreshShieldVisual();
	void RefreshTorchLight(float InLightRadius);
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

	// 테스트용: 로비 없이 Q/E 스킬을 바꾼다. 패시브는 지금 것을 유지한다.
	//   예) TestSetSkills Bow.PiercingShot Bow.HunterMark
	// 직업 데이터의 ActiveSkills 에 있는 ID 만 통한다(RequestApplyLoadout 이 검사).
	UFUNCTION(Exec, Category = "Debug")
	void TestSetSkills(FName SkillQId, FName SkillEId);

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
