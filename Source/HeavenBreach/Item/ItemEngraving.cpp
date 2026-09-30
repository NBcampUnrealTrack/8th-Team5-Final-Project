#include "Item/ItemEngraving.h"

#include "Engine/DataTable.h"
#include "Inventory/InventoryTypes.h"
#include "Item/Item.h"
#include "Item/ItemEngravingTypes.h"
#include "Item/ItemStatBinding.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	// 각인을 받는 부위. 옵션 줄의 bWeapon / bShield / bArmor 와 짝이다.
	enum class EEngraveTarget : uint8
	{
		None,
		Weapon,
		Shield,
		Armor,
	};

	// 무기 · 방패는 표가 따로라 표 종류로 가른다.
	// 방어구는 악세서리 · 소모품과 같은 표(DT_Item)에 있어서 슬롯으로 고른다.
	EEngraveTarget GetTarget(const FItemInstance& Item, const FItemViewData& View)
	{
		const UScriptStruct* RowStruct = Item.ItemRow.DataTable->GetRowStruct();
		if (RowStruct == FWeaponData::StaticStruct())
		{
			return EEngraveTarget::Weapon;
		}
		if (RowStruct == FShieldData::StaticStruct())
		{
			return EEngraveTarget::Shield;
		}

		switch (View.EquipSlotType)
		{
		case EEquipSlotType::Helmet:
		case EEquipSlotType::Chest:
		case EEquipSlotType::Legs:
			return EEngraveTarget::Armor;
		default:
			return EEngraveTarget::None;
		}
	}

	bool IsAllowedOn(const FEngraveOptionEntry& Entry, EEngraveTarget Target)
	{
		switch (Target)
		{
		case EEngraveTarget::Weapon: return Entry.bWeapon;
		case EEngraveTarget::Shield: return Entry.bShield;
		case EEngraveTarget::Armor:  return Entry.bArmor;
		default:                     return false;
		}
	}

	// 이 스탯이 이 타입(+/%)을 받는가. 장비 합산과 같은 함수로 확인한다 — 규칙이 두 곳에 생기지 않게.
	// 왜 굴릴 때 확인하나: 받지 않는 옵션이 한 번 발리면 저장에 영원히 남는다. 장착해도 경고만 찍히고 0 이다.
	bool CanTake(const FEngraveOptionEntry& Entry)
	{
		FItemOption Option;
		Option.Stat = Entry.Stat;
		Option.ValueType = Entry.ValueType;

		FItemStatTotals Scratch;   // 확인만 하고 버린다
		return ItemStats::AddOptionToTotals(Option, Scratch);
	}

	// 풀에서 이 부위에 붙을 수 있고 스탯 표가 받는 줄만 남긴다. 잘못 적힌 줄은 여기서 경고한다.
	TArray<const FEngraveOptionEntry*> FilterPool(const TArray<FEngraveOptionEntry>& Pool,
		EEngraveTarget Target, const FName& RowName)
	{
		TArray<const FEngraveOptionEntry*> Result;
		for (const FEngraveOptionEntry& Entry : Pool)
		{
			if (!IsAllowedOn(Entry, Target))
			{
				continue;
			}
			if (!CanTake(Entry))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Engrave] %s 행: 스탯 %s 가 이 타입(+/%%)을 받지 않거나 표에 없다 — 뺀다"),
					*RowName.ToString(), *Entry.Stat.ToString());
				continue;
			}
			Result.Add(&Entry);
		}
		return Result;
	}

	// 이미 붙은 (스탯, 타입) 인가. 같은 스탯이라도 + 와 % 는 다른 것으로 친다.
	bool IsUsed(const TArray<FItemOption>& Used, const FEngraveOptionEntry& Entry)
	{
		for (const FItemOption& Option : Used)
		{
			if (Option.Stat != Entry.Stat)
			{
				continue;
			}
			if (Option.ValueType == Entry.ValueType)
			{
				return true;
			}
		}
		return false;
	}

	// 가중치 목록에서 칸 하나를 뽑는다. 0 이하인 칸은 안 뽑힌다. 뽑을 칸이 없으면 INDEX_NONE.
	int32 PickWeighted(const TArray<float>& Weights)
	{
		float Total = 0.f;
		for (const float Weight : Weights)
		{
			if (Weight > 0.f)
			{
				Total += Weight;
			}
		}
		if (Total <= 0.f)
		{
			return INDEX_NONE;
		}

		float Roll = FMath::FRandRange(0.f, Total);
		int32 Last = INDEX_NONE;
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			const float Weight = Weights[Index];
			if (Weight <= 0.f)
			{
				continue;
			}
			Last = Index;
			if (Roll < Weight)
			{
				return Index;
			}
			Roll -= Weight;
		}
		return Last;   // 부동소수 오차로 끝까지 온 경우. 마지막으로 뽑을 수 있던 칸
	}

	// 개수를 뽑는다. Weights 는 [0] 이 가장 적은 개수, [1] 이 그 다음 …
	// 비어 있거나 칸 수가 안 맞으면 균등. 가중치가 전부 0 이면 가장 적은 개수.
	int32 RollCount(int32 Min, int32 Max, const TArray<float>& Weights, const FName& RowName, const TCHAR* What)
	{
		// CSV 로 넣으면 에디터의 ClampMin 을 안 거친다. 음수와 뒤집힌 범위를 여기서 막는다.
		const int32 Low = FMath::Max(0, FMath::Min(Min, Max));
		const int32 High = FMath::Max(Low, FMath::Max(Min, Max));
		const int32 Choices = High - Low + 1;

		if (Weights.Num() == 0)
		{
			return FMath::RandRange(Low, High);
		}
		if (Weights.Num() != Choices)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Engrave] %s 행: %s 개수 가중치가 %d 칸인데 개수는 %d 가지다 — 균등으로 뽑는다"),
				*RowName.ToString(), What, Weights.Num(), Choices);
			return FMath::RandRange(Low, High);
		}

		const int32 Picked = PickWeighted(Weights);
		if (Picked == INDEX_NONE)
		{
			return Low;
		}
		return Low + Picked;
	}

	// 범위 안에서 Step 단위 값 하나. 칸마다 같은 확률이다.
	//   2 ~ 4, Step 1  →  2 · 3 · 4 가 1/3 씩
	// 반올림으로 만들지 않는 이유: 2~4 를 뽑아 반올림하면 끝값(2, 4)이 가운데(3)의 절반만 나온다.
	float RollValue(const FEngraveOptionEntry& Entry)
	{
		const float Low = FMath::Min(Entry.Min, Entry.Max);
		const float High = FMath::Max(Entry.Min, Entry.Max);

		// CSV 로 넣은 Step 0 은 ClampMin 을 안 거친다. 0 으로 나누지 않게 막는다.
		if (Entry.Step <= 0.f)
		{
			return FMath::FRandRange(Low, High);
		}

		const int32 Steps = FMath::RoundToInt((High - Low) / Entry.Step);
		const int32 Picked = FMath::RandRange(0, Steps);
		return Low + Picked * Entry.Step;
	}

	// 풀에서 Count 개를 뽑아 Out 에 담는다.
	// Used 에 있는 (스탯, 타입) 은 건너뛰고, 뽑을 때마다 Used 에 더한다.
	// 좋은 쪽과 나쁜 쪽이 같은 Used 를 써서 둘 사이에서도 겹치지 않는다.
	void PickOptions(const TArray<const FEngraveOptionEntry*>& Pool, int32 Count,
		TArray<FItemOption>& Used, TArray<FItemOption>& Out, const FName& RowName)
	{
		for (int32 Round = 0; Round < Count; ++Round)
		{
			TArray<const FEngraveOptionEntry*> Candidates;
			TArray<float> Weights;
			for (const FEngraveOptionEntry* Entry : Pool)
			{
				if (IsUsed(Used, *Entry))
				{
					continue;
				}
				Candidates.Add(Entry);
				Weights.Add(Entry->Weight);
			}

			const int32 Picked = PickWeighted(Weights);
			if (Picked == INDEX_NONE)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Engrave] %s 행: 뽑을 옵션이 모자란다 — %d 개 중 %d 개만 발랐다"),
					*RowName.ToString(), Count, Round);
				return;
			}

			const FEngraveOptionEntry* Entry = Candidates[Picked];

			FItemOption Option;
			Option.Stat = Entry->Stat;
			Option.ValueType = Entry->ValueType;
			Option.Value = RollValue(*Entry);

			Out.Add(Option);
			Used.Add(Option);
		}
	}

	// ================= 저장 모양 =================

	// 타입은 숫자 대신 글자로 쓴다 — enum 순서가 바뀌어도 저장이 안 깨지게.
	// 쳐낼 조건: EItemOptionValueType 에 새 타입이 생기면 아래 두 함수에 한 줄씩 더한다.
	FString ValueTypeToJson(EItemOptionValueType Type)
	{
		if (Type == EItemOptionValueType::Percent)
		{
			return TEXT("percent");
		}
		return TEXT("flat");
	}

	bool ValueTypeFromJson(const FString& Name, EItemOptionValueType& OutType)
	{
		if (Name == TEXT("flat"))
		{
			OutType = EItemOptionValueType::Flat;
			return true;
		}
		if (Name == TEXT("percent"))
		{
			OutType = EItemOptionValueType::Percent;
			return true;
		}
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> OptionsToJson(const TArray<FItemOption>& Options)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FItemOption& Option : Options)
		{
			const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("stat"), Option.Stat.ToString());
			Object->SetStringField(TEXT("type"), ValueTypeToJson(Option.ValueType));
			Object->SetNumberField(TEXT("value"), Option.Value);
			Array.Add(MakeShared<FJsonValueObject>(Object));
		}
		return Array;
	}

	// 못 읽는 줄은 빼고 나머지는 살린다. 아이템 전체를 버리지 않는다.
	// 칸 이름을 TryGet 으로 읽는 이유: Get 은 칸이 없으면 에러 로그를 찍는다.
	void OptionsFromJson(const TSharedPtr<FJsonObject>& EngravingObject, const TCHAR* Field,
		TArray<FItemOption>& OutOptions)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!EngravingObject->TryGetArrayField(Field, Array))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!Value->TryGetObject(Object))
			{
				continue;
			}

			// 칸이 빠진 줄은 뺀다
			FString StatName;
			if (!(*Object)->TryGetStringField(TEXT("stat"), StatName))
			{
				continue;
			}
			FString TypeName;
			if (!(*Object)->TryGetStringField(TEXT("type"), TypeName))
			{
				continue;
			}
			double Number = 0.0;
			if (!(*Object)->TryGetNumberField(TEXT("value"), Number))
			{
				continue;
			}

			FItemOption Option;

			// 태그를 지웠거나 이름을 바꿨으면 빈 태그가 된다. false = 엔진 에러 로그 대신 아래서 직접 경고
			Option.Stat = FGameplayTag::RequestGameplayTag(FName(*StatName), false);
			if (!Option.Stat.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("[Engrave] 저장된 스탯 %s 가 태그 목록에 없다 — 뺀다"), *StatName);
				continue;
			}
			if (!ValueTypeFromJson(TypeName, Option.ValueType))
			{
				UE_LOG(LogTemp, Warning, TEXT("[Engrave] 저장된 타입 %s 를 모른다 — 뺀다"), *TypeName);
				continue;
			}
			Option.Value = static_cast<float>(Number);

			OutOptions.Add(Option);
		}
	}
}

