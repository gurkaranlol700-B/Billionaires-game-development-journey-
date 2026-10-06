#include "Audio/SWRandomEmitter.h"

#include "Audio/SWSoundSet.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ASWRandomEmitter::ASWRandomEmitter()
{
	PrimaryActorTick.bCanEverTick = true;
	// A sound every few seconds does not need a 60 Hz tick.
	PrimaryActorTick.TickInterval = 0.25f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ASWRandomEmitter::BeginPlay()
{
	Super::BeginPlay();
	ResetTimer();
	// Stagger the first play, or every emitter in the level fires together on load.
	TimeToNextPlay *= FMath::FRandRange(0.1f, 1.f);
}

void ASWRandomEmitter::ResetTimer()
{
	TimeToNextPlay = FMath::FRandRange(FMath::Min(IntervalRange.X, IntervalRange.Y),
		FMath::Max(IntervalRange.X, IntervalRange.Y));
}

bool ASWRandomEmitter::PickLocation(FVector& OutLocation) const
{
	const FVector Origin = GetActorLocation();

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;

	// Up to a few attempts to find a spot behind the player; if the player is
	// staring straight at the emitter, stay quiet rather than breaking the illusion.
	const int32 Attempts = bOnlyOutOfView ? 6 : 1;
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const FVector Candidate = Origin + FMath::VRand() * FMath::FRandRange(0.f, ScatterRadius);

		if (!bOnlyOutOfView || !Pawn)
		{
			OutLocation = Candidate;
			return true;
		}

		FVector ToSound = Candidate - Pawn->GetActorLocation();
		if (ToSound.Normalize())
		{
			// Roughly outside a 100 degree cone in front of the player.
			if (FVector::DotProduct(Pawn->GetActorForwardVector(), ToSound) < 0.64f)
			{
				OutLocation = Candidate;
				return true;
			}
		}
	}

	return false;
}

void ASWRandomEmitter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bActive || !Sounds || !Sounds->IsPlayable())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
	{
		if (FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > MaxPlayerDistance)
		{
			return;   // too far to matter; the timer waits rather than firing into nothing
		}
	}

	TimeToNextPlay -= DeltaSeconds;
	if (TimeToNextPlay > 0.f)
	{
		return;
	}

	FVector Location;
	if (PickLocation(Location))
	{
		Sounds->PlayAtLocation(this, Location, VolumeScale);
		ResetTimer();
	}
	else
	{
		// Player is looking right at it -- try again shortly instead of resetting
		// the whole interval, so the room does not go unnaturally quiet.
		TimeToNextPlay = 0.75f;
	}
}
