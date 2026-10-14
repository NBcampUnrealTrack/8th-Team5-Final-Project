// Copyright HeavenBreach Team5. All Rights Reserved.

#include "Combat/Attack/HBPlayerArrow.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/GameplayCue/HitCuePlay.h"
#include "Combat/Interface/DamageableInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Tag/Ability/HB_AbilityTag.h"
#include "Core/Tag/Cue/HB_CueTag.h"
#include "EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Player/HBPlayerCharacter.h"
#include "Player/HBPlayerController.h"

UProjectileMovementComponent::EHandleBlockingHitResult UHBArrowMovementComponent::HandleBlockingHit(const FHitResult& Hit, const float TimeTick, const FVector& MoveDelta, float& SubTickTimeRemaining)
{
	// 보통은 컴포넌트 Hit 이벤트(OnHit)가 이동 중에 먼저 불려 관통 처리를 끝내 둔다. 여기선 그 결과를 다시 묻는다.
	// (bSimulationUseScopedMovement 를 켜 Hit 이벤트가 늦게 오더라도 여기서 먼저 처리된다.)
	AHBPlayerArrow* Arrow = Cast<AHBPlayerArrow>(GetOwner());
	if (Arrow && Arrow->TryPierce(Hit))
	{
		// 튕김·멈춤 없이 남은 시간만큼 원래 속도로 계속 간다. 그 Pawn 은 이미 무시 등록됐다.
		SubTickTimeRemaining = TimeTick * (1.f - Hit.Time);
		return EHandleBlockingHitResult::AdvanceNextSubstep;
	}

	return Super::HandleBlockingHit(Hit, TimeTick, MoveDelta, SubTickTimeRemaining);
}

AHBPlayerArrow::AHBPlayerArrow()
{
	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComp->InitSphereRadius(4.f);
	CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	// Visibility 는 무시한다. 날아가는 화살이 상호작용 트레이스를 가리지 않게.
	CollisionComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 관통 화살이 Pawn 을 지나가게 하려고 막힘 처리를 가로챈 이동 컴포넌트를 쓴다(이름은 그대로).
	MovementComp = CreateDefaultSubobject<UHBArrowMovementComponent>(TEXT("Movement"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	// 속도는 Launch 가 정한다. 그 전엔 멈춰 있다.
	MovementComp->bAutoActivate = false;

	TrailComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	TrailComp->SetupAttachment(CollisionComp);

	// 평소엔 꺼 둔다. 벽·사물에 박히면 Visibility(상호작용 트레이스)만 막도록 켠다.
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(CollisionComp);
	PickupSphere->InitSphereRadius(15.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	PickupSphere->SetGenerateOverlapEvents(false);

	// 화살은 관통 타격. 평타 화살과 스킬 화살(관통 사격 · 회피 공격)이 모두 이 클래스라 한 번에 적용된다.
	ImpactTypeTag = CueTag::TAG_GameplayCue_Hit_Pierce;
}

void AHBPlayerArrow::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// 클라 트레이스와 서버 판정이 같도록 모든 머신에서 BP 의 PickupRadius 를 반영한다.
	PickupSphere->SetSphereRadius(PickupRadius);
}

void AHBPlayerArrow::Launch(AHBPlayerCharacter* InShooter, const float InSpeed, const float InDamageCoef, const FDataTableRowHandle& InAmmoRow, const FHBArrowLaunchOptions& Options)
{
	Shooter = InShooter;
	DamageCoef = InDamageCoef;
	AmmoItemRow = InAmmoRow;
	PierceRemaining = FMath::Max(Options.MaxPierceCount, 0);
	bSkillArrow = Options.bSkillArrow;
	SkillCastId = Options.SkillCastId;
	SetLifeSpan(ArrowLifeSpan);

	if (InShooter)
	{
		// 쏜 사람 몸(캡슐·메시)에 바로 부딪히지 않게.
		CollisionComp->IgnoreActorWhenMoving(InShooter, true);
	}

	if (HasAuthority())
	{
		CollisionComp->OnComponentHit.AddDynamic(this, &AHBPlayerArrow::OnHit);
	}

	MovementComp->ProjectileGravityScale = GravityScale;
	MovementComp->InitialSpeed = InSpeed;
	MovementComp->MaxSpeed = InSpeed;
	MovementComp->Velocity = GetActorForwardVector() * InSpeed;
	MovementComp->Activate(true);
}

void AHBPlayerArrow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AHBPlayerArrow, AmmoItemRow);
	DOREPLIFETIME(AHBPlayerArrow, StuckState);
}

