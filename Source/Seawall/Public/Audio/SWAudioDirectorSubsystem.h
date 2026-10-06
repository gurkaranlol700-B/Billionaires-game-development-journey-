// SEAWALL -- the part of the mix that knows how frightened you are.
//
// Horror audio is not a set of sounds, it is a state. One number, Fear, is fed
// by the game (scripted moments, the dark, and from M2 the Watchman's distance)
// and everything else hangs off it: how hard your heart beats, how you breathe,
// how much low drone sits under the room, how much of the world you can still
// hear. The player never sees the number -- they just notice their own body
// reacting before they have seen anything.
//
// It is a world subsystem because there is exactly one per level, it needs to
// tick, and anything can reach it without a reference being wired up by hand.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SWAudioDirectorSubsystem.generated.h"

class UAudioComponent;
class USWSoundSet;

UENUM(BlueprintType)
enum class ESWScareRecipe : uint8
{
	/** Pull the world down to near silence and let go. Often nothing follows -- that is the point. */
	SilenceDrop,
	/** Silence, then the hit, then ringing ears and a pounding heart. */
	Stinger,
	/** Your own footsteps, replayed a beat late from behind you, still walking after you stop. */
	FollowerSteps,
	/** Something heavy falls somewhere else in the building, muffled through the walls. */
	DistantSlam,
	/** The failing fixture surges, pops, and takes the light with it. */
	LightSurge
};

UCLASS()
class SEAWALL_API USWAudioDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---- fear --------------------------------------------------------------

	/** 0 = safe, 1 = terrified. Read by the player's body audio every frame. */
	UFUNCTION(BlueprintPure, Category = "Seawall|Audio")
	float GetFear() const { return Fear; }

	/** A sudden shock that decays away. Scares use this. */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void AddFearSpike(float Amount);

	/** A floor that stays until changed -- "this whole corridor is bad". */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void SetFearFloor(float Floor);

	/**
	 * The M2 hook. The Watchman calls this every tick it is hunting; fear rises as
	 * it closes in, with no scripting anywhere. Intensity scales the contribution.
	 */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void ReportThreat(const FVector& ThreatLocation, float Intensity = 1.f);

	// ---- events ------------------------------------------------------------

	/** Told by the player audio component on every step, so the follower can echo them. */
	void NotifyPlayerFootstep(const FVector& Location, bool bSprinting, bool bCrouched);

	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void PlayScare(ESWScareRecipe Recipe, const FVector& Location);

	/** True while a scare is allowed -- respects the cooldown that stops scares stacking. */
	UFUNCTION(BlueprintPure, Category = "Seawall|Audio")
	bool CanScare() const;

	// ---- UWorldSubsystem ---------------------------------------------------

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void UpdateFear(float DeltaTime);
	void UpdateTensionBeds(float DeltaTime);
	void UpdateWorldDuck(float DeltaTime);
	void UpdatePendingEvents(float DeltaTime);

	/** Spawns a 2D looping bed, started silent. Restarted by Tick if the wave is not flagged looping. */
	UAudioComponent* SpawnBed(USWSoundSet* Set);

	/** The player pawn's location, or zero. Used to place follower steps behind them. */
	FVector GetListenerLocation() const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SubDroneBed;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ClusterBed;

	/** One delayed sound waiting to fire: the follower's step, or the hit after the silence. */
	struct FPendingSound
	{
		float TimeRemaining = 0.f;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<USWSoundSet> Set;
		float VolumeScale = 1.f;
		bool b2D = false;
	};
	TArray<FPendingSound> PendingSounds;

	float Fear = 0.f;
	float FearFloor = 0.f;
	float FearSpike = 0.f;
	float ThreatFear = 0.f;

	/** Counts down while the world is held quiet before a hit. */
	float SilenceTimer = 0.f;
	float SilenceDuration = 0.f;

	/** Starts high so the first scare of a level is never blocked by the cooldown. */
	float TimeSinceLastScare = 1000.f;

	/** Follower state: how long it keeps walking, and the delay it lags the player by. */
	float FollowerTimeRemaining = 0.f;
	float FollowerDelay = 0.2f;

	/** What we last pushed to the submix, so we only talk to the mixer when it changes. */
	float LastWorldVolume = -1.f;
};
