// SEAWALL -- what each surface sounds like under the player.
//
// The floor decides the sound. Concrete, the grit and rubble on top of it, a
// puddle, a metal grating and broken glass are five different instruments, and
// each of them is played differently depending on whether you are creeping,
// walking or running for your life.
//
// A surface is identified by the Physical Material on whatever the foot trace
// hit, which is the same data the rest of the game uses for impacts later.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "SWFootstepAudioData.generated.h"

class USWSoundSet;

/** Every sound one surface can make under the player. Any slot may be left empty. */
USTRUCT(BlueprintType)
struct FSWSurfaceFootsteps
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> Walk;

	/** Heavier, more heel, more grit. Not just the walk take played louder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> Sprint;

	/** Slow roll of the foot, almost no transient -- the sound of trying not to be heard. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> Crouch;

	/** Played when stopping hard or turning sharply: the foot dragging rather than landing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> Scuff;

	/** The push-off. Without it a jump starts in silence and feels weightless. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> JumpTakeoff;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> LandSoft;

	/** A drop that hurts: shoe, body weight and debris, usually with a synth thud layered under it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> LandHard;

	/** Optional extra layer rolled per step -- loose grit, a puddle tick, glass shifting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> Sweetener;

	/**
	 * Layered UNDER a hard landing, not chosen instead of it: the shoe is what you
	 * hear, the body is what you feel. Two sounds at once is the whole difference
	 * between a drop that lands and a drop that clicks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TObjectPtr<USWSoundSet> BodyImpact;
};

UCLASS(BlueprintType)
class SEAWALL_API USWFootstepAudioData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Used when a surface has no entry, or when the trace hits something with no Physical Material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	FSWSurfaceFootsteps Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Seawall|Footsteps")
	TMap<TEnumAsByte<EPhysicalSurface>, FSWSurfaceFootsteps> Surfaces;

	/** Never fails: falls back to Default so a missing surface is quiet-but-correct, not silent. */
	const FSWSurfaceFootsteps& Resolve(EPhysicalSurface Surface) const;
};
