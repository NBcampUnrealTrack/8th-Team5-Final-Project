#pragma once
#include "CoreMinimal.h"

namespace HBLobbyProtocol
{
    HEAVENBREACH_API FString PasswordProof(const FString& Password, const FString& Salt);
    HEAVENBREACH_API bool IsValidRoomName(const FString& Name);
}
