#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "HBTrapBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UPrimitiveComponent;

// 누구를 때리나.
UENUM(BlueprintType)
enum class EHBTrapTargets : uint8
{
	Both        UMETA(DisplayName = "둘 다"),
	PlayersOnly UMETA(DisplayName = "플레이어만"),
	EnemiesOnly UMETA(DisplayName = "적만"),
};

// 언제 켜지나.
UENUM(BlueprintType)
enum class EHBTrapTriggerMode : uint8
{
	Always   UMETA(DisplayName = "항상"),            // 계속 도는 칼날
	Interval UMETA(DisplayName = "N초마다"),          // 주기 화살 · 주기 가시
	Volume   UMETA(DisplayName = "범위에 들어오면"),   // 발판 · 근접 감지
	Manual   UMETA(DisplayName = "BP 가 직접"),        // 레버 등. BP 에서 Activate 를 부른다
};

// 발판이 무엇에 눌리나.
UENUM(BlueprintType)
enum class EHBTrapTriggerSense : uint8
{
	Detection UMETA(DisplayName = "감지 (센서)"),     // 살아 있는 대상만 감지한다
	Pressure  UMETA(DisplayName = "무게 (압력판)"),   // 올라선 캐릭터면 살았든 죽었든 눌린다
};

// 지금 어느 단계인가. 서버가 정하고 복제한다.
UENUM(BlueprintType)
enum class EHBTrapPhase : uint8
{
	Idle     UMETA(DisplayName = "대기"),
	Warning  UMETA(DisplayName = "예고"),
	Active   UMETA(DisplayName = "작동"),
	Cooldown UMETA(DisplayName = "재정비"),
};

/**
 * 트랩의 공통 판정. 레벨 디자이너가 BP 자식을 만들어 모델 · 애니메이션을 붙인다.
 *
 * 왜 액터에 ASC 를 두나: 피격 쪽이 이미 "공격자 ASC 의 태그" 로 가드 불가 · 경직 · 타격 큐를 고른다
 * (AHBPlayerCharacter::ApplyMeleeHit, HitCuePlay). 트랩 ASC 에 그 태그를 달면 피격 코드를 고치지 않고 그대로 탄다.
 *
 * 작동방식:
 *   - 판정: HitboxTag 를 단 충돌체에 대상이 닿아 있으면, 켜져 있는 동안 HitInterval 마다 효과를 준다.
 *     움직이는 부위(칼날 · 가시)에 충돌 박스를 붙이면 모델이 어떻게 생겼든 상관없다.
 *   - 발동: 예고(WarningDuration) → 작동(ActiveDuration) → 재정비(RearmDelay) → 대기.
 *   - 연출: 단계가 바뀌면 모든 화면에서 BP 이벤트가 불린다. 애니메이션은 BP 가 재생한다.
 *   - 판정과 단계 결정은 서버만 한다.
 *
 * 쳐낼 조건: 판정이 크게 다른 트랩(끌어당기기 등)이 생기면 그 종류만 자식 클래스로 뺀다.
 */
