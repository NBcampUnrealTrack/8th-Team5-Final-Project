#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HBInteractAnimComponent.generated.h"

class UAnimMontage;
class UAnimSequenceBase;

/**
 * 상자·시체를 뒤지는 동작. 켜지면 Start → Loop 반복, 꺼지면 End.
 *
 * 켜고 끄는 판단은 본인 클라이언트가 한다(길게 누르기 시작, 상자 창 열림/닫힘 모두 클라에서 일어난다).
 * 서버에 알리면 서버가 복제 변수로 모든 화면에 퍼뜨린다.
 * 멀티캐스트가 아니라 복제 변수인 이유: 나중에 시야에 들어온 플레이어도 지금 상태를 받아야 한다.
 *
 * 켜져 있는 동안 State.BoxInteracting 을 붙인다. 평타·스킬·패링이 이미 이 태그로 막히게 되어 있다.
 */
UCLASS(ClassGroup = (Player), meta = (BlueprintSpawnableComponent))
class HEAVENBREACH_API UHBInteractAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHBInteractAnimComponent();

	// 본인 클라이언트(또는 호스트)가 부른다. 같은 값이면 아무 일도 없다.
	void SetInteracting(bool bNewInteracting);

	bool IsInteracting() const { return bInteracting; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	TSoftObjectPtr<UAnimSequenceBase> StartAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	TSoftObjectPtr<UAnimSequenceBase> LoopAnim;

	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	TSoftObjectPtr<UAnimSequenceBase> EndAnim;

	// 정식 전사 ABP(ABP_Warrior_F) 의 상체 슬롯. 포션과 같은 슬롯이다.
	UPROPERTY(EditDefaultsOnly, Category = "Interact")
	FName SlotName = TEXT("UpperBody");

	UPROPERTY(EditDefaultsOnly, Category = "Interact", meta = (ClampMin = "0.0", Units = "s"))
	float BlendTime = 0.12f;

private:
	UFUNCTION(Server, Reliable)
	void Server_SetInteracting(bool bNewInteracting);

	UFUNCTION()
	void OnRep_Interacting();

	// 서버와 각 클라가 값이 바뀐 뒤 부른다. 애니메이션과 태그를 맞춘다.
	void ApplyInteracting();

	void PlayLoop();
	void SetBoxInteractingTag(bool bAdd);

	UPROPERTY(ReplicatedUsing = OnRep_Interacting)
	bool bInteracting = false;

	// 이 머신에서 마지막으로 적용한 값. OnRep 이 같은 값으로 두 번 와도 다시 재생하지 않게 한다.
	bool bAppliedInteracting = false;

	UPROPERTY()
	TObjectPtr<UAnimMontage> PlayingMontage;

	FTimerHandle LoopTimerHandle;
};
