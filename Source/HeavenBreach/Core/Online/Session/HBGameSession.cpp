#include "Core/Online/Session/HBGameSession.h"

void AHBGameSession::RegisterServer()
{
	// RequestHostLobby is the sole creator of Steam sessions. GameMode creation
	// and ServerTravel must never advertise a second dedicated/server session.
}
