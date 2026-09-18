#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HBSkillAugment.generated.h"

class UGameplayEffect;
class UTexture2D;

/**
 * 스킬 하나가 가질 수 있는 증강 하나.
 *
 * 왜 넣었나: 증강이 지금까지 태그로만 존재해서, UI 가 "이 스킬에 어떤 증강이 있고
 * 그게 무슨 효과인지" 를 알 방법이 없었다. 후보 카드의 설명도 PlayerState 안에
 * 44줄짜리 하드코딩 함수로 만들고 있었다.
 *
 * 작동방식: 스킬 GA 애셋이 이 구조체의 배열을 들고 있다. GA 는 클래스 기본값이라
 * 인스턴스 없이 CDO 에서 읽히므로, 서버든 클라든 UI 가 그냥 꺼내 쓴다.
 * 실제 보유 여부는 여기 없다. PlayerState 의 TakenAugments 가 답한다.
 *
 * 증강이 바꿀 수 있는 것은 네 갈래다.
 *   전역 능력치   GrantEffect          어트리뷰트를 바꾸는 상시 GE
 *   이 스킬 수치  Params               스킬 코드가 꺼내 쓰는 숫자들
 *   코스트        AdditionalCostEffect 발동 시 추가로 낼 GE
 *   행동          (칸 없음)            AugmentTag 만 보고 코드가 분기한다
 *
 * 쳐낼 조건: 행동 증강이 다섯 개를 넘고 필요한 데이터가 서로 갈리기 시작하면
 * Instanced UObject 배열로 옮긴다. 보스 패턴 정의와 같은 판단 기준이다.
 */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBSkillAugment
{
	GENERATED_BODY()

	// 보유 판정 키이자 조회 키. 스킬 코드가 HasAugment 로 이걸 묻는다.
	// 이름 규칙: Augment.<직업>.<스킬>.<이름>  예) Augment.Warrior.Parry.Mitigate
	// 규칙을 지켜야 두 스킬이 같은 태그를 선언하는 사고가 안 난다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	FGameplayTag AugmentTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	FText DisplayName;

	// 항목 이름을 중괄호로 감싸면 Params 의 값으로 치환된다.
	// 예) "쿨타임이 {Cooldown} 배가 됩니다" + Params[SkillParam.Cooldown]=0.5
	// 숫자를 글에 직접 적지 말 것. 밸런스를 고칠 때 두 곳을 고치게 되고,
	// 한쪽을 빠뜨리면 UI 가 거짓말을 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	TSoftObjectPtr<UTexture2D> Icon;

	// 이 스킬에만 걸리는 수치들. 키는 무엇을, 값은 얼마나.
	// 예) SkillParam.Cooldown → 0.5,  SkillParam.ManaCost → 0.7
	//
	// 왜 맵인가: 증강 하나가 여러 수치를 동시에 바꿀 수 있어야 한다.
	// 값 하나짜리 칸으로는 "쿨 50% + 마나 30%" 같은 증강을 못 담는다.
	//
	// 숫자의 뜻은 여기서 정하지 않는다. 꺼내 쓰는 스킬이 배율로 읽을지
	// 절대값으로 읽을지 정한다. 항목 어휘는 HB_SkillParamTag.h 에 있다.
	//
	// 한 항목은 한 증강만 정한다. 두 증강이 같은 항목을 정하는 것은 설계 실수라,
	// 합산 규칙을 만들지 않고 먼저 찾은 것을 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	TMap<FGameplayTag, float> Params;

	// 증강을 고른 순간 영구로 붙일 GE. 전역 효과가 없으면 비워 둔다.
	// 이동속도, 공격력처럼 어트리뷰트가 있는 것은 전부 이쪽이다.
	//
	// 반드시 Infinite 여야 하고 Period 는 0 이어야 한다.
	// Duration 이면 증강이 시간이 지나 저절로 풀리고,
	// Period 가 0 이 아니면 대상 어트리뷰트의 BaseValue 를 영구히 갈아엎는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	TSubclassOf<UGameplayEffect> GrantEffect;

	// 이 증강을 가지면 발동할 때 이 코스트를 추가로 낸다. 없으면 비워 둔다.
	//
	// "마나 대신 체력" 같은 교체는 이걸로 표현한다.
	// 체력 GE 를 여기 넣고 Params 에 ManaCost → 0 을 적으면 된다.
	// 교체 칸을 따로 두지 않는 이유는, 증강 두 개가 코스트를 건드릴 때
	// 어느 쪽이 이기는지 정해야 하고 그 규칙이 반드시 버그로 돌아오기 때문이다.
	// 추가로만 쌓으면 순서를 따질 일이 없다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	TSubclassOf<UGameplayEffect> AdditionalCostEffect;

	// Params 에서 항목 하나를 찾는다. 없으면 false 이고 Out 은 손대지 않는다.
	bool FindParam(FGameplayTag ParamTag, float& Out) const;

	// 설명의 치환자를 Params 의 실제 수치로 바꾼 것.
	// 치환자 이름은 항목 태그의 마지막 조각이다.
	// SkillParam.Cooldown → {Cooldown}
	FText GetFormattedDescription() const;
};
