// SEAWALL -- every audio dial in one place: Project Settings > Game > Seawall Audio.
//
// The point of this class is that tuning horror audio is done by ear, over and
// over, and nothing here should require a recompile or hunting through actors.
// Values are saved into DefaultGame.ini, so they travel with the project.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SWAudioSettings.generated.h"

class USoundSubmix;
class USWFootstepAudioData;
class USWSoundSet;

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Seawall Audio"))
class SEAWALL_API USWAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---- content -----------------------------------------------------------
	// Soft references: the audio system loads them when the level starts, and a
	// missing one disables just that feature instead of breaking the game.

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWFootstepAudioData> FootstepData;

	/** Rustle on crouch, lean and fast turns. */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> ClothSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> FlashlightClickSet;

	/** Calm, exerted, exhausted, frightened -- the four breath sets, in that order. */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> BreathCalmSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> BreathExertedSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> BreathExhaustedSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> BreathFearSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> HeartbeatCalmSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> HeartbeatRaisedSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> HeartbeatPoundingSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> TinnitusSet;

	/** Looping beds faded in by fear: the sub drone and the dissonant cluster. */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> TensionSubDroneSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> TensionClusterSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> StingerSet;

	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USWSoundSet> RiserSet;

	/** Optional. When set, the world gets ducked and low-passed as fear rises. */
	UPROPERTY(config, EditAnywhere, Category = "Content")
	TSoftObjectPtr<USoundSubmix> WorldSubmix;

	// ---- footsteps ---------------------------------------------------------

	UPROPERTY(config, EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FootstepVolumeWalk = 0.55f;

	UPROPERTY(config, EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FootstepVolumeSprint = 1.f;

	UPROPERTY(config, EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float FootstepVolumeCrouch = 0.18f;

	/** Below this landing speed (cm/s) the landing is soft; above it, hard. */
	UPROPERTY(config, EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0.0"))
	float HardLandingSpeed = 550.f;

	/** Degrees per second of view rotation that counts as a sharp turn and rustles cloth. */
	UPROPERTY(config, EditAnywhere, Category = "Footsteps", meta = (ClampMin = "0.0"))
	float TurnRustleSpeed = 140.f;

	/** How far down the foot trace looks for a surface, from the capsule bottom. */
	UPROPERTY(config, EditAnywhere, Category = "Footsteps", meta = (ClampMin = "10.0"))
	float SurfaceTraceDistance = 120.f;

	// ---- body --------------------------------------------------------------

	/** Heart rate at zero fear and full stamina, and at full fear -- interpolated between. */
	UPROPERTY(config, EditAnywhere, Category = "Body")
	FVector2D HeartbeatBPMRange = FVector2D(62.f, 148.f);

	/** The heart is inaudible when calm and only creeps in with fear; this is its ceiling. */
	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float HeartbeatVolume = 0.85f;

	/** Fear below this and the heartbeat stays silent. */
	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeartbeatFearThreshold = 0.15f;

	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BreathVolume = 0.7f;

	/** Stamina fraction below which breathing becomes exerted, then exhausted. */
	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BreathExertedStamina = 0.6f;

	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BreathExhaustedStamina = 0.25f;

	/** Seconds between breaths, at rest and when spent. */
	UPROPERTY(config, EditAnywhere, Category = "Body")
	FVector2D BreathIntervalRange = FVector2D(4.2f, 1.1f);

	/**
	 * Exertion is how hard the body is working, 0-1. It is not the stamina bar:
	 * stamina refills in a few seconds, but a heart that has been sprinting stays
	 * up for much longer. This is what the player actually hears.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0"))
	float ExertionRisePerSecond = 0.30f;

	/** Recovery is deliberately slower than the climb -- the pound outlasts the run. */
	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0"))
	float ExertionDecayPerSecond = 0.07f;

	/**
	 * Added the moment a sprint ends. A real heart rate peaks a beat AFTER you stop,
	 * which is exactly why slowing down feels worse than running -- you were busy
	 * running, and now there is nothing to do but listen to yourself.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExertionSprintStopBump = 0.18f;

	// ---- fear --------------------------------------------------------------

	/** How fast a fear spike bleeds away, per second. Lower = dread lingers. */
	UPROPERTY(config, EditAnywhere, Category = "Fear", meta = (ClampMin = "0.0"))
	float FearDecayPerSecond = 0.12f;

	/** Walking in the dark is frightening on its own. Added while the flashlight is off. */
	UPROPERTY(config, EditAnywhere, Category = "Fear", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlashlightOffFear = 0.15f;

	/** A threat this close (cm) contributes full fear; at twice this, none. */
	UPROPERTY(config, EditAnywhere, Category = "Fear", meta = (ClampMin = "100.0"))
	float ThreatFearRadius = 1500.f;

	UPROPERTY(config, EditAnywhere, Category = "Fear")
	FVector2D TensionSubDroneVolume = FVector2D(0.f, 0.9f);

	UPROPERTY(config, EditAnywhere, Category = "Fear")
	FVector2D TensionClusterVolume = FVector2D(0.f, 0.75f);

	/** Cluster stays out until fear passes this -- it is the "something is here" layer, not wallpaper. */
	UPROPERTY(config, EditAnywhere, Category = "Fear", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TensionClusterFearThreshold = 0.45f;

	/** World low-pass cutoff (Hz) at no fear and at full fear: the tunnel-hearing effect. */
	UPROPERTY(config, EditAnywhere, Category = "Fear")
	FVector2D WorldLowpassRange = FVector2D(20000.f, 900.f);

	/** World submix level at no fear and at full fear -- the world recedes as panic rises. */
	UPROPERTY(config, EditAnywhere, Category = "Fear")
	FVector2D WorldDuckRange = FVector2D(1.f, 0.72f);

	// ---- scares ------------------------------------------------------------

	/** Hard scares closer together than this are skipped, however many triggers fire. */
	UPROPERTY(config, EditAnywhere, Category = "Scares", meta = (ClampMin = "0.0"))
	float ScareCooldownSeconds = 60.f;

	/** How long the world is pulled down to near-silence before a hit lands. */
	UPROPERTY(config, EditAnywhere, Category = "Scares", meta = (ClampMin = "0.0"))
	float SilenceDropSeconds = 1.5f;

	UPROPERTY(config, EditAnywhere, Category = "Scares", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SilenceDropLevel = 0.06f;

	/** Fear added by a stinger hit. */
	UPROPERTY(config, EditAnywhere, Category = "Scares", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StingerFearSpike = 0.8f;

	/** Delay before a follower step echoes the player's own, and how far behind it sounds. */
	UPROPERTY(config, EditAnywhere, Category = "Scares")
	FVector2D FollowerDelayRange = FVector2D(0.15f, 0.3f);

	UPROPERTY(config, EditAnywhere, Category = "Scares", meta = (ClampMin = "0.0"))
	float FollowerDistance = 600.f;

	/** The follower keeps walking for this long after the player stops -- the part that lands. */
	UPROPERTY(config, EditAnywhere, Category = "Scares", meta = (ClampMin = "0.0"))
	float FollowerOvershootSeconds = 0.55f;

	static const USWAudioSettings& Get();
};
