#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/GameFlow/HBPostMatchTypes.h"
#include "HBPlayerController.generated.h"

class UCommonActivatableWidget;
class UHB_ContainerScreen;
class UHB_HUDLayer;
class UHB_UILayout;
class UInventoryComponent;
class UInputAction;
class UInputMappingContext;
class UInventoryUIComponent;
class UHB_SkillAugment;
class UHB_SpectateWidget;
class UUserWidget;
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

	UFUNCTION(BlueprintCallable, Category = "Player|UI")
	void ToggleSkillAugment();

	// 상자 / 시체 등 외부 인벤토리를 Menu 레이어에 띄운다.
	// InteractionComponent 의 OnRequestOpenContainer 를 받은 쪽에서 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Player|UI")
	void OpenContainer(UInventoryComponent* InContainer);

	UFUNCTION(BlueprintCallable, Category = "Player|UI")
	void CloseContainer();

	UFUNCTION(BlueprintPure, Category = "Player|UI")
	bool IsContainerOpen() const { return ContainerScreen != nullptr; }

	// 서버 전용. 빙의를 푼 뒤 부른다. 살아 있는 팀원 시점으로 넘어간다.
	void StartSpectatingTeammates();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// 접속이 끊기면 Logout 보다 먼저 불려 폰을 지운다. 살아 있던 사람은 여기서 시체로 남긴다.
	virtual void PawnLeavingGame() override;

	UPROPERTY(ReplicatedUsing = OnRep_SpectatingTeammates)
	bool bSpectatingTeammates = false;

	UFUNCTION()
	void OnRep_SpectatingTeammates();

	UFUNCTION(Server, Reliable)
	void ServerSpectateNext();

	// 서버. Current 다음 생존 팀원 폰. 없으면 nullptr.
	APawn* FindNextSpectateTarget(const AActor* Current) const;

	// 서버. 보던 대상이 죽거나 사라지면 다음 사람으로.
	void CheckSpectateTarget();

	FTimerHandle SpectateCheckTimer;

	UPROPERTY(Transient)
	TObjectPtr<UHB_SpectateWidget> SpectateWidget;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> SkillAugmentAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|UI")
	TSubclassOf<UHB_SkillAugment> SkillAugmentScreenClass;

	UPROPERTY(Transient)
	TObjectPtr<UHB_SkillAugment> SkillAugmentScreen;

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

	// ExtractionResult 맵 전용: 탈출 성공(및 일반 매치 종료) 시 띄울 결과 위젯 (구 WBP_ExtractionResult)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameFlow|Result")
	TSubclassOf<UUserWidget> ExtractionResultWidgetClass;

	// ExtractionResult 맵 전용: 사망으로 인해 도착했을 때 띄울 결과 위젯
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameFlow|Result")
	TSubclassOf<UUserWidget> DeathResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UHB_UILayout> UILayout;

	// Game 레이어에 올린 HUD. 피격 연출처럼 외부에서 HUD 를 직접 건드려야 할 때 쓴다
	UPROPERTY(Transient)
	TObjectPtr<UHB_HUDLayer> HUDLayer;

	UPROPERTY(Transient)
	TObjectPtr<UHB_ContainerScreen> ContainerScreen;

	// 상자 창이 열리고 닫힐 때 캐릭터의 뒤지는 동작을 켜고 끈다.
	void SetPawnInteracting(bool bInteracting);

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

public:
	/** Sent by the server right before ClientTravel to the post-match map, since PlayerState/GameState don't survive that travel. */
	UFUNCTION(Client, Reliable, Category = "GameFlow")
	void ClientSetPostMatchReason(EHBPostMatchReason Reason);

protected:
	void CreateUILayout();

	// ExtractionResult 맵에 도착했을 때만 동작. UHBMatchResultSubsystem에 저장된 사유를 읽어서
	// ExtractionResultWidgetClass / DeathResultWidgetClass 중 하나를 CreateWidget + AddToViewport 하고
	// 마우스 커서/UI 입력모드를 켠다. 반환값 true면 ExtractionResult 맵이라는 뜻 (호출자가 인게임
	// HUD/입력 세팅을 건너뛰는 데 사용).
	bool TryShowPostMatchResultWidget();

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
	// Scale 은 흔들림 세기 배율이다. 1.0 이 애셋에 적힌 그대로.
	// 상황마다 클래스를 나누지 않고 이 값으로 세기를 가른다.
	void ClientPlayCameraShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale = 1.f);

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
	/**
	 * bSuppressScreenEffect: 패링·방어태세로 피해가 0이면 참.
	 *
	 * HP 바는 그대로 갱신하고 붉은 화면 연출만 건너뛴다. 막아냈는데 피가
	 * 튀는 화면이 나오면 성공한 건지 맞은 건지 구분이 안 된다.
	 * 기획서 19.3 도 "화면 전체를 붉게 덮는 피격 연출은 사용하지 않는다" 다.
	 *
	 * 일반 피격의 붉은 효과는 그대로 둔다. 그쪽까지 없앨지는 별도 판단이다.
	 */
	UFUNCTION(Client,Reliable)
	void ClientPlayHitEffect(float NewHP, float NewMaxHP, bool bSuppressScreenEffect);
#pragma endregion
};
