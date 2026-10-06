// SEAWALL -- the sounds that make a dead building feel occupied.
//
// A drip that falls every 3.0 seconds at the same pitch from the same spot stops
// being heard within ten seconds; the brain files it as wallpaper. The same drip
// at 2.4s, then 5.1s, then 3.3s, each from a slightly different place at a
// slightly different pitch, never gets filed away -- so the room stays alive and
// the player keeps listening. That is the whole trick, and it is why this actor
// randomises time, place, pitch and level rather than looping a file.
//
// Drop one in a room, give it a sound set, and tune the two dials by walking around.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SWRandomEmitter.generated.h"

class USWSoundSet;

UCLASS()
class SEAWALL_API ASWRandomEmitter : public AActor
{
	GENERATED_BODY()

public:
	ASWRandomEmitter();

	virtual void Tick(float DeltaSeconds) override;

	/** What plays. Drips, pipe knocks, structural groans, distant debris. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter")
	TObjectPtr<USWSoundSet> Sounds;

	/** Seconds between plays, picked fresh each time. Wide ranges feel more alive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter")
	FVector2D IntervalRange = FVector2D(4.f, 14.f);

	/** Each play happens somewhere inside this radius (cm), not at the actor itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter", meta = (ClampMin = "0.0"))
	float ScatterRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeScale = 1.f;

	/** Silent when the player is further than this (cm). Keeps distant rooms off the voice count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter", meta = (ClampMin = "0.0"))
	float MaxPlayerDistance = 4000.f;

	/**
	 * Only play when the spot is outside the player's view. A noise from something
	 * you cannot see is frightening; the same noise from a visible empty corner is
	 * just a sound effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter")
	bool bOnlyOutOfView = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Emitter")
	bool bActive = true;

protected:
	virtual void BeginPlay() override;

	/** A point inside ScatterRadius, honouring bOnlyOutOfView. Returns false if none is suitable. */
	bool PickLocation(FVector& OutLocation) const;

	void ResetTimer();

private:
	float TimeToNextPlay = 0.f;
};
