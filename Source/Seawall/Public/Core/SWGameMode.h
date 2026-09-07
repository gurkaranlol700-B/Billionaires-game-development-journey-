// SEAWALL -- default game mode.
//
// Exists mainly to say "the player is an ASWCharacter". Kept as C++ rather than
// a Blueprint so the pawn class cannot be silently unset by a content edit.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SWGameMode.generated.h"

UCLASS()
class SEAWALL_API ASWGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASWGameMode();
};
