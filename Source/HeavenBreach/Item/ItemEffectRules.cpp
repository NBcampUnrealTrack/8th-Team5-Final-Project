#include "Item/ItemEffectRules.h"

#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "EquipmentComponent.h"
#include "Item/ItemStatBinding.h"
#include "Combat/Effect/HBGameplayEffects.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Item/HB_ItemTag.h"
#include "Player/AttributeSet/HB_PlayerAttributeSet.h"
#include "Player/HBPlayerCharacter.h"

namespace
{
	// ================= 공용 =================

	// 조건을 만족하나. Param 은 효과 줄의 조건값이다. 뜻은 조건 종류가 정한다.
	bool IsConditionMet(const UAbilitySystemComponent* ASC, EItemEffectCondition Type, float Param)
	{
		switch (Type)
		{
		case EItemEffectCondition::Always:
			return true;

		case EItemEffectCondition::HealthBelow:
		{
			const float MaxHP = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute());
			if (MaxHP <= 0.f)
			{
				return false;
			}
			const float HP = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());
			return HP / MaxHP <= Param / 100.f;
		}

		case EItemEffectCondition::ManaFull:
		{
			const float MaxMana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxManaAttribute());
			if (MaxMana <= 0.f)
			{
				return false;
			}
			const float Mana = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_ManaAttribute());
			return Mana + KINDA_SMALL_NUMBER >= MaxMana;
		}
		}
		return false;
	}

	// 스탯 한 칸만 채운 스탯 GE 를 Target 에 건다. 오라와 조건 동안 이 같이 쓴다.
	// 숫자 옵션과 같은 계산(AddOptionToTotals · FillSpec)을 타서 "공격력 +10%" 의 뜻이 장비와 같다.
	FActiveGameplayEffectHandle ApplyStatEffect(UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC, const FResolvedItemEffect& Effect)
	{
		FItemOption Option;
		Option.Stat = Effect.Def.Stat;
		Option.ValueType = Effect.Def.ValueType;
		Option.Value = Effect.Amount;

		FItemStatTotals Totals;
		if (!ItemStats::AddOptionToTotals(Option, Totals))
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemEffect] %s: 스탯 %s 가 이 타입(+/%%)을 받지 않거나 표에 없다 — 무시"),
				*Effect.Def.Tag.ToString(), *Effect.Def.Stat.ToString());
			return FActiveGameplayEffectHandle();
		}

		// 누가 붙였는지 남도록 Source 의 컨텍스트로 만든다
		FGameplayEffectSpec Spec(GetDefault<UGE_ItemEffectStats>(), SourceASC->MakeEffectContext(), 1.f);
		ItemStats::FillSpec(Spec, Totals);
		if (Effect.Def.ReceiverTag.IsValid())
		{
			Spec.DynamicGrantedTags.AddTag(Effect.Def.ReceiverTag);
		}
		return TargetASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	// 자기 피해. 체력 1 은 남긴다 — 기획상 아이템 부작용으로는 죽지 않는다.
	void ApplyNonLethalSelfDamage(UAbilitySystemComponent* ASC, float Amount)
	{
		if (Amount <= 0.f)
		{
			return;
		}

		const float HP = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_HPAttribute());
		const float Allowed = FMath::Min(Amount, HP - 1.f);
		if (Allowed <= 0.f)
		{
			return;   // 이미 1 이하. 더 깎지 않는다
		}

		FGameplayEffectSpec Spec(GetDefault<UGE_ItemSelfDamage>(), ASC->MakeEffectContext(), 1.f);
		Spec.SetSetByCallerMagnitude(ItemTag::TAG_Data_ItemEffect_Magnitude, -Allowed);
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	// 체력 증감. + 는 회복 (ReceiveHeal 을 지나 회복 효과 옵션이 곱해진다), - 는 피해 (체력 1 은 남긴다).
	void ApplyHealthChange(AHBPlayerCharacter* Owner, UAbilitySystemComponent* ASC, float Amount)
	{
		if (Amount > 0.f)
		{
			Owner->ReceiveHeal(Amount);
			return;
		}
		ApplyNonLethalSelfDamage(ASC, -Amount);
	}

	// ================= 오라 =================

	// 오라를 받을 파티원. 협력 PvE 라 모든 플레이어가 파티원이다.
	// 쳐낼 조건: 팀이 다시 생기면 AHB_PlayerState::IsSameTeam 을 여기서 본다.
	void CollectAuraTargets(const AHBPlayerCharacter* Owner, const FGameplayTag& AuraTag, float Radius,
		TArray<UAbilitySystemComponent*>& OutTargets)
	{
		const UWorld* World = Owner->GetWorld();
		if (!World)
		{
			return;
		}
		AGameStateBase* GameState = World->GetGameState();
		if (!GameState)
		{
			return;
		}

		const float RadiusSq = Radius * Radius;
		const FVector SourceLocation = Owner->GetActorLocation();

		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (!PlayerState)
			{
				continue;
			}
			APawn* Pawn = PlayerState->GetPawn();
			if (!Pawn)
			{
				continue;
			}
			if (Pawn == Owner)
			{
				continue;
			}
			UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
			if (!TargetASC)
			{
				continue;
			}
			if (TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
			{
				continue;
			}
			// 같은 오라를 직접 낀 사람은 안 받는다. 자기 몫은 숫자 옵션이 준다 — 받으면 두 배가 된다.
			// 쳐낼 조건: 둘 다 끼면 두 배를 주기로 바뀌면 이 검사를 지운다.
			if (TargetASC->HasMatchingGameplayTag(AuraTag))
			{
				continue;
			}
			if (FVector::DistSquared(Pawn->GetActorLocation(), SourceLocation) > RadiusSq)
			{
				continue;
			}
			OutTargets.Add(TargetASC);
		}
	}

	bool HasAppliedTo(const TArray<FItemAuraApplied>& Applied, const UAbilitySystemComponent* TargetASC)
	{
		for (const FItemAuraApplied& Entry : Applied)
		{
			if (Entry.TargetASC.Get() == TargetASC)
			{
				return true;
			}
		}
		return false;
	}

	// 이 오라로 건 것을 전부 뗀다. 대상이 이미 사라졌으면 뗄 것도 없다.
	void RemoveAllApplied(TArray<FItemAuraApplied>& Applied)
	{
		for (const FItemAuraApplied& Entry : Applied)
		{
			UAbilitySystemComponent* TargetASC = Entry.TargetASC.Get();
			if (!TargetASC)
			{
				continue;
			}
			TargetASC->RemoveActiveGameplayEffect(Entry.Handle);
		}
		Applied.Reset();
	}

	// 대상 목록에 맞춘다. 나간 사람에게서 떼고, 새로 들어온 사람에게 건다.
	// 이미 건 사람에게는 다시 걸지 않는다 — 스탯이 틱마다 깜빡이지 않게.
	void SyncAuraTargets(UAbilitySystemComponent* SourceASC, const FResolvedItemEffect& Effect,
		const TArray<UAbilitySystemComponent*>& Wanted, TArray<FItemAuraApplied>& Applied)
	{
		// 뒤에서부터 돌아야 지우면서 돌 수 있다
		for (int32 Index = Applied.Num() - 1; Index >= 0; --Index)
		{
			UAbilitySystemComponent* TargetASC = Applied[Index].TargetASC.Get();
			if (!TargetASC)
			{
				Applied.RemoveAt(Index);   // 대상이 사라졌다
				continue;
			}
			if (Wanted.Contains(TargetASC))
			{
				continue;                  // 아직 범위 안
			}
			TargetASC->RemoveActiveGameplayEffect(Applied[Index].Handle);
			Applied.RemoveAt(Index);
		}

		for (UAbilitySystemComponent* TargetASC : Wanted)
		{
			if (HasAppliedTo(Applied, TargetASC))
			{
				continue;
			}
			FItemAuraApplied& New = Applied.AddDefaulted_GetRef();
			New.TargetASC = TargetASC;
			New.Handle = ApplyStatEffect(SourceASC, TargetASC, Effect);
		}
	}

	void TickAuras(AHBPlayerCharacter* Owner, UAbilitySystemComponent* ASC,
		const TArray<FResolvedItemEffect>& Effects, TMap<FGameplayTag, TArray<FItemAuraApplied>>& Auras)
	{
		TArray<FGameplayTag> Active;

		for (const FResolvedItemEffect& Effect : Effects)
		{
			if (Effect.Def.Archetype != EItemEffectArchetype::Aura)
			{
				continue;
			}
			Active.Add(Effect.Def.Tag);

			TArray<UAbilitySystemComponent*> Wanted;
			CollectAuraTargets(Owner, Effect.Def.Tag, Effect.Condition, Wanted);   // 조건값 = 반경
			SyncAuraTargets(ASC, Effect, Wanted, Auras.FindOrAdd(Effect.Def.Tag));
		}

		// 벗거나 부서진 오라는 대상에게서 전부 뗀다
		TArray<FGameplayTag> Keys;
		Auras.GetKeys(Keys);
		for (const FGameplayTag& Key : Keys)
		{
			if (Active.Contains(Key))
			{
				continue;
			}
			RemoveAllApplied(Auras[Key]);
			Auras.Remove(Key);
		}
	}

	// ================= 조건 동안 =================

	void TickWhileConditions(UAbilitySystemComponent* ASC, const TArray<FResolvedItemEffect>& Effects,
		TMap<FGameplayTag, FActiveGameplayEffectHandle>& Conditions)
	{
		TArray<FGameplayTag> Active;

		for (const FResolvedItemEffect& Effect : Effects)
		{
			if (Effect.Def.Archetype != EItemEffectArchetype::WhileCondition)
			{
				continue;
			}
			Active.Add(Effect.Def.Tag);

			FActiveGameplayEffectHandle& Handle = Conditions.FindOrAdd(Effect.Def.Tag);
			const bool bMet = IsConditionMet(ASC, Effect.Def.ConditionType, Effect.Condition);
			const bool bHas = Handle.IsValid();
			if (bMet == bHas)
			{
				continue;   // 그대로면 건드리지 않는다
			}

			if (!bMet)
			{
				ASC->RemoveActiveGameplayEffect(Handle);
				Handle.Invalidate();
				continue;
			}
			Handle = ApplyStatEffect(ASC, ASC, Effect);
		}

		// 벗은 효과는 뗀다
		TArray<FGameplayTag> Keys;
		Conditions.GetKeys(Keys);
		for (const FGameplayTag& Key : Keys)
		{
			if (Active.Contains(Key))
			{
				continue;
			}
			const FActiveGameplayEffectHandle Handle = Conditions[Key];
			if (Handle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(Handle);
			}
			Conditions.Remove(Key);
		}
	}

	// ================= 주기 =================

	void TickPeriodics(AHBPlayerCharacter* Owner, UAbilitySystemComponent* ASC,
		const TArray<FResolvedItemEffect>& Effects, float DeltaSeconds, TMap<FGameplayTag, float>& Elapsed)
	{
		TArray<FGameplayTag> Active;

		for (const FResolvedItemEffect& Effect : Effects)
		{
			if (Effect.Def.Archetype != EItemEffectArchetype::Periodic)
			{
				continue;
			}
			Active.Add(Effect.Def.Tag);

			// 조건이 끊기면 쌓인 시간을 버린다. "조건을 유지한 채로 한 주기" 를 셀 때만 발동한다.
			float& Time = Elapsed.FindOrAdd(Effect.Def.Tag);
			if (!IsConditionMet(ASC, Effect.Def.ConditionType, Effect.Condition))
			{
				Time = 0.f;
				continue;
			}

			// 정의의 주기가 0 이하면 매 틱 발동해 버린다. DT 설정 실수로 본다.
			if (Effect.Def.Interval <= 0.f)
			{
				continue;
			}

			Time += DeltaSeconds;
			if (Time < Effect.Def.Interval)
			{
				continue;
			}
			Time -= Effect.Def.Interval;
			ApplyHealthChange(Owner, ASC, Effect.Amount);
		}

		// 벗은 효과는 쌓인 시간을 버린다
		TArray<FGameplayTag> Keys;
		Elapsed.GetKeys(Keys);
		for (const FGameplayTag& Key : Keys)
		{
			if (Active.Contains(Key))
			{
				continue;
			}
			Elapsed.Remove(Key);
		}
	}
}

