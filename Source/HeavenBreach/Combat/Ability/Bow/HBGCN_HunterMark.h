// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "HBGCN_HunterMark.generated.h"

class UMaterialBillboardComponent;
class UMaterialInterface;
class UNiagaraSystem;

/**
 * 사냥꾼의 표식 — 대상 머리 위 표식 (GameplayCue.Bow.HunterMark).
 * UGE_HunterMark 의 큐라서 GE 가 붙어 있는 동안 모든 화면에 보이고 GE 가 끝나면 사라진다. 적 코드는 건드리지 않는다.
 * 모양은 기문 님 테스트 표적(BP_Test_HunterMarkTarget)의 MarkGlyphVisual 과 같다(머티리얼 빌보드).
 * 엔진이 에셋으로만 큐를 찾으므로 BP 자식(BP_GCN_Bow_HunterMark)이 있어야 동작한다.
 */
UCLASS()
class HEAVENBREACH_API AHBGCN_HunterMarkGlyph : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	AHBGCN_HunterMarkGlyph();

protected:
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(VisibleAnywhere, Category = "HunterMark")
	TObjectPtr<UMaterialBillboardComponent> Glyph;

	/** 비면 테스트 머티리얼 M_Test_HunterMarkBillboard */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark")
	TSoftObjectPtr<UMaterialInterface> GlyphMaterial;

	/** 빌보드 크기(월드 cm). 테스트 표적은 24 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "1.0", Units = "cm"))
	float GlyphSize = 40.f;

	/** 캡슐 꼭대기에서 위로 이만큼 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (Units = "cm"))
	float HeightAboveHead = 40.f;
};

/**
 * 사냥꾼의 표식 — 적용 순간 연출 (GameplayCue.Bow.HunterMarkCast, 1회).
 * 시전자 오른손(hand_r)의 손 입자 + 표식 범위 중심의 바닥 펄스.
 * GA 가 서버에서 시전자 ASC 로 실행한다. 위치 = 큐 Location, 반경 = RawMagnitude.
 * BP 자식(BP_GCN_Bow_HunterMarkCast)이 있어야 동작한다.
 */
UCLASS()
class HEAVENBREACH_API UHBGCN_HunterMarkCast : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UHBGCN_HunterMarkCast();

	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** 비면 테스트 NS_Test_HunterMarkPalmBurst */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark")
	TSoftObjectPtr<UNiagaraSystem> PalmBurst;

	UPROPERTY(EditDefaultsOnly, Category = "HunterMark")
	FName PalmSocket = TEXT("hand_r");

	/** 인계 문서 값 0.3 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "0.0"))
	float PalmBurstScale = 0.3f;

	/** 바닥 범위 펄스 액터. 비면 테스트 BP_Test_HunterMarkPulse. 1m 평면이라 반경에 맞춰 키운다 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark")
	TSoftClassPtr<AActor> PulseClass;

	/** 펄스 메시의 기본 지름(cm). 엔진 Plane = 100 */
	UPROPERTY(EditDefaultsOnly, Category = "HunterMark", meta = (ClampMin = "1.0", Units = "cm"))
	float PulseMeshDiameter = 100.f;
};
