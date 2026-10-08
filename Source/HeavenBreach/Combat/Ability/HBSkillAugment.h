#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HBSkillAugment.generated.h"

class UGameplayEffect;
class UTexture2D;

/**
 * 이번 판에서 어떤 증강을 몇 단계까지 먹었는지.
 *
 * 왜 TMap 이 아닌가: UPROPERTY 복제가 TMap 을 지원하지 않는다.
 * PlayerState 가 이 배열을 복제하고 UI 가 그대로 읽는다.
 */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBAugmentStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AugmentTag;

	// 1 부터 시작한다. 0 은 목록에 없는 것과 같으므로 저장하지 않는다.
	UPROPERTY(BlueprintReadOnly)
	int32 Tier = 0;
};

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

	/**
	 * 단계별 수치. 그 단계에서의 최종값이며 누적값이 아니다.
	 * 1단계 10, 2단계 15 면 2단계는 15 다. 25 가 아니다.
	 *
	 * 비우면 단계 없는 1회성 증강으로 동작한다. 최대 단계는 이 배열의 길이다.
	 * 기획서의 "최대 3단계" 는 여기 3개를 넘겨 넣지 않는 것으로 지킨다.
	 *
	 * 모디파이어가 Multiply 인 증강(이동속도)은 배율 자체를 적는다. 5% 증가면
	 * 0.05 가 아니라 1.05 다. Add 인 증강은 더할 값을 적는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Augment")
	TArray<float> TierMagnitudes;

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

UENUM(BlueprintType)
enum class EHBUpgradeCandidateType : uint8
{
	// 이미 가진 스킬에 증강을 하나 붙인다. 액티브든 패시브든 상관없다.
	Augment,
	// 아직 없는 패시브를 통째로 얻는다.
	NewPassive
};

/**
 * 레벨업 때 뜨는 카드 한 장.
 *
 * 작동방식: 서버가 GenerateUpgradeCandidates 로 최대 3장을 만들어 복제하고,
 * 플레이어가 CandidateId 를 골라 보내면 서버가 그 장을 찾아 적용한다.
 *
 * 쳐낼 조건: 카드 종류가 셋 이상 되면 Type 대신 각자의 구조체로 나눈다.
 * 지금은 "증강 붙이기" 와 "패시브 얻기" 둘뿐이라 한 구조체로 충분하다.
 *
 * 왜 여기 있나: 원래 HB_PlayerState.h 에 있었다. 증강 후보는 증강 데이터에
 * 속하는 개념이고 PlayerState 는 그것을 담는 그릇일 뿐이라 이쪽으로 옮겼다.
 * DrawCandidates 처럼 액터에 얽매이지 않는 순수 함수가 이 타입을 값으로
 * 다루려면, PlayerState 헤더를 끌어오지 않고도 정의를 볼 수 있어야 한다.
 */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBUpgradeCandidate
{
	GENERATED_BODY()

	// 선택을 서버로 보낼 때 쓰는 식별자. 종류마다 다른 규칙으로 만든다.
	UPROPERTY(BlueprintReadOnly)
	FName CandidateId;

	UPROPERTY(BlueprintReadOnly)
	EHBUpgradeCandidateType Type = EHBUpgradeCandidateType::Augment;

	// 이 카드를 고르면 올라갈 단계. 단계 없는 증강은 1.
	UPROPERTY(BlueprintReadOnly)
	int32 Tier = 1;

	// 어느 스킬의 카드인가. NewPassive 면 얻게 될 패시브의 SkillId 다.
	UPROPERTY(BlueprintReadOnly)
	FName TargetSkillId;

	// Type == Augment 일 때만 유효하다. 고르면 이 태그가 TakenAugments 에 들어간다.
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AugmentTag;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FText Description;

	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * 레벨업 때 고른 카드 한 번의 기록.
 *
 * 왜 필요한가: TakenAugments 는 "무엇을 몇 단계까지" 만 담아서 고른 순서와
 * 같은 증강을 두 번 고른 사실이 남지 않는다. 스탯창이 선택마다 노드 하나를
 * 시간 순서대로 그리려면 선택 한 번당 한 줄이 필요하다.
 *
 * 문구와 아이콘은 담지 않는다. 복제 비용 때문이고, UI 가 AugmentTag /
 * TargetSkillId 로 데이터를 다시 찾는다. 이 값들은 후보 카드의 값을 그대로 옮긴다.
 */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBUpgradePick
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EHBUpgradeCandidateType Type = EHBUpgradeCandidateType::Augment;

	// 어느 스킬의 증강인가. 공통 강화면 비어 있다. NewPassive 면 얻은 패시브의 SkillId.
	UPROPERTY(BlueprintReadOnly)
	FName TargetSkillId;

	// Type == Augment 일 때만 유효하다.
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AugmentTag;

	// 그 선택으로 올라간 단계. 단계 없는 증강과 패시브는 1.
	UPROPERTY(BlueprintReadOnly)
	int32 Tier = 1;
};