bool ItemEffectRules::Resolve(const FItemEffect& Effect, FResolvedItemEffect& OutResolved)
{
	const UDataTable* Table = Effect.Definition.DataTable;
	if (!Table)
	{
		return false;
	}

	// 다른 구조체의 표를 고르면 GetRow 가 "잘못된 타입" 에러를 찍는다. 구조체부터 본다.
	if (Table->GetRowStruct() != FItemEffectDef::StaticStruct())
	{
		return false;
	}

	const FItemEffectDef* Def = Effect.Definition.GetRow<FItemEffectDef>(TEXT("ItemEffectRules::Resolve"));
	if (!Def)
	{
		return false;
	}

	OutResolved.Def = *Def;
	OutResolved.Amount = Effect.Amount;
	OutResolved.Condition = Effect.Condition;
	return true;
}

void ItemEffectRules::Tick(AHBPlayerCharacter* Owner, float DeltaSeconds, FItemEffectRuntime& State)
{
	if (!Owner)
	{
		return;
	}
	UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}
	const UEquipmentComponent* Equipment = Owner->GetEquipment();
	if (!Equipment)
	{
		return;
	}

	TArray<FResolvedItemEffect> Effects;
	Equipment->CollectEquippedEffects(Effects);

	TickAuras(Owner, ASC, Effects, State.Auras);
	TickWhileConditions(ASC, Effects, State.Conditions);
	TickPeriodics(Owner, ASC, Effects, DeltaSeconds, State.PeriodicElapsed);
}

