// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HBStartingLoadout.generated.h"

class UHBJobData;

/**
 * 로비에서 확정한 시작 구성. 직업 하나와 스킬 슬롯 셋이 전부다.
 *
 * 왜 넣었나: 로비와 매치가 서로 다른 PlayerState 를 쓰는데 선택을 담을 칸이
 * 어디에도 없다. 이 구조체가 AHB_PlayerState 에 얹혀 복제되고, CopyProperties
 * 로 매치까지 건너간다.
 *
 * 왜 스킬은 ID 로 담나: 실제 GA 클래스와 수치는 직업 DA 가 갖는다. 클라이언트가
 * 보낸 클래스 포인터를 그대로 부여하면 조작할 수 있으므로, 서버는 ID 만 받아
 * 직업 DA 의 목록에서 찾아 검증한다.
 *
 * 왜 필드 네 개를 따로 두지 않고 묶었나: 로비 컨트롤러가 편집값 / 서버 확정값 /
 * 응답 대기값 세 벌을 동시에 들고 비교한다. 낱개로 두면 멤버가 열두 개가 되고
 * 비교도 손으로 해야 한다.
 *
 * 빈 슬롯은 정상이다. 스킬을 하나도 고르지 않고 매치에 들어갈 수 있다.
 *
 * 쳐낼 조건: 슬롯이 넷 이상이 되거나 매치 중 획득분까지 담아야 하면
 * TArray<FName> 로 바꾼다. 지금은 칸이 고정이라 이름 있는 필드가 읽기 쉽다.
 * 레벨·증강·추가 패시브는 여기 넣지 않는다 — 그쪽은 AHB_PlayerState 의
 * TakenAugments 와 ResetMatchProgression 이 따로 관리한다.
 */
USTRUCT(BlueprintType)
struct HEAVENBREACH_API FHBStartingLoadout
{
	GENERATED_BODY()

	/**
	 * 고른 직업의 데이터 애셋.
	 *
	 * 왜 FName 이 아니라 포인터인가: 이름으로 담으면 그 이름을 다시 DA 로 되돌릴
	 * 표(레지스트리)가 어딘가 있어야 한다. 직업 DA 는 애셋이라 서버와 클라이언트
	 * 양쪽에 똑같이 존재하므로, 처음부터 애셋을 가리키면 그 표가 필요 없다.
	 *
	 * 쳐낼 조건: 복제된 애셋 포인터가 다른 플레이어의 클라이언트에서 null 로
	 * 풀리면(= 그 DA 가 그 머신에 로드돼 있지 않으면) FName + 조회로 되돌려야
	 * 한다. 그때 조회표는 매치 캐릭터 BP 의 배열 하나로 충분하다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TObjectPtr<UHBJobData> Job = nullptr;

	/** Q 슬롯. 비어 있으면 Q 스킬 없이 시작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName SkillQId = NAME_None;

	/** E 슬롯. 비어 있으면 E 스킬 없이 시작한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName SkillEId = NAME_None;

	/** 시작 패시브 하나. 레벨업으로 얻는 추가 패시브는 여기 담지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName PassiveSkillId = NAME_None;

	/**
	 * 편집값이 서버 확정값과 같은지 보는 용도다.
	 * 로비 컨트롤러가 이 비교로 "저장할 게 남았나" 를 판단한다.
	 *
	 * 작동방식: 네 필드를 전부 비교한다. 한 줄로 && 를 늘어놓지 않고 조기
	 * 리턴으로 쪼갠 것은 이 프로젝트 규약이며 다른 뜻은 없다.
	 *
	 * TStructOpsTypeTraits 를 선언하지 않은 이유: 복제 시 엔진이 프로퍼티를
	 * 개별 비교한다. 필드 넷이라 그걸로 충분하고, 이 연산자는 순수하게
	 * 우리 코드에서만 쓴다.
	 */
	bool operator==(const FHBStartingLoadout& Other) const
	{
		if (Job != Other.Job)
		{
			return false;
		}
		if (SkillQId != Other.SkillQId)
		{
			return false;
		}
		if (SkillEId != Other.SkillEId)
		{
			return false;
		}
		return PassiveSkillId == Other.PassiveSkillId;
	}

	bool operator!=(const FHBStartingLoadout& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * 시작 구성이 쓸 수 있는 것인지 본다. 서버가 확정하기 전에 반드시 통과시킨다.
 *
 * 왜 필요한가: AHBPlayerCharacter::ApplyLoadout 은 검증 함수가 아니라 파괴적
 * 세터다. 기존 어빌리티를 먼저 전부 회수한 뒤 잘못된 ID 를 조용히 빈 슬롯으로
 * 만든다. 쓰레기를 넘기면 스킬이 사라지고 아무도 모른다.
 *
 * 작동방식: 직업만 필수고 스킬 슬롯은 "채워져 있으면" 검사한다.
 * 빈 슬롯은 정상이다 — 스킬을 하나도 고르지 않고 매치에 들어갈 수 있다.
 *
 * 쳐낼 조건: 같은 GA 를 두 슬롯에 꽂는 기획이 생기면 AbilityClass 중복 검사를
 * 뺀다. 지금은 ID 가 다른데 같은 GA 를 가리키는 것은 DA 설정 실수다.
 */
HEAVENBREACH_API bool ValidateStartingLoadout(
	const FHBStartingLoadout& InLoadout,
	FText& OutError);

/**
 * 직업 DA 의 InitialSkillIds 로 기본 구성을 만든다.
 *
 * 왜 필요한가: 로비를 거치지 않은 진입(PIE 테스트 등)과, 전달된 구성이 잘못됐을
 * 때의 폴백이다. 이게 없으면 그런 경우 스킬이 하나도 안 붙는다.
 *
 * 작동방식: 앞에서부터 첫 액티브를 Q, 둘째를 E, 첫 패시브를 담는다. 목록에 없는
 * ID 는 오류 로그를 남기고 건너뛴다 — 오타 하나가 나머지 스킬까지 죽이지 않게.
 * 모은 결과는 위 검증을 통과해야만 반환한다.
 *
 * 쳐낼 조건: DA 가 슬롯을 명시하는 구조로 바뀌면 이 배열 해석은 사라진다.
 */
HEAVENBREACH_API bool MakeDefaultStartingLoadout(
	UHBJobData* InJob,
	FHBStartingLoadout& OutLoadout,
	FText& OutError);
