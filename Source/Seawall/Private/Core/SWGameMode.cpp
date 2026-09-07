#include "Core/SWGameMode.h"

#include "Player/SWCharacter.h"

ASWGameMode::ASWGameMode()
{
	DefaultPawnClass = ASWCharacter::StaticClass();
	// No HUD or player state yet -- those arrive with the inventory at M3.
}