bool ItemEngraving::EnsureRolled(FItemInstance& Item, const UDataTable* EngraveTable)
{
	if (Item.Engraving.bEngraved)
	{
		return false;
	}

	FItemViewData View;
	if (!ResolveItemViewData(Item.ItemRow, View))
	{
		return false;
	}

	const EEngraveTarget Target = GetTarget(Item, View);
	if (Target == EEngraveTarget::None)
	{
		return false;   // 악세서리 · 소모품 · 재료
	}

	if (!EngraveTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Engrave] 각인 표가 없다 — %s 를 안 바르고 둔다"), *Item.ItemRow.RowName.ToString());
		return false;
	}

	// 다른 구조체의 표를 고르면 FindRow 가 "잘못된 타입" 에러를 찍는다. 구조체부터 본다.
	if (EngraveTable->GetRowStruct() != FEngraveGradeRow::StaticStruct())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Engrave] %s 는 EngraveGradeRow 표가 아니다"), *EngraveTable->GetName());
		return false;
	}

	// 행 이름 = 등급 이름 (Rare / Epic / Legend)
	const UEnum* GradeEnum = StaticEnum<EItemGrade>();
	if (!GradeEnum)
	{
		return false;
	}
	const FName RowName(*GradeEnum->GetNameStringByValue(static_cast<int64>(View.Grade)));

	const FEngraveGradeRow* Row = EngraveTable->FindRow<FEngraveGradeRow>(RowName, TEXT("ItemEngraving::EnsureRolled"), false);
	if (!Row)
	{
		return false;   // 이 등급은 각인을 안 받는다 (일반 등)
	}

	FItemEngraving Result;
	TArray<FItemOption> Used;

	const int32 BenefitCount = RollCount(Row->BenefitCountMin, Row->BenefitCountMax,
		Row->BenefitCountWeights, RowName, TEXT("좋은 옵션"));
	PickOptions(FilterPool(Row->Benefits, Target, RowName), BenefitCount, Used, Result.Benefits, RowName);

	// 나쁜 쪽은 좋은 쪽과 따로 개수를 뽑는다 (독립 시행). 좋은 쪽이 몇 개든 상관없다.
	const int32 RiskCount = RollCount(Row->RiskCountMin, Row->RiskCountMax,
		Row->RiskCountWeights, RowName, TEXT("나쁜 옵션"));
	PickOptions(FilterPool(Row->Risks, Target, RowName), RiskCount, Used, Result.Risks, RowName);

	Result.bEngraved = true;
	Item.Engraving = Result;
	return true;
}

