#include "Combat/Ability/Warrior/GA_WarriorParry.h"

// 활성 유지와 태그 부여는 부모(UHBPlayerPassiveGABase)가 한다.
// 여기 있는 것은 설명 치환이 물어보는 수치를 답하는 것 하나뿐이다.
//
// 이 파일을 지우지 말 것. UHT 가 생성한 코드가 여기 컴파일 단위에 들어간다.

bool UGA_WarriorParry::GetNamedSkillValue(const FName InName, float& OutValue) const
{
	// 패링 수치는 어트리뷰트도 GE 모디파이어도 아니라 목록이 찾아갈 곳이 없다.
	// 값의 주인인 이 클래스가 직접 답한다.
	if (InName == TEXT("ParryWindow"))
	{
		OutValue = WindowDuration;
		return true;
	}
	if (InName == TEXT("ParryReduction"))
	{
		OutValue = DamageReduction;
		return true;
	}

	return Super::GetNamedSkillValue(InName, OutValue);
}
