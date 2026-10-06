// SEAWALL -- "the floor sounds different in here".
//
// The corridor floor is one material, but it is not one surface: there is a
// puddle by the doorway, rubble under the collapsed section, a steel grating
// over the drain. Authoring separate materials for each would be wasteful, so
// instead a box says "inside me, the floor is water", and the footstep trace
// asks the boxes first.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "GameFramework/Actor.h"
#include "SWSurfaceVolume.generated.h"

class UBoxComponent;

UCLASS()
class SEAWALL_API ASWSurfaceVolume : public AActor
{
	GENERATED_BODY()

public:
	ASWSurfaceVolume();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seawall|Surface")
	TObjectPtr<UBoxComponent> Bounds;

	/** What the floor counts as while the player is standing inside this box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Surface")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType1;

	/** Higher wins where volumes overlap -- a puddle on top of rubble is still a puddle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Surface")
	int32 Priority = 0;

	/**
	 * Returns true and fills OutSurface if any volume contains Location.
	 * Static so the footstep code can ask without holding a reference to anything.
	 */
	static bool FindOverrideAt(const UWorld* World, const FVector& Location, EPhysicalSurface& OutSurface);
};
