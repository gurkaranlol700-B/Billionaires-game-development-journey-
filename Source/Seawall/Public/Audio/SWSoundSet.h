// SEAWALL -- one kind of sound, plus the small amount of randomness that keeps it alive.
//
// A footstep is not a sound file. It is eight recordings of a foot, picked at
// random with slight changes in level and pitch, so the ear never catches the
// loop. That behaviour is identical for steps, cloth, drips and door handles, so
// it lives here once and every system asks a sound set to play itself.
//
// This is a data asset on purpose: the dials are edited in the editor, by ear,
// without a recompile.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SWSoundSet.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

UCLASS(BlueprintType)
class SEAWALL_API USWSoundSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The takes. 8-12 is the usual target for footsteps; below ~5 the ear starts hearing a pattern. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound")
	TArray<TObjectPtr<USoundBase>> Sounds;

	/** Level for this set as a whole -- the first dial to reach for when something is too loud. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Volume = 1.f;

	/** Multiplied on top of Volume per play. Nothing in the real world repeats at exactly one level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound")
	FVector2D VolumeRange = FVector2D(0.92f, 1.08f);

	/** Per play, as a multiplier. Keep it small: past about 0.1 the material itself starts changing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound")
	FVector2D PitchRange = FVector2D(0.96f, 1.04f);

	/** How many recently used takes are barred from being picked again. 0 allows immediate repeats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound", meta = (ClampMin = "0", ClampMax = "8"))
	int32 NoRepeatDepth = 2;

	/** 0-1. Sweetener layers -- grit, a gear rattle -- should not fire on every single step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Probability = 1.f;

	/** How the sound fades with distance and muffles through walls. Null uses the engine default, which is far too big for foley. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Caps how many of these can sound at once, so a sprint cannot stack into mush. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Sound")
	TObjectPtr<USoundConcurrency> Concurrency;

	/** True when there is at least one sound and the set is not silenced. */
	UFUNCTION(BlueprintPure, Category = "Seawall|Sound")
	bool IsPlayable() const;

	/**
	 * Picks a take honouring NoRepeatDepth, and rolls Probability.
	 * Returns null when the roll fails -- callers treat that as "stay silent".
	 */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Sound")
	USoundBase* Pick();

	/** Plays in the world at Location. VolumeScale is how the caller expresses walk vs sprint. */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Sound", meta = (WorldContext = "WorldContextObject"))
	UAudioComponent* PlayAtLocation(const UObject* WorldContextObject, const FVector& Location,
		float VolumeScale = 1.f, float PitchScale = 1.f);

	/** Plays without a position -- for the player's own body, which is always exactly where the ears are. */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Sound", meta = (WorldContext = "WorldContextObject"))
	UAudioComponent* Play2D(const UObject* WorldContextObject, float VolumeScale = 1.f, float PitchScale = 1.f);

private:
	/** Which takes were used recently. Transient because it is play state, not content. */
	UPROPERTY(Transient)
	TArray<int32> RecentIndices;
};
