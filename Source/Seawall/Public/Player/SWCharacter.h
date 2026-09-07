// SEAWALL -- first-person player character.
//
// Design note: in first person the sense of *weight* comes from the camera and
// the movement ramps, not from a skeleton. There is deliberately no visible
// body yet. FirstPersonBody exists as an empty hook so that attaching a real
// MetaHuman at M3 is a mesh assignment rather than a restructure -- the camera
// already hangs off that component instead of being welded to the capsule.
//
// Everything is server-authoritative from the first line. Single player is a
// one-player listen server, so co-op later costs almost nothing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "SWCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UInputAction;
class UInputMappingContext;

UCLASS(config = Game, BlueprintType, Blueprintable)
class SEAWALL_API ASWCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASWCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- components -------------------------------------------------------

	/** Empty by design. The slot a MetaHuman drops into at M3. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seawall|Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seawall|Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seawall|Components")
	TObjectPtr<USpotLightComponent> Flashlight;

	// ---- camera -----------------------------------------------------------

	/** Eye height above the capsule base while standing. 165 = slightly short, deliberately: it makes the ceiling press down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera", meta = (ClampMin = "80.0", ClampMax = "200.0"))
	float EyeHeightStanding = 165.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera", meta = (ClampMin = "40.0", ClampMax = "160.0"))
	float EyeHeightCrouched = 92.f;

	/** How fast the eye slides between standing and crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera")
	float EyeHeightInterpSpeed = 9.f;

	/** Narrow FOV restricts peripheral vision, so things enter frame suddenly. The cheapest scare multiplier there is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera", meta = (ClampMin = "50.0", ClampMax = "120.0"))
	float BaseFOV = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera", meta = (ClampMin = "50.0", ClampMax = "130.0"))
	float SprintFOV = 81.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera")
	float FOVInterpSpeed = 4.f;

	/**
	 * When a real body mesh is assigned and has this socket, the camera follows it
	 * with damping instead of using EyeHeightStanding. Damping matters: raw head-bone
	 * motion piped straight into a first-person camera is what makes players sick.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera")
	FName HeadSocketName = TEXT("head");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Camera")
	float HeadSocketDamping = 12.f;

	// ---- movement ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Movement")
	float WalkSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Movement")
	float CrouchSpeed = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Movement")
	float SprintSpeed = 420.f;

	/** THE weight number. Low acceleration means you lean into motion instead of teleporting into it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Movement")
	float MaxAcceleration = 900.f;

	/** You slide a few centimetres past where you released. Nothing in a body stops dead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Movement")
	float BrakingDeceleration = 1100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Movement")
	float GroundFriction = 6.f;

	// ---- stamina ----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Stamina")
	float MaxStamina = 100.f;

	/** 25/s against 100 max gives roughly four seconds of sprint. Long enough to escape, short enough to hurt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Stamina")
	float SprintDrainRate = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Stamina")
	float StaminaRegenRate = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Stamina")
	float StaminaRegenDelay = 1.5f;

	/** Below this you cannot start a sprint -- so emptying the bar has a real cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Stamina")
	float MinStaminaToSprint = 20.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Seawall|Stamina")
	float CurrentStamina = 100.f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Seawall|Stamina")
	bool bIsSprinting = false;

	UFUNCTION(BlueprintPure, Category = "Seawall|Stamina")
	float GetStaminaFraction() const;

	// ---- head bob ---------------------------------------------------------

	/** Master switch. Turn it off and walk the corridor to A/B whether the bob is helping or just making you queasy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|HeadBob")
	bool bEnableHeadBob = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|HeadBob")
	float BobVerticalAmplitude = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|HeadBob")
	float BobLateralAmplitude = 0.8f;

	/** Bob is driven by distance travelled, not by time, so it stays in step at every speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|HeadBob")
	float StepLength = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|HeadBob")
	float BobInterpSpeed = 10.f;

	// ---- landing ----------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Landing")
	float LandingDipScale = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Landing")
	float LandingDipMax = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Landing")
	float LandingDipRecoverySpeed = 7.f;

	// ---- breathing --------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Breathing")
	float BreathAmplitude = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Breathing")
	float BreathFrequency = 0.9f;

	/** How much worse the sway gets at zero stamina. This is the player hearing their own body panic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Breathing")
	float BreathExhaustionMultiplier = 4.f;

	// ---- lean -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Lean")
	float LeanAngle = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Lean")
	float LeanOffset = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Lean")
	float LeanInterpSpeed = 8.f;

	// ---- flashlight -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	bool bFlashlightStartsOn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	float FlashlightIntensity = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	float FlashlightInnerCone = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	float FlashlightOuterCone = 44.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	float FlashlightAttenuation = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	FLinearColor FlashlightColor = FLinearColor(1.f, 0.87f, 0.72f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	float BatteryMax = 100.f;

	/** Wired but zero. Making the flashlight a resource later is one number, not a feature. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Flashlight")
	float BatteryDrainPerSecond = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Seawall|Flashlight")
	float CurrentBattery = 100.f;

	UFUNCTION(BlueprintCallable, Category = "Seawall|Flashlight")
	void SetFlashlightEnabled(bool bEnabled);

	// ---- noise ------------------------------------------------------------
	// Nothing listens yet. The Watchman at M2 subscribes to exactly this, and
	// retrofitting noise into every movement path later would be miserable.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Noise")
	float NoiseRangeWalk = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Noise")
	float NoiseRangeSprint = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Noise")
	float NoiseRangeCrouch = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Noise")
	float NoiseRangeLand = 900.f;

	/** Fired on every footstep so audio and AI can both hang off one event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Seawall|Noise")
	void OnFootstep(float NoiseRange, bool bSprinting, bool bCrouched);

	// ---- interaction ------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Interaction")
	float InteractRange = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Interaction")
	float InteractRadius = 8.f;

	UFUNCTION(BlueprintImplementableEvent, Category = "Seawall|Interaction")
	void OnInteractHit(const FHitResult& Hit);

	// ---- input assets (optional) -----------------------------------------
	// Leave these empty and the character builds its own bindings in code, so it
	// runs with zero assets. Assign real assets later to make them rebindable.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_Crouch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_Interact;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_Flashlight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_LeanLeft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	TObjectPtr<UInputAction> IA_LeanRight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Input")
	float LookSensitivity = 0.6f;

protected:
	// input handlers
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_SprintStart();
	void Input_SprintStop();
	void Input_CrouchToggle();
	void Input_Interact();
	void Input_FlashlightToggle();
	void Input_LeanLeft(const FInputActionValue& Value);
	void Input_LeanRight(const FInputActionValue& Value);

	/** Sprint is a request the server grants, so a client cannot simply declare itself fast. */
	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	void UpdateStamina(float DeltaSeconds);
	void UpdateMaxSpeed();
	void UpdateCamera(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);
	void EmitNoise(float Range);
	void BuildFallbackInput();

private:
	float BobPhase = 0.f;
	float DistanceSinceStep = 0.f;
	float CurrentEyeHeight = 165.f;
	float LandingDip = 0.f;
	float BreathPhase = 0.f;
	float TimeSinceStaminaSpend = 0.f;
	float LeanTarget = 0.f;
	float CurrentLean = 0.f;
	FVector CurrentBobOffset = FVector::ZeroVector;
	bool bFlashlightOn = true;
	bool bBuiltFallbackInput = false;
};
