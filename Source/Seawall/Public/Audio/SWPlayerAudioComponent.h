// SEAWALL -- everything the player's own body sounds like.
//
// In first person you never see yourself, so your feet, your clothes, your
// lungs and your heart are the only evidence that you have a body at all. That
// makes this the most important audio in the game: it is the sound the player
// hears most, and the one that tells them how much trouble they are in.
//
// The component does the listening for itself -- speed, surface, stamina, fear --
// so the character class only has to say "a step happened" and stay out of the way.

#pragma once

#include "Chaos/ChaosEngineInterface.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "SWPlayerAudioComponent.generated.h"

class ACharacter;
class USWFootstepAudioData;
class USWSoundSet;
struct FSWSurfaceFootsteps;

UCLASS(ClassGroup = (Seawall), meta = (BlueprintSpawnableComponent))
class SEAWALL_API USWPlayerAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USWPlayerAudioComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** One stride has landed. Called from the character's distance-based step timing. */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void HandleFootstep(bool bSprinting, bool bCrouched);

	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void HandleJump(bool bSprintJump);

	/** FallSpeed is the downward speed at touchdown, which decides soft versus hard. */
	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void HandleLanded(float FallSpeed);

	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void HandleFlashlight(bool bOn);

	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void HandleCrouch(bool bCrouched);

	UFUNCTION(BlueprintCallable, Category = "Seawall|Audio")
	void HandleLean(float Direction);

	/** The surface currently under the player. Impacts and the AI will want this too. */
	UFUNCTION(BlueprintPure, Category = "Seawall|Audio")
	EPhysicalSurface GetCurrentSurface() const { return CurrentSurface; }

protected:
	/** Traces down for the physical material, letting surface volumes override it. */
	EPhysicalSurface TraceSurface(FVector& OutFootLocation);

	const FSWSurfaceFootsteps* ResolveSurfaceSounds(EPhysicalSurface Surface) const;

	/** Builds while sprinting, lingers afterwards. See USWAudioSettings for the dials. */
	void UpdateExertion(float DeltaTime);

	void UpdateBreath(float DeltaTime);
	void UpdateHeartbeat(float DeltaTime);
	void UpdateTurnRustle(float DeltaTime);
	void UpdateScuff(float DeltaTime);

	/** 0 = fine, 1 = terrified. Read from the director; 0 when there is none. */
	float GetFear() const;

	/** 0 = spent, 1 = fresh. Read from the character's stamina. */
	float GetStaminaFraction() const;

public:
	/** 0 = resting, 1 = heart hammering. Drives heartbeat rate and breathing. */
	UFUNCTION(BlueprintPure, Category = "Seawall|Audio")
	float GetExertion() const { return Exertion; }

protected:

private:
	UPROPERTY(Transient) TObjectPtr<USWFootstepAudioData> FootstepData;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> ClothSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> FlashlightClickSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> BreathCalmSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> BreathExertedSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> BreathExhaustedSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> BreathFearSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> HeartbeatCalmSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> HeartbeatRaisedSet;
	UPROPERTY(Transient) TObjectPtr<USWSoundSet> HeartbeatPoundingSet;

	TWeakObjectPtr<ACharacter> OwningCharacter;

	EPhysicalSurface CurrentSurface = SurfaceType_Default;

	float BreathTimer = 2.f;
	float HeartTimer = 0.f;
	float TurnRustleCooldown = 0.f;
	float PreviousYaw = 0.f;
	float PreviousSpeed = 0.f;
	float ScuffCooldown = 0.f;
	float Exertion = 0.f;
	bool bWasSprinting = false;
};