void AHBPlayerArrow::OnHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
	if (bHasHit || !HasAuthority())
	{
		return;
	}

	// 관통 화살이 뚫고 지나가는 Pawn 이면 여기서 끝. 이동 컴포넌트가 멈추지 않고 계속 날린다.
	if (TryPierce(Hit))
	{
		return;
	}

	bHasHit = true;

	UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 명중  대상=%s  컴포넌트=%s  본=%s"),
		*GetNameSafe(OtherActor), *GetNameSafe(Hit.GetComponent()), *Hit.BoneName.ToString());

	// 맞은 쪽이 피해를 받으면 그쪽이 자기 큐를 쏜다(막기·패링에 따라 소리가 갈리므로).
	bool bTargetPlayedCue = false;

	// 관통으로 이미 맞은 대상은 다시 피해를 주지 않는다.
	if (OtherActor && OtherActor != Shooter && !HitActors.Contains(OtherActor))
	{
		HitActors.Add(OtherActor);
		bTargetPlayedCue = ApplyArrowDamage(OtherActor, Hit);
	}

	// 벽·사물·시체처럼 피해를 안 받는 곳은 아무도 큐를 안 쏘므로 화살이 쏜다.
	// 적 발사체(AEnemyProjectile::OnHit)와 같은 규칙이다. 재질은 HitCuePlay 가 Hit 에서 읽는다.
	if (!bTargetPlayedCue)
	{
		FGameplayTagContainer ImpactTags;
		if (ImpactTypeTag.IsValid())
		{
			ImpactTags.AddTag(ImpactTypeTag);
		}
		HitCuePlay::PlayImpact(Shooter, OtherActor, Hit, 0.f, ImpactTags);
	}

	StickTo(OtherActor, Hit);
}

bool AHBPlayerArrow::TryPierce(const FHitResult& Hit)
{
	if (!HasAuthority() || bHasHit)
	{
		return false;
	}

	APawn* HitPawn = Cast<APawn>(Hit.GetActor());
	if (!HitPawn || HitPawn == Shooter)
	{
		return false;
	}

	// 이미 뚫은 Pawn(OnHit 과 이동 컴포넌트가 같은 충돌로 두 번 부름 등) — 계속 지나간다.
	if (PiercedActors.Contains(HitPawn))
	{
		return true;
	}

	// 시체(State.Dead)는 일반 화살도 그냥 지나간다. 시체 메시가 WorldDynamic 을 막아 바닥의 시체 위에 화살이 박히던 문제.
	// 피해·관통 횟수 없이 이후 이동에서 무시만 한다.
	const UAbilitySystemComponent* HitASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(HitPawn);
	if (HitASC && HitASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead))
	{
		PiercedActors.Add(HitPawn);
		CollisionComp->IgnoreActorWhenMoving(HitPawn, true);
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살이 시체를 지나감  대상=%s"), *GetNameSafe(HitPawn));
		return true;
	}

	if (PierceRemaining <= 0)
	{
		return false;
	}

	// 피해를 받을 수 있는 Pawn 만 관통 횟수를 쓴다. 시체·사망자처럼 피해를 거부하는 Pawn 은 횟수 없이 지나간다.
	// 아군도 Damageable 이고 CanBeDamaged 가 true 면(팀킬 허용) 횟수를 쓴다.
	const bool bCountsTowardLimit = Shooter && HitPawn->Implements<UDamageableInterface>()
		&& IDamageableInterface::Execute_CanBeDamaged(HitPawn, Shooter);
	if (bCountsTowardLimit)
	{
		--PierceRemaining;
	}
	PiercedActors.Add(HitPawn);

	// 이 Pawn 의 모든 컴포넌트(캡슐·메시)를 이후 이동에서 무시한다. 남은 이동 시간은 같은 방향으로 계속 간다.
	CollisionComp->IgnoreActorWhenMoving(HitPawn, true);

	if (!HitActors.Contains(HitPawn))
	{
		HitActors.Add(HitPawn);
		ApplyArrowDamage(HitPawn, Hit);
	}

	UE_LOG(LogTemp, Warning, TEXT("[BowSkill] 화살 관통  대상=%s  남은 관통=%d  횟수소모=%d"), *GetNameSafe(HitPawn), PierceRemaining, bCountsTowardLimit ? 1 : 0);
	return true;
}

