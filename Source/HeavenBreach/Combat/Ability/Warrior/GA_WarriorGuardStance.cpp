#include "Combat/Ability/Warrior/GA_WarriorGuardStance.h"

#include "Core/Tag/Warrior/HB_WarriorTag.h"

UGA_WarriorGuardStance::UGA_WarriorGuardStance()
{
	// 보유 여부가 곧 이 태그다. 다른 패시브와 같은 형태다.
	// SelfPassiveEffect 는 비워 둔다 — 붙일 수치가 없다.
	ActivationOwnedTags.AddTag(WarriorTag::TAG_Passive_GuardStance);
}
