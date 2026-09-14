#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class UGameplayAbility;
struct FHBSkillAugment;

/**
 * 스킬 설명의 {치환자} 를 애셋의 실제 수치로 바꾼다.
 *
 * 왜 넣었나: 설명에 숫자를 손으로 적으면 GE 와 DA 두 곳을 고치게 되고,
 * 한쪽을 빠뜨리면 UI 가 거짓말을 한다. 값의 주인을 애셋 하나로 두고
 * 설명은 그것을 비추기만 한다.
 *
 * 왜 FText::Format 을 안 쓰나: {+Attack%} 처럼 이름에 기호가 섞인 것을
 * 엔진 포매터가 받아 주는지 확인할 수 없고, 색상 태그를 값에만 두르려면
 * 어차피 직접 훑어야 한다.
 *
 * 문법
 *   {Name}     값 그대로
 *   {Name%}    백분율로 표시
 *   {+Name}    양수면 부호를 붙인다
 *   {-Name}    감소량으로 표시한다. 값이 감소가 아니면 치환을 거절한다
 *   {+Name%} {-Name%} 조합 가능. 대소문자는 구분하지 않는다
 *
 * 표기는 저자가 기대하는 형태를 선언하는 것이고, 숫자를 정하는 것은 목록이다.
 * 둘이 어긋나면 치환하지 않고 로그를 남긴다. 표기가 이기는 일은 없다.
 *
 * 쓸 수 있는 이름은 HBSkillDescription.cpp 의 BuildRegistry 에 있다.
 * 이름을 늘리려면 거기 한 줄만 추가한다.
 */

/**
 * 치환 결과.
 *
 * 여기 담기는 숫자가 무엇인가. 출처마다 다르므로 섞어서 읽지 말 것.
 *
 *   곱셈 모디파이어   기존 값 대비 상대 변화율.
 *                     흡혈률 0.1 에 x1.2 면 20% 다.
 *                     최종 흡혈률 12% 도 아니고 2%p 도 아니다.
 *
 *   덧셈 모디파이어   그 단위의 증감량.
 *                     비율 속성에 +0.05 면 5%p 증가다.
 *
 *   덮어쓰기 모디파이어  그 값 자체. 변화량이 아니다.
 *                     저체력 임계값처럼 "얼마가 된다" 를 말하는 수치가 이 방식이다.
 *
 *   GA 자체 값        원래 정의된 값 그 자체. 변화량이 아니다.
 *                     지속시간, 마나, 패링 감소율이 여기 속한다.
 */
struct HEAVENBREACH_API FHBFormattedText
{
	// 태그가 없는 결과. 평범한 UTextBlock 이 쓴다.
	FText Plain;

	// 값에 <Value> 가 둘러진 결과. URichTextBlock 이 쓴다.
	// 원문 구간의 < > & 는 이스케이프되어 있다.
	FText Rich;
};

/** 수치를 어디서 찾을지. */
struct HEAVENBREACH_API FHBSkillValueContext
{
	// 이 문장이 붙은 스킬의 GA. 없으면 아무것도 못 읽는다.
	TSubclassOf<UGameplayAbility> AbilityClass = nullptr;

	// 증강 문장이면 지정한다.
	//
	// 이번에는 쓰지 않는다. 증강 화면이 아직 UpgradeCandidates 를 읽지 않아
	// 넣어도 눈으로 확인할 방법이 없다.
	const FHBSkillAugment* Augment = nullptr;

	// 로그에 찍을 스킬 이름. 못 읽은 치환자가 어느 스킬 것인지 알아야 한다.
	FName DebugSkillId = NAME_None;
};

/** 평문과 리치를 함께 만든다. 치환자가 없어도 리치는 이스케이프를 거친다. */
HEAVENBREACH_API FHBFormattedText FormatSkillText(const FText& InText, const FHBSkillValueContext& InContext);