UCLASS(Blueprintable)
class HEAVENBREACH_API AHBTrapBase : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AHBTrapBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용. Manual 모드나 BP 연출에서 켠다. 대기 중이 아니면 무시한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Trap")
	void Activate();

	// 서버 전용. 작동 중이면 끈다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Trap")
	void Deactivate();

	// 대상에게 피해 · 상태이상을 준다. 서버 전용. 2단계의 트랩 발사체도 이걸 부른다.
	void ApplyEffectTo(AActor* Target, const FHitResult& Hit);

	// 이 트랩이 때리는 대상인가. 살아 있는 플레이어 · 적만.
	// const 가 아닌 이유: CanBeDamaged 에 트랩 자신을 공격자로 넘긴다.
	bool IsTargetAllowed(const AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Trap")
	EHBTrapPhase GetPhase() const { return Phase; }

protected:
	virtual void BeginPlay() override;

	// 모든 화면에서 불린다. 여기서 모델 애니메이션 · 소리를 재생한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Trap")
	void BP_OnTrapWarning();

	UFUNCTION(BlueprintImplementableEvent, Category = "Trap")
	void BP_OnTrapActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Trap")
	void BP_OnTrapDeactivated();

	// ----- 효과 -----

	// 0 이면 피해 없이 상태이상만 준다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect", meta = (ClampMin = "0.0"))
	float Damage = 10.f;

	// 맞은 대상에게 거는 GE (둔화 · 독 등). 지속 피해는 GE 의 Period 로 만든다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect")
	TArray<TSubclassOf<UGameplayEffect>> StatusEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect")
	EHBTrapTargets Targets = EHBTrapTargets::Both;

	// 판정 안에 머무는 대상을 다시 때리는 간격. 켜질 때마다 새로 센다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect", meta = (ClampMin = "0.05"))
	float HitInterval = 1.f;

	// 막기 · 패링을 건너뛴다. 대부분의 트랩은 막을 수 없다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect")
	bool bUnblockable = true;

	// 막지 못하고 맞은 플레이어를 경직시킨다 (GA_PlayerStagger).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect")
	bool bStagger = false;

	// 타격 이펙트 · 소리. 비우면 기본 타격 큐.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Effect", meta = (Categories = "GameplayCue.Hit"))
	FGameplayTag HitCueTag;

	// 이 태그가 붙은 충돌체가 판정을 한다. 칼날 · 가시처럼 움직이는 부위에 충돌 박스를 붙이고 태그를 단다.
	UPROPERTY(EditDefaultsOnly, Category = "Trap|Effect")
	FName HitboxTag = TEXT("TrapHitbox");

	// 판정 안에 머무는 대상을 확인하는 주기. HitInterval 보다 작아야 간격이 밀리지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "Trap|Effect", meta = (ClampMin = "0.02"))
	float OverlapCheckRate = 0.1f;

	// ----- 발동 -----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Trigger")
	EHBTrapTriggerMode TriggerMode = EHBTrapTriggerMode::Always;

	// N초마다 모드의 주기.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Trigger", meta = (ClampMin = "0.1"))
	float Interval = 3.f;

	// 켜지기 전 예고 시간. 밟고 나서 가시가 튀어나오기까지. 0 이면 바로 켜진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Trigger", meta = (ClampMin = "0.0"))
	float WarningDuration = 0.f;

	// 켜져 있는 시간. 0 이면 Deactivate 를 부를 때까지 계속. 항상 모드는 이 값을 보지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Trigger", meta = (ClampMin = "0.0"))
	float ActiveDuration = 1.f;

	// 꺼진 뒤 다시 켜질 수 있기까지.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Trigger", meta = (ClampMin = "0.0"))
	float RearmDelay = 2.f;

	// 범위에 들어오면 모드에서, 이 태그가 붙은 충돌체에 대상이 들어오면 켜진다 (발판 · 감지 범위).
	UPROPERTY(EditDefaultsOnly, Category = "Trap|Trigger")
	FName TriggerVolumeTag = TEXT("TrapTrigger");

	// 범위에 들어오면 모드에서 발판이 무엇에 반응하나. 무게면 시체가 올라가 있는 동안 계속 다시 작동한다.
	// 판정(피해 · 상태이상)은 어느 쪽이든 살아 있는 대상만 받는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trap|Trigger")
	EHBTrapTriggerSense TriggerSense = EHBTrapTriggerSense::Detection;

	UPROPERTY(VisibleAnywhere, Category = "Trap")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComp;

private:
	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	EHBTrapPhase Phase = EHBTrapPhase::Idle;

	UFUNCTION()
	void OnRep_Phase(EHBTrapPhase OldPhase);

	// 서버. 단계를 바꾸고 서버 화면에도 BP 이벤트를 부른다.
	void SetPhase(EHBTrapPhase NewPhase);
	void BroadcastPhase(EHBTrapPhase OldPhase);

	void PrepareCollision();
	void BindCollision();
	void RequestActivation();
	void BeginActive();
	void EndActive();
	void FinishCooldown();
	void HitAllOverlapping();
	void TryHit(AActor* Target, const FHitResult& Hit);
	void ApplyStatusEffects(AActor* Target, const FHitResult& Hit);

	// Targets 설정(플레이어 / 적 / 둘 다)에 맞는 종류인가. 살았는지는 보지 않는다.
	bool MatchesTargetKind(const AActor* Actor) const;

	// 이 대상이 발판을 누르나. TriggerSense 에 따라 시체를 포함할지 갈린다.
	bool CanPressTrigger(const AActor* Actor);
	FHitResult MakeHitFor(const AActor* Target, const UPrimitiveComponent* Hitbox) const;

	UFUNCTION()
	void HandleHitboxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Hitboxes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> TriggerVolumes;

	// 대상별 마지막으로 때린 시각. 켜질 때마다 비운다.
	TMap<TWeakObjectPtr<AActor>, double> LastHitTimes;

	FTimerHandle IntervalTimer;
	FTimerHandle PhaseTimer;
	FTimerHandle OverlapCheckTimer;
};
