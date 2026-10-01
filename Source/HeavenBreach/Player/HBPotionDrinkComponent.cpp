#include "Player/HBPotionDrinkComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/State/HB_StateTag.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Inventory/InventoryComponent.h"
#include "TimerManager.h"

UHBPotionDrinkComponent::UHBPotionDrinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// 기본 애셋은 포션 테스트(test_gm/PotionAnimationSync)가 쓰던 것이다. BP 에서 바꿀 수 있다.
	FirstPersonDrinkAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Levels/test_gm/PotionAnimationSync/Animations/AN_Potion01_LowerView.AN_Potion01_LowerView")));
	ThirdPersonDrinkAnim = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Characters/Warrior/Animations/UpperBody/Interaction/NaturalGripTest/AN_HB_PotionDrink_Natural_Potion_01.AN_HB_PotionDrink_Natural_Potion_01")));
	BottleMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Asset/MultistoryDungeons/Meshes/Props/Potion_01.Potion_01")));

	BottleRelativeTransform = FTransform(
		FRotator(-2.3145f, 90.4719f, -2.0187f),
		FVector(-7.6969f, 4.7575f, -4.5315f),
		FVector(0.46f));

	InterruptTags.AddTag(StateTag::TAG_State_Attacking);
	InterruptTags.AddTag(StateTag::TAG_State_Blocking);
	InterruptTags.AddTag(StateTag::TAG_State_ParryWindow);
	InterruptTags.AddTag(StateTag::TAG_State_Staggered);
	InterruptTags.AddTag(AbilityTag::TAG_State_Dead);
	// 상자를 뒤지는 동작과 같은 상체 슬롯을 쓴다. 뒤지는 중에는 마시지 않는다.
	InterruptTags.AddTag(StateTag::TAG_State_BoxInteracting);
}

void UHBPotionDrinkComponent::BeginPlay()
{
	Super::BeginPlay();

	// 병은 모든 화면에 보여야 해서 각 머신이 자기 것을 만든다. 복제할 필요가 없다.
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetMesh())
	{
		return;
	}

	BottleComp = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("PotionBottle"));
	BottleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BottleComp->SetGenerateOverlapEvents(false);
	BottleComp->SetHiddenInGame(true);
	BottleComp->SetupAttachment(Character->GetMesh(), BottleSocketName);
	BottleComp->SetRelativeTransform(BottleRelativeTransform);
	BottleComp->RegisterComponent();
}

void UHBPotionDrinkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 마시던 중에 사라지면 인벤토리가 사용 중으로 영영 잠긴다. 없던 일로 돌린다.
	if (GetOwner() && GetOwner()->HasAuthority() && IsDrinking())
	{
		if (Inventory)
		{
			Inventory->CancelItemUse(PendingInstanceId);
		}
		PendingInstanceId.Invalidate();
	}
	StopListeningInterrupts();

	if (Inventory)
	{
		Inventory->OnItemUseBegan.RemoveDynamic(this, &UHBPotionDrinkComponent::HandleItemUseBegan);
	}

	Super::EndPlay(EndPlayReason);
}

void UHBPotionDrinkComponent::BindInventory(UInventoryComponent* InInventory)
{
	if (!InInventory || Inventory == InInventory)
	{
		return;
	}

	if (Inventory)
	{
		Inventory->OnItemUseBegan.RemoveDynamic(this, &UHBPotionDrinkComponent::HandleItemUseBegan);
	}
	Inventory = InInventory;

	// 방송은 서버에서만 나오지만, 클라에서 묶어도 해가 없어 역할을 가리지 않는다.
	Inventory->OnItemUseBegan.AddUniqueDynamic(this, &UHBPotionDrinkComponent::HandleItemUseBegan);
}

