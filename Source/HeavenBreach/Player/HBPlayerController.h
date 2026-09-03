#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HBPlayerController.generated.h"

class UCommonActivatableWidget;
class UHB_ContainerScreen;
class UHB_HUDLayer;
class UHB_UILayout;
class UInventoryComponent;
class UInputAction;
class UInputMappingContext;
class UInventoryUIComponent;
struct FInputActionValue;

UCLASS(Blueprintable)
class HEAVENBREACH_API AHBPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHBPlayerController();

	UFUNCTION(BlueprintCallable, Category = "Player|Inventory")
	void RequestCloseInventory();

	UFUNCTION(BlueprintCallable, Category = "Travel")
	void TravelToLevel(const FString& InLevelPath);

	// Root CommonUI widget. Every screen is pushed into one of its layers.
	UFUNCTION(BlueprintPure, Category = "Player|UI")
	UHB_UILayout* GetUILayout() const { return UILayout; }

	UFUNCTION(BlueprintPure, Category = "Player|UI")
	UHB_HUDLayer* GetHUDLayer() const { return HUDLayer; }

	// 상자 / 시체 등 외부 인벤토리를 Menu 레이어에 띄운다.
	// InteractionComponent 의 OnRequestOpenContainer 를 받은 쪽에서 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Player|UI")
	void OpenContainer(UInventoryComponent* InContainer);

	UFUNCTION(BlueprintCallable, Category = "Player|UI")
	void CloseContainer();

	UFUNCTION(BlueprintPure, Category = "Player|UI")
	bool IsContainerOpen() const { return ContainerScreen != nullptr; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputMappingContext> PlayerMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> BlockAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> Skill1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> Skill2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	int32 MappingPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> ToggleInventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> QuickSlot1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> QuickSlot2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> QuickSlot3Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> QuickSlot4Action;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Player|Inventory")
	TObjectPtr<UInventoryUIComponent> InventoryUI;

	// WBP_UILayout
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UHB_UILayout> UILayoutClass;

	// WBP_MainHUD, pushed into the UI.Layer.Game stack on begin play
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UCommonActivatableWidget> HUDLayerClass;

	// WBP_BoxContainer
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UHB_ContainerScreen> ContainerScreenClass;

	UPROPERTY(Transient)
	TObjectPtr<UHB_UILayout> UILayout;

	// Game 레이어에 올린 HUD. 피격 연출처럼 외부에서 HUD 를 직접 건드려야 할 때 쓴다
	UPROPERTY(Transient)
	TObjectPtr<UHB_HUDLayer> HUDLayer;

	UPROPERTY(Transient)
	TObjectPtr<UHB_ContainerScreen> ContainerScreen;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	
	////////////////////////////////////
	// Test for session
	UFUNCTION(Exec)
	void TestCreateSteamLobby();
	
	UFUNCTION(Exec)
	void TestFindSteamLobby();
	
	UFUNCTION(Exec)
	void TestJoinSteamLobby(int32 InSearchResultIndex);
	
	UFUNCTION(Exec)
	void TestLeaveSteamLobby();
	
	UFUNCTION(Exec)
	void TestConnectMatchServer();
	
	UFUNCTION(Exec)
	void TestPublishMatchServer();
	
	UFUNCTION(Exec)
	void TestConnectPublishdMatchServer();
	
	UFUNCTION(Exec)
	void TestStartMatch();

	// 테스트 전용: 지정한 전용 서버 주소로 매치 시작 이동을 검증한다.
	UFUNCTION(Exec)
	void TestStartMatchAtAddress(const FString& ServerAddress);
	
	UFUNCTION(Exec)
	void TestExtract();
	
	UFUNCTION(Server, Reliable)
	void ServerTestExtract();
	
	UFUNCTION(BlueprintCallable, Category = "GameFlow")
	void ReturnToHub();

	UFUNCTION(Exec)
	void TestReturnToHub();
	////////////////////////////////////


	void CreateUILayout();

private:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void StartCrouch();
	void StopCrouch();
	void Attack();
	void Block();
	void StopBlock();
	void ToggleInventory();
	void StartSprint();
	void StopSprint();
	void ActivateSkill1();
	void ActivateSkill2();
	void UseQuickSlot1();
	void UseQuickSlot2();
	void UseQuickSlot3();
	void UseQuickSlot4();

private:
	bool bWasQuickSlot1Pressed = false;
	bool bWasQuickSlot2Pressed = false;
	bool bWasQuickSlot3Pressed = false;
	bool bWasQuickSlot4Pressed = false;
	
#pragma region CameraShake
public:
	UFUNCTION(Client,Reliable)
	void ClientPlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass);

	/**
	 * 피격 혈흔 연출을 재생하고 HP 바를 서버 값으로 즉시 맞춘다.
	 *
	 * 복제를 기다리지 않는 이유: AttributeSet 이 PlayerState 에 붙어 있고
	 * APlayerState 의 NetUpdateFrequency 는 1 이라, HP 는 최대 1초 늦게 도착한다.
	 * 사운드(GameplayCue)와 카메라쉐이크는 즉시 나가므로 연출과 HP 바만 뒤늦게 반응했다.
	 * 그래서 쉐이크와 동일하게 피격 시점의 Client RPC 로 보낸다.
	 *
	 * 뒤늦게 도착하는 OnHPChanged 는 같은 값을 다시 넣으므로 화면상 변화가 없다.
	 * 회복 / 리스폰 등 피격 이외의 HP 변화는 여전히 복제 경로를 탄다.
	 */
	UFUNCTION(Client,Reliable)
	void ClientPlayHitEffect(float NewHP, float NewMaxHP);
#pragma endregion
};
