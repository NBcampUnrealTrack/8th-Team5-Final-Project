// Copyright HeavenBreach Team5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

/**
 *  상호작용 가능한 오브젝트가 구현하는 인터페이스.
 *  상자(ABoxActor), 문, NPC 등이 공통으로 사용한다.
 *  Interact()는 반드시 서버에서만 호출한다. (권한 판정은 호출측 InteractionComponent가 책임)
 */
UINTERFACE(MinimalAPI, BlueprintType, meta = (DisplayName = "Interactable Interface"))
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class HEAVENBREACH_API IInteractableInterface
{
	GENERATED_BODY()

public:

	/** 현재 이 오브젝트와 상호작용할 수 있는 상태인지 반환 (거리 판정 등은 호출측이 별도 수행) */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** 상호작용 실행. 반드시 서버에서만 호출한다. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);

	/** 상호작용 프롬프트 문구 반환 (예: "상자 열기 [E]"). 로컬 UI 표시에 사용. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractPrompt() const;
};