void ItemEngraving::WriteJson(const FItemEngraving& Engraving, const TSharedRef<FJsonObject>& ItemObject)
{
	if (!Engraving.bEngraved)
	{
		return;   // 소모품 · 일반 등급 · 아직 안 굴린 것
	}

	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("engraved"), true);
	Object->SetArrayField(TEXT("benefits"), OptionsToJson(Engraving.Benefits));
	Object->SetArrayField(TEXT("risks"), OptionsToJson(Engraving.Risks));
	ItemObject->SetObjectField(TEXT("engraving"), Object);
}

void ItemEngraving::ReadJson(const TSharedPtr<FJsonObject>& ItemObject, FItemEngraving& OutEngraving)
{
	OutEngraving = FItemEngraving();

	if (!ItemObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (!ItemObject->TryGetObjectField(TEXT("engraving"), Object))
	{
		return;   // 각인이 생기기 전 저장이거나 안 발린 아이템. 불러온 뒤 입구에서 굴린다
	}

	bool bEngraved = false;
	(*Object)->TryGetBoolField(TEXT("engraved"), bEngraved);
	OutEngraving.bEngraved = bEngraved;

	OptionsFromJson(*Object, TEXT("benefits"), OutEngraving.Benefits);
	OptionsFromJson(*Object, TEXT("risks"), OutEngraving.Risks);
}