void ItemEffectRules::Clear(AHBPlayerCharacter* Owner, FItemEffectRuntime& State)
{
	// 파티원에게 건 오라. 대상의 ASC 로 뗀다
	for (TPair<FGameplayTag, TArray<FItemAuraApplied>>& Pair : State.Auras)
	{
		RemoveAllApplied(Pair.Value);
	}
	State.Auras.Reset();

	// 나에게 건 조건 동안 효과. 내 ASC 로 뗀다
	UAbilitySystemComponent* ASC = nullptr;
	if (Owner)
	{
		ASC = Owner->GetAbilitySystemComponent();
	}
	if (ASC)
	{
		for (const TPair<FGameplayTag, FActiveGameplayEffectHandle>& Pair : State.Conditions)
		{
			if (Pair.Value.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}
	State.Conditions.Reset();

	State.PeriodicElapsed.Reset();
}

void ItemEffectRules::OnSkillUsed(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo)
	{
		return;
	}

	// 서버에서만. "체력 1 은 남긴다" 를 서버 체력으로 잘라야 한다. 클라 체력 바는 복제로 따라온다.
	if (!ActorInfo->IsNetAuthority())
	{
		return;
	}

	AHBPlayerCharacter* Owner = Cast<AHBPlayerCharacter>(ActorInfo->AvatarActor.Get());
	if (!Owner)
	{
		return;
	}
	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC)
	{
		return;
	}
	const UEquipmentComponent* Equipment = Owner->GetEquipment();
	if (!Equipment)
	{
		return;
	}

	TArray<FResolvedItemEffect> Effects;
	Equipment->CollectEquippedEffects(Effects);

	const float MaxHP = ASC->GetNumericAttribute(UHB_PlayerAttributeSet::GetPlayer_MaxHPAttribute());
	for (const FResolvedItemEffect& Effect : Effects)
	{
		if (Effect.Def.Archetype != EItemEffectArchetype::OnSkillUse)
		{
			continue;
		}
		if (!IsConditionMet(ASC, Effect.Def.ConditionType, Effect.Condition))
		{
			continue;
		}

		// 효과량은 최대 체력의 % 다. -3 이면 3% 를 낸다
		ApplyHealthChange(Owner, ASC, MaxHP * Effect.Amount / 100.f);
	}
}
