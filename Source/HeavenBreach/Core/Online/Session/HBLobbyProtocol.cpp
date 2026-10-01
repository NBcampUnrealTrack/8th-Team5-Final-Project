#include "Core/Online/Session/HBLobbyProtocol.h"
#include "Misc/SecureHash.h"

FString HBLobbyProtocol::PasswordProof(const FString& Password, const FString& Salt)
{
    if (Password.IsEmpty()) return FString();
    const FTCHARToUTF8 Bytes(*(Salt + TEXT(":") + Password));
    uint8 Hash[FSHA1::DigestSize];
    FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash);
    return BytesToHex(Hash, FSHA1::DigestSize);
}

bool HBLobbyProtocol::IsValidRoomName(const FString& Name)
{
    const FString Trimmed = Name.TrimStartAndEnd();
    return !Trimmed.IsEmpty() && Trimmed.Len() <= 48 && !Trimmed.Contains(TEXT("\n")) && !Trimmed.Contains(TEXT("\r"));
}
