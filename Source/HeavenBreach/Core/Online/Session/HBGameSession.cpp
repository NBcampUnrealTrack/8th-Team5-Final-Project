#include "Core/Online/Session/HBGameSession.h"
#include "Core/Online/Session/HBOnlineSessionSubsystem.h"
#include "Engine/GameInstance.h"

FString AHBGameSession::ApproveLogin(const FString& Options)
{
	const UGameInstance* Instance = GetGameInstance();
	const UHBOnlineSessionSubsystem* Sessions = Instance ? Instance->GetSubsystem<UHBOnlineSessionSubsystem>() : nullptr;
	if (Sessions && Sessions->IsMatchAdmissionClosed())
	{
		return TEXT("Match admission is closed");
	}
	return Super::ApproveLogin(Options);
}

void AHBGameSession::RegisterServer()
{
	// RequestHostLobby is the sole creator of Steam sessions. GameMode creation
	// and ServerTravel must never advertise a second dedicated/server session.
}
