// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "HBGCN_Magic.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * 마법 시전 손 이펙트 (GameplayCue.Magic.CastHand). 시전 중 추가되고 끝나면 제거된다.
 * UHBPlayerCastGABase::AddHandCue 가 보내는 파라미터 규약:
 *   AbilityLevel = 이펙트 종류(1 구체, 2 순간이동, 3 서리, 4 폭발), RawMagnitude = 나이아가라 크기,
 *   Location = hand_l 기준 상대 오프셋(월드 좌표가 아님).
 * 엔진이 에셋으로만 큐를 찾으므로 BP 자식이 태그를 지정해야 동작한다(C++ 에서 GameplayCueTag 를 두지 않는다).
 */
UCLASS()
class HEAVENBREACH_API AHBGCN_MagicCastHand : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AHBGCN_MagicCastHand();

protected:
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	/** 이펙트 종류(AbilityLevel) -> 시전 나이아가라. 생성자에서 기본 경로를 채운다 */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue")
	TMap<int32, TSoftObjectPtr<UNiagaraSystem>> CastSystems;

	/** 붙일 소켓 */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue")
	FName HandSocket = TEXT("hand_l");

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveFx;
};

/**
 * 마법 일회성 연출 (구체 적중 / 순간 이동 / 마력 폭발이 BP 자식으로 재사용).
 * ActorClass(디자이너의 로컬 FX BP)를 Location 에 로컬 스폰한다. 서버는 건너뛴다.
 * 순간이동은 AbilityLevel 0 = 출발(ActorClass), 1 = 도착(ActorClassAlt, 없으면 ActorClass).
 * 크기: ScaleDivisor > 0 이고 RawMagnitude > 0 이면 XY = RawMagnitude / ScaleDivisor, 아니면 DefaultScale.
 * 소리(Sound / SoundAlt)는 이펙트와 따로 Location 에서 낸다. 규칙은 ActorClass / ActorClassAlt 와 같다.
 * ActorClass 를 비우고 소리만 써도 된다(서리 지대 — 지대 이펙트는 AHBFrostField 가 직접 띄운다).
 */
UCLASS()
class HEAVENBREACH_API UHBGCN_MagicBurst : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** 이번 실행에 낼 소리. 순간이동 도착이면 SoundAlt, 비어 있으면 Sound */
	USoundBase* PickSound(const FGameplayCueParameters& Parameters) const;

	/**
	 * 터질 때 낼 소리. 이펙트 BP 와 따로 둔 이유: 지금 이펙트는 test_gm 의 임시 BP 라
	 * 거기에 넣으면 정식 이펙트로 바꿀 때 소리도 같이 사라진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue")
	TObjectPtr<USoundBase> Sound;

	/** AbilityLevel == 1 일 때 쓰는 소리(순간이동 도착). 비면 Sound */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue")
	TObjectPtr<USoundBase> SoundAlt;

	UPROPERTY(EditDefaultsOnly, Category = "MagicCue")
	TSoftClassPtr<AActor> ActorClass;

	/** AbilityLevel == 1 일 때 쓰는 클래스(순간이동 도착). 비면 ActorClass */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue")
	TSoftClassPtr<AActor> ActorClassAlt;

	/** 0 이하면 RawMagnitude 로 크기를 정하지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue", meta = (ClampMin = "0.0"))
	float ScaleDivisor = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "MagicCue", meta = (ClampMin = "0.0"))
	float DefaultScale = 1.f;

	/** 0 이면 수명을 건드리지 않는다(FX BP 가 스스로 관리) */
	UPROPERTY(EditDefaultsOnly, Category = "MagicCue", meta = (ClampMin = "0.0", Units = "s"))
	float LifeSpan = 0.f;
};
