#include "Audio/SWSurfaceVolume.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"

ASWSurfaceVolume::ASWSurfaceVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(100.f, 100.f, 60.f));
	// Query only: this marks an area, it must never push the player around.
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->bHiddenInGame = true;
	RootComponent = Bounds;
}

bool ASWSurfaceVolume::FindOverrideAt(const UWorld* World, const FVector& Location, EPhysicalSurface& OutSurface)
{
	if (!World)
	{
		return false;
	}

	bool bFound = false;
	int32 BestPriority = MIN_int32;

	// Iterating the volumes is cheaper than an overlap query here: there are a
	// handful of them per level and this runs once per footstep, not per frame.
	for (TActorIterator<ASWSurfaceVolume> It(World); It; ++It)
	{
		const ASWSurfaceVolume* Volume = *It;
		if (!Volume || !Volume->Bounds)
		{
			continue;
		}

		const FVector Local = Volume->GetActorTransform().InverseTransformPosition(Location);
		const FVector Extent = Volume->Bounds->GetUnscaledBoxExtent();
		const bool bInside =
			FMath::Abs(Local.X) <= Extent.X &&
			FMath::Abs(Local.Y) <= Extent.Y &&
			FMath::Abs(Local.Z) <= Extent.Z;

		if (bInside && Volume->Priority >= BestPriority)
		{
			BestPriority = Volume->Priority;
			OutSurface = Volume->Surface;
			bFound = true;
		}
	}

	return bFound;
}