bool AHBPlayerArrow::ApplyArrowDamage(AActor* Target, const FHitResult& Hit)
{
	if (!Shooter || !Target->Implements<UDamageableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 피해 없음 (Damageable 아님)  대상=%s"), *GetNameSafe(Target));
		return false;
	}

	if (!IDamageableInterface::Execute_CanBeDamaged(Target, Shooter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 피해 없음 (대상이 거부)  대상=%s"), *GetNameSafe(Target));
		return false;
	}

	// 근접 판정과 같은 순서: 공격력 × 계수 → 패시브 보정 → 맞는 쪽에 수치만 넘김.
	// 헤드샷은 맞는 쪽이 Hit 의 본으로 판단한다.
	float FinalDamage = Shooter->GetAttackPower() * DamageCoef;
	// 스킬 화살은 스킬 피해 배율(옵션)을 패시브 보정 전에 곱한다.
	if (bSkillArrow)
	{
		FinalDamage *= Shooter->GetSkillDamageMultiplier();
	}
	// 화살은 지금 시전이 아니라 자기 SkillCastId 로만 잔류 마력을 알린다(아래).
	FinalDamage = Shooter->ModifyOutgoingDamage(Target, FinalDamage, false);

	// 잔류 마력: 스킬 화살은 자기 시전 ID 로 알린다. 화살이 날아가는 사이 스킬이 끝나도(State.UsingSkill 없음) 적용된다.
	if (SkillCastId > 0)
	{
		Shooter->NotifySkillHit(SkillCastId);
	}

	{
		// 맞는 순간에만 쏜 사람에게 화살 성질을 붙인다. 맞는 쪽 큐가 이걸 보고 소리를 고른다.
		// 화살은 GA 가 끝난 뒤에 맞아서, 이게 없으면 쏜 사람에게 태그가 없어 기본음이 났다.
		HitCuePlay::FScopedImpactTag ImpactScope(Shooter, ImpactTypeTag);
		IDamageableInterface::Execute_ApplyMeleeHit(Target, Shooter, FinalDamage, Hit, EHBDamageSource::Ranged);
	}
	UE_LOG(LogTemp, Warning, TEXT("[Bow] 피해 %.1f → %s"), FinalDamage, *GetNameSafe(Target));

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	const bool bKilled = TargetASC && TargetASC->HasMatchingGameplayTag(AbilityTag::TAG_State_Dead);
	Shooter->NotifyDamageDealt(Target, FinalDamage, bKilled);

	// 한 발이 맞으면 내구도 1. 빗나가면 안 닳는다(근접 헛스윙과 같다). 관통으로 여러 명 맞아도 한 발에 1.
	if (bDurabilitySpent)
	{
		return true;
	}
	bDurabilitySpent = true;
	if (UEquipmentComponent* Equipment = Shooter->GetEquipment())
	{
		bool bJustBroke = false;
		Equipment->DamageEquippedItemDurability(EEquipSlotType::PrimaryWeapon, 1.f, bJustBroke);
	}
	return true;
}

void AHBPlayerArrow::StickTo(AActor* HitActor, const FHitResult& Hit)
{
	UPrimitiveComponent* HitComp = Hit.GetComponent();
	const EHBArrowStuckKind Kind = HBArrow::ResolveStuckKind(HitActor, HitComp);
	if (Kind == EHBArrowStuckKind::None || !IsValid(HitActor))
	{
		Destroy();
		return;
	}

	MovementComp->StopMovementImmediately();
	MovementComp->Deactivate();

	// 맞은 지점에서 조금 더 밀어 넣어 촉이 파묻힌 것처럼 보이게 한다.
	SetActorLocation(GetActorLocation() + GetActorForwardVector() * StickPenetration, false, nullptr, ETeleportType::TeleportPhysics);

	// 몬스터·플레이어는 맞은 본에 붙여 움직임·래그돌을 따라간다(EnemyProjectile HitBehavior 와 같은 방식).
	// 벽·사물은 움직이면서 네트워크로 찾을 수 있는 것만 붙인다. 런타임에 만든 던전 조각처럼 복제되지 않는
	// 정적 물체에 붙이면 클라가 부모를 못 찾아 위치가 어긋날 수 있어서, 그런 곳은 그 자리에 멈춘 채로 둔다.
	UPrimitiveComponent* AttachComp = HitComp;
	FName AttachBone = Hit.BoneName;
	bool bAttach = false;
	if (Kind == EHBArrowStuckKind::Character)
	{
		bAttach = true;
		// 캡슐에 먼저 맞으므로(본 이름 없음) 메시의 최근접 본에 붙인다. 그래야 애니메이션·래그돌을 따라간다.
		if (!Cast<USkeletalMeshComponent>(HitComp))
		{
			if (const ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
			{
				if (USkeletalMeshComponent* CharMesh = HitCharacter->GetMesh())
				{
					const FName ClosestBone = CharMesh->FindClosestBone(Hit.ImpactPoint, nullptr, 0.f, /*bRequirePhysicsAsset*/ true); // IK·가상 본 제외
					if (!ClosestBone.IsNone())
					{
						AttachComp = CharMesh;
						AttachBone = ClosestBone;
					}
				}
			}
		}
	}
	else
	{
		bAttach = HitComp->Mobility == EComponentMobility::Movable && HitComp->IsSupportedForNetworking();
	}

	if (bAttach)
	{
		AttachToComponent(AttachComp, FAttachmentTransformRules::KeepWorldTransform, AttachBone);
	}

	if (Kind == EHBArrowStuckKind::Character)
	{
		HitActor->OnDestroyed.AddDynamic(this, &AHBPlayerArrow::OnStuckActorDestroyed);
		SetLifeSpan(CharacterStickLifeSpan);
	}
	else
	{
		SetLifeSpan(WorldStickLifeSpan);
	}

	StuckState.Kind = Kind;
	// World 는 Actor 를 안 읽고, 복제 안 되는 런타임 던전 조각일 수 있어 비운다.
	StuckState.Actor = (Kind == EHBArrowStuckKind::Character) ? HitActor : nullptr;

	// 서버는 RepNotify 가 자동으로 안 불린다.
	OnRep_StuckState();
	ForceNetUpdate();

	UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 박힘  종류=%s  대상=%s  컴포넌트=%s  본=%s"),
		Kind == EHBArrowStuckKind::World ? TEXT("World") : TEXT("Character"), *GetNameSafe(HitActor),
		bAttach ? *GetNameSafe(AttachComp) : TEXT("(none)"), *AttachBone.ToString());
}

void AHBPlayerArrow::OnRep_StuckState()
{
	if (StuckState.Kind == EHBArrowStuckKind::None)
	{
		return;
	}

	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 비행 연출을 끈다. BP 자식(관통 화살)이 단 것까지: 나이아가라는 전부, 기류 메시·빛은 FlightOnly 태그가 붙은 것.
	TInlineComponentArray<UNiagaraComponent*> Niagaras(this);
	for (UNiagaraComponent* Niagara : Niagaras)
	{
		Niagara->Deactivate();
	}
	static const FName FlightOnlyTag(TEXT("FlightOnly"));
	for (UActorComponent* Comp : GetComponentsByTag(USceneComponent::StaticClass(), FlightOnlyTag))
	{
		CastChecked<USceneComponent>(Comp)->SetVisibility(false);
	}

	if (StuckState.Kind == EHBArrowStuckKind::World)
	{
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		return;
	}

	// 맞은 본인 1인칭 화면에선 숨긴다. 머리에 박히면 카메라를 가린다.
	if (const APawn* StuckPawn = Cast<APawn>(StuckState.Actor))
	{
		if (StuckPawn->IsLocallyControlled())
		{
			MeshComp->SetHiddenInGame(true);
		}
	}
}

void AHBPlayerArrow::OnStuckActorDestroyed(AActor* DestroyedActor)
{
	Destroy();
}

bool AHBPlayerArrow::CanInteract_Implementation(AActor* Interactor) const
{
	return StuckState.Kind == EHBArrowStuckKind::World && !AmmoItemRow.IsNull() && !bPickedUp;
}

FText AHBPlayerArrow::GetInteractPrompt_Implementation() const
{
	return PickupPrompt;
}

void AHBPlayerArrow::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || !CanInteract_Implementation(Interactor))
	{
		return;
	}

	AHBPlayerCharacter* Player = Cast<AHBPlayerCharacter>(Interactor);
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Inventory)
	{
		return;
	}

	// 이름은 Gold 지만 아무 행이나 넣는다. 같은 화살 스택이 있으면 합쳐진다.
	if (Inventory->AddGold(AmmoItemRow, 1))
	{
		bPickedUp = true;
		UE_LOG(LogTemp, Warning, TEXT("[Bow] 화살 회수  %s"), *GetNameSafe(Player));
		Destroy();
		return;
	}

	if (AHBPlayerController* PC = Player->GetController<AHBPlayerController>())
	{
		PC->ClientShowHudNotification(BagFullMessage);
	}
}
