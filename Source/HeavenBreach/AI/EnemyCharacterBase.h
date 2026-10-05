// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Interaction/InteractableInterface.h"
#include "EnemyCharacterBase.generated.h"

class UEnemyAbilitySystemComponent;
class UEnemyAttributeSetBase;
class UEnemyArchetypeData;
class UGameplayEffect;
struct FGameplayTag;
struct FDataTableRowHandle;
class ULootComponent;
class AEnemyCharacterBase;

// 죽었다. 소환 명부처럼 남의 사망을 알아야 하는 쪽이 구독한다.
//
// 왜 델리게이트인가: BecomeDead 안에 구독자를 직접 호출로 박으면 적 베이스가
// 소환 · 연출 · 퀘스트를 전부 알게 된다. 알리기만 하고 끝낸다.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemyDied, AEnemyCharacterBase*);

UCLASS()
class HEAVENBREACH_API AEnemyCharacterBase : public ACharacter, public IAbilitySystemInterface, public IDamageableInterface, public IInteractableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEnemyCharacterBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ----- IDamageableInterface -----
	virtual bool CanBeDamaged_Implementation(AActor* InstigatorActor) const override;
	virtual void ApplyMeleeHit_Implementation(AActor* InstigatorActor, float RawDamage, const FHitResult& Hit, EHBDamageSource Source) override;

	// ----- GAS -----
	// 빙의 될 시
	virtual void PossessedBy(AController* NewController) override;

	// AbilitySystemInterface로 오버라이드됨
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// ----------

	//~ Begin IInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	//~ End IInteractableInterface

	// true = 달리기, false = 걷기
	UFUNCTION(BlueprintCallable, Category = "AI")
	void SetMoveSpeed(bool bRun);

	// MaxWalkSpeed 를 현재 상태(걷기/달리기 × 이동속도 배율)로 다시 맞춘다.
	//
	// 왜 public 인가: ANS_MoveForward 가 대시 구간에 속도를 올렸다가 끝날 때 되돌린다.
	// 노티파이 객체는 몽타주 애셋에 하나뿐이라 여러 적이 공유한다. 원래 값을 멤버에
	// 저장하면 두 마리가 동시에 대시할 때 서로의 값을 덮어쓴다. 저장하는 대신
	// 여기서 다시 계산하게 하면 그 문제가 없다.
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	void RefreshMoveSpeed();

	// setter
	void SetWalkSpeed(float InWalkSpeed) { WalkSpeed = InWalkSpeed; RefreshMoveSpeed(); }
	void SetRunSpeed(float InRunSpeed) { RunSpeed = InRunSpeed; RefreshMoveSpeed(); }
	void SetAccRadius(float InAcceptanceRadius) { AcceptanceRadius = InAcceptanceRadius; }
	void SetReward(float InExpReward) { ExpReward = InExpReward; }

	// 드랍 테이블의 행을 루트 컴포넌트에 전달한다.
	// 스탯 DT 에서 주입되므로 적 BP 마다 따로 설정하지 않는다.
	void SetLootRow(const FDataTableRowHandle& InRow);

	// 이 개체를 임시 소환체로 만든다. 드랍을 만들지 않고, 시체를 열 수 없고,
	// 죽은 뒤 InDespawnDelay 초 뒤에 사라진다. 경험치는 그대로 준다.
	//
	// 왜 베이스가 갖나: 이 세 성질은 소환뿐 아니라 이벤트로 잠깐 나오는 적에게도
	// 똑같이 필요하다. 소환 어빌리티가 들고 있으면 그때 가서 또 만들게 된다.
	//
	// 쳐낼 조건: 임시 개체가 영원히 소환수뿐이면 소환 컴포넌트로 옮겨도 된다.
	void MarkAsSummoned(float InDespawnDelay);

	bool IsSummoned() const { return bSummoned; }

	// 경계 팀을 정한다. 스폰하는 쪽(PCG · 소환)이 스폰 직후에 부른다.
	void SetAlertTeam(FName InAlertTeam) { AlertTeam = InAlertTeam; }
	FName GetAlertTeam() const { return AlertTeam; }

	// 같은 경계 팀원들에게 이 플레이어를 알린다. 서버 전용.
	// 맞았을 때(ApplyMeleeHit)와 교전이 아니었다가 처음 발견했을 때(컨트롤러의 시야 감지) 부른다.
	void AlertTeamMates(AActor* Player);

	// 시체를 물리로 넘긴다. 사망 몽타주 끝의 AN_StartRagdoll 이 부른다.
	//
	// 왜 BecomeDead 가 아닌가: 거기서 켜면 사망 애니가 아예 안 나오고
	// 그 자리에서 폭삭 주저앉는다. 애니메이션이 끝나는 프레임이어야 한다.
	//
	// 쳐낼 조건: 물리 애셋이 없는 적은 몽타주에 노티파이를 안 넣으면 된다.
	UFUNCTION(BlueprintCallable, Category = "AI")
	void StartRagdoll();

	// 사망 알림. 서버에서만 브로드캐스트된다
	FOnEnemyDied OnDied;

	UFUNCTION(BlueprintCallable)
	void BecomeDead();

	// getter
	const UEnemyArchetypeData* GetArchetypeData() const { return ArchetypeData; }
	UEnemyAbilitySystemComponent* GetEnemyASC() { return AbilitySystemComp; }
	bool IsDead() const { return bDead; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// 데미지 GE
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> IncomingDamageEffect;

	// character BP에서 데이터 관리
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<UEnemyArchetypeData> ArchetypeData;

	// GAS
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UEnemyAbilitySystemComponent> AbilitySystemComp;

	// 적 인벤토리
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TObjectPtr<ULootComponent> LootComponent;

	// GAS 스텟
	UPROPERTY()
	TObjectPtr<UEnemyAttributeSetBase> AttributeSet;

	// 뼈대 별로 데미지 배율. 여기에 없다면 1배
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	TMap<FName, float> BoneDamageMultipliers;

	// 걷는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float WalkSpeed = 200.f;
	// 달리는 속도
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float RunSpeed = 600.f;
	// 목적지까지 도달 허용 범위
	UPROPERTY(EditDefaultsOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 100.f;
	// 받아온 이동속도 배율 값
	UPROPERTY(BlueprintReadOnly, Category = "AI|Movement")
	float CachedSpeedRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	float ExpReward = 10.f;

	// 사망 확인
	UPROPERTY(ReplicatedUsing = OnRep_Dead, VisibleAnywhere, BlueprintReadOnly)
	bool bDead = false;

	// 사망 후 컨트롤러를 치우기까지의 시간.
	// 죽기 직전에 돌던 몽타주가 전부 끝날 만큼은 길어야 한다.
	UPROPERTY(EditDefaultsOnly, Category = "AI", meta = (ClampMin = "0.0"))
	float ControllerCleanupDelay = 3.f;

	FTimerHandle ControllerCleanupTimer;

	// 타이머가 부른다. 컨트롤러를 치워 AI 로직을 끊는다
	void ShutdownAI();

	// ----- 임시 진단 -----
	// 시체가 스스로 움직이는 원인을 찾기 위한 것. 원인을 잡으면 지운다.
	FTimerHandle DeadDebugTimer;
	void DebugDeadTick();

	// 소환된 임시 개체인가. MarkAsSummoned 로만 켜진다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	bool bSummoned = false;

	// 죽고 나서 사라지기까지. 소환체만 쓴다
	float DespawnDelay = 3.f;

	FTimerHandle DespawnTimer;

	// 타이머가 부른다. 시체를 치운다
	void DespawnSelf();

	// 달리기 확인
	bool bRunning = false;

	// 마지막으로 자신을공격한 대상
	TWeakObjectPtr<AActor> LastDamageInstigator;

	/**
	 * 경계 팀. 한 명이 맞거나 플레이어를 발견하면 같은 팀 전원이 그 위치로 수색하러 온다. 비어 있으면 팀이 없다.
	 *
	 * 왜 Team.* 태그가 아닌가: Team.Player / Team.Enemy 는 "같은 편이면 안 맞는다" 판정이다. 무리 구분은 뜻이 달라 섞지 않는다.
	 * 왜 FName 인가: 레벨 디자이너가 태그 등록 없이 바로 적게 하려고.
	 * 작동방식: 같은 이름 + 같은 레벨(방)이어야 한 팀이다. 같은 방 모듈이 던전에 여러 번 나와도 방끼리 섞이지 않는다.
	 *
	 * 쳐낼 조건: 팀 단위 협동(포위 · 신호 공유)이 생기면 팀 관리 서브시스템으로 옮긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Team")
	FName AlertTeam;

	// 한 적이 팀에 알리는 최소 간격. 연타 때마다 적 전체를 훑지 않게.
	UPROPERTY(EditDefaultsOnly, Category = "AI|Team", meta = (ClampMin = "0.0"))
	float TeamAlertInterval = 1.f;

	bool bTeamAlertSent = false;
	double LastTeamAlertTime = 0.0;

	// 사망변수가 변경되면 해야할것들
	UFUNCTION()
	void OnRep_Dead();

	// GAS에서 이동속도 변경 시 연결된 함수
	void OnMoveSpeedRateChanged(const struct FOnAttributeChangeData& Data);

	// 죽인대상에게 보상
	void GiveExpToKiller();

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