void UHBPotionDrinkComponent::HandleItemUseBegan(FGuid InstanceId, FName ItemId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Inventory)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

	// 이미 마시는 중이거나(인벤토리가 막지만 한 번 더), 공격·경직 중이면 시작하지 않는다.
	if (IsDrinking() || (ASC && ASC->HasAnyMatchingGameplayTags(InterruptTags)))
	{
		Inventory->CancelItemUse(InstanceId);
		return;
	}

	PendingInstanceId = InstanceId;

	if (ASC)
	{
		for (const FGameplayTag& Tag : InterruptTags)
		{
			const FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
				.AddUObject(this, &UHBPotionDrinkComponent::HandleInterruptTagChanged);
			InterruptHandles.Emplace(Tag, Handle);
		}
	}

	// 길이는 남이 보는 애니메이션 기준이다. 1인칭용과 길이가 같게 만들어져 있다.
	const UAnimSequenceBase* Anim = ThirdPersonDrinkAnim.LoadSynchronous();
	const float Duration = Anim ? Anim->GetPlayLength() : 3.3f;
	GetWorld()->GetTimerManager().SetTimer(DrinkTimerHandle, this, &UHBPotionDrinkComponent::FinishDrink, Duration, false);

	// 마시는 병 외형은 아이템별 WorldMeshPath 를 따른다. 못 찾으면 Multicast 쪽에서
	// 기본 BottleMesh 로 떨어진다.
	FString BottleMeshPath;
	const int32 EntryIndex = Inventory->FindEntryIndexById(InstanceId);
	if (EntryIndex != INDEX_NONE)
	{
		FItemViewData ViewData;
		if (ResolveItemViewData(Inventory->GetEntries()[EntryIndex].Item.ItemRow, ViewData))
		{
			BottleMeshPath = ViewData.WorldMeshPath;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Potion] 음용 시작  Item=%s  Duration=%.2f"), *ItemId.ToString(), Duration);
	Multicast_StartDrink(BottleMeshPath);
}

void UHBPotionDrinkComponent::HandleInterruptTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount > 0 && IsDrinking())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Potion] 음용 끊김  원인=%s  포션은 돌려받음"), *Tag.ToString());
		InterruptDrink();
	}
}

void UHBPotionDrinkComponent::FinishDrink()
{
	if (!IsDrinking())
	{
		return;
	}

	const FGuid InstanceId = PendingInstanceId;
	PendingInstanceId.Invalidate();
	StopListeningInterrupts();

	// 여기서 쿨타임·회복(OnItemUsed → 캐릭터의 ReceiveHeal)·소모가 한꺼번에 일어난다.
	const bool bCommitted = Inventory && Inventory->CommitItemUse(InstanceId);
	UE_LOG(LogTemp, Warning, TEXT("[Potion] 음용 완료  적용=%d"), bCommitted ? 1 : 0);

	Multicast_StopDrink(false);
}

void UHBPotionDrinkComponent::InterruptDrink()
{
	const FGuid InstanceId = PendingInstanceId;
	PendingInstanceId.Invalidate();
	StopListeningInterrupts();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DrinkTimerHandle);
	}

	if (Inventory)
	{
		Inventory->CancelItemUse(InstanceId);
	}

	Multicast_StopDrink(true);
}

void UHBPotionDrinkComponent::StopListeningInterrupts()
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (ASC)
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Entry : InterruptHandles)
		{
			ASC->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::NewOrRemoved).Remove(Entry.Value);
		}
	}
	InterruptHandles.Reset();
}

void UHBPotionDrinkComponent::Multicast_StartDrink_Implementation(const FString& InBottleMeshPath)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// 본인 화면은 카메라가 머리에 붙어 있어 3인칭용 동작으로는 병이 화면 밖으로 나간다.
	UAnimSequenceBase* Anim = Character->IsLocallyControlled()
		? FirstPersonDrinkAnim.LoadSynchronous()
		: ThirdPersonDrinkAnim.LoadSynchronous();
	if (!Anim)
	{
		return;
	}

	PlayingMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Anim, DrinkSlotName, DrinkBlendTime, DrinkBlendTime, 1.f, 1);

	if (BottleComp)
	{
		// 아이템의 WorldMeshPath 를 우선 쓰고, 못 찾으면 기본 BottleMesh 로 떨어진다.
		UStaticMesh* Mesh = !InBottleMeshPath.IsEmpty()
			? TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(InBottleMeshPath)).LoadSynchronous()
			: nullptr;
		if (!Mesh)
		{
			Mesh = BottleMesh.LoadSynchronous();
		}
		if (Mesh)
		{
			BottleComp->SetStaticMesh(Mesh);
		}
		BottleComp->SetHiddenInGame(false);
	}
}

void UHBPotionDrinkComponent::Multicast_StopDrink_Implementation(bool bInterrupted)
{
	// 끝까지 마셨으면 몽타주는 스스로 끝난다. 끊겼을 때만 멈춘다.
	if (bInterrupted && PlayingMontage)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Stop(DrinkBlendTime, PlayingMontage);
		}
	}
	PlayingMontage = nullptr;

	if (BottleComp)
	{
		BottleComp->SetHiddenInGame(true);
	}
}
