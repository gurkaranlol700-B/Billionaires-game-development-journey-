#include "Player/SWCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"

ASWCharacter::ASWCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 34cm radius, 184cm tall standing.
	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(50.f);
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->MaxAcceleration = MaxAcceleration;
	Move->BrakingDecelerationWalking = BrakingDeceleration;
	Move->GroundFriction = GroundFriction;
	Move->BrakingFrictionFactor = 1.f;
	Move->bUseControllerDesiredRotation = false;
	Move->bOrientRotationToMovement = false;
	// Almost no steering once airborne. Falling should feel like falling.
	Move->AirControl = AirControlDefault;
	Move->JumpZVelocity = JumpVelocityWalk;
	Move->JumpOffJumpZFactor = 0.f;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// The body hook. Empty on purpose -- see the header.
	FirstPersonBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonBody"));
	FirstPersonBody->SetupAttachment(GetCapsuleComponent());
	// Sit the body at the capsule's base. Derived rather than hard-coded so it
	// cannot silently drift out of sync if the capsule size ever changes.
	FirstPersonBody->SetRelativeLocation(
		FVector(0.f, 0.f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
	FirstPersonBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonBody->SetCastShadow(true);
	FirstPersonBody->bOnlyOwnerSee = true;

	// Camera hangs off the body component, not the capsule, so swapping in a real
	// mesh later is an assignment rather than a restructure.
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(FirstPersonBody);
	Camera->bUsePawnControlRotation = true;
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 174.f));
	Camera->SetFieldOfView(BaseFOV);

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetRelativeLocation(FVector(8.f, 6.f, -8.f));
	Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetIntensity(FlashlightIntensity);
	Flashlight->SetInnerConeAngle(FlashlightInnerCone);
	Flashlight->SetOuterConeAngle(FlashlightOuterCone);
	Flashlight->SetAttenuationRadius(FlashlightAttenuation);
	Flashlight->SetLightColor(FlashlightColor);
	Flashlight->SetCastShadows(true);

	CurrentEyeHeight = EyeHeightStanding;
}

void ASWCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ASWCharacter, CurrentStamina, COND_OwnerOnly);
	DOREPLIFETIME(ASWCharacter, bIsSprinting);
}

void ASWCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentStamina = MaxStamina;
	CurrentBattery = BatteryMax;
	CurrentEyeHeight = EyeHeightStanding;
	bFlashlightOn = bFlashlightStartsOn;
	Flashlight->SetVisibility(bFlashlightOn);

	UpdateMaxSpeed();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub =
				LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (InputMapping)
				{
					Sub->AddMappingContext(InputMapping, 0);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ASWCharacter::BuildFallbackInput()
{
	// Runs only when no input assets were assigned. This is what lets the
	// character work the moment it compiles, with no content to author first.
	if (bBuiltFallbackInput)
	{
		return;
	}
	bBuiltFallbackInput = true;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	if (!IA_Move)       { IA_Move       = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D); }
	if (!IA_Look)       { IA_Look       = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D); }
	if (!IA_Sprint)     { IA_Sprint     = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean); }
	if (!IA_Crouch)     { IA_Crouch     = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean); }
	if (!IA_Interact)   { IA_Interact   = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean); }
	if (!IA_Flashlight) { IA_Flashlight = MakeAction(TEXT("IA_Flashlight"), EInputActionValueType::Boolean); }
	if (!IA_LeanLeft)   { IA_LeanLeft   = MakeAction(TEXT("IA_LeanLeft"), EInputActionValueType::Boolean); }
	if (!IA_LeanRight)  { IA_LeanRight  = MakeAction(TEXT("IA_LeanRight"), EInputActionValueType::Boolean); }
	if (!IA_Jump)       { IA_Jump       = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean); }

	if (!InputMapping)
	{
		InputMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Seawall"));

		// WASD as a 2D axis: W/S drive Y (forward), A/D drive X (right).
		auto AddMoveKey = [this](const FKey& Key, bool bNegate, bool bSwizzleToY)
		{
			FEnhancedActionKeyMapping& M = InputMapping->MapKey(IA_Move, Key);
			if (bSwizzleToY)
			{
				UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(InputMapping);
				M.Modifiers.Add(Swizzle);
			}
			if (bNegate)
			{
				UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(InputMapping);
				M.Modifiers.Add(Negate);
			}
		};

		AddMoveKey(EKeys::W, false, true);
		AddMoveKey(EKeys::S, true,  true);
		AddMoveKey(EKeys::D, false, false);
		AddMoveKey(EKeys::A, true,  false);

		InputMapping->MapKey(IA_Look, EKeys::Mouse2D);

		InputMapping->MapKey(IA_Sprint, EKeys::LeftShift);
		InputMapping->MapKey(IA_Crouch, EKeys::LeftControl);
		InputMapping->MapKey(IA_Interact, EKeys::E);
		InputMapping->MapKey(IA_Flashlight, EKeys::F);
		InputMapping->MapKey(IA_Jump, EKeys::SpaceBar);

		// Lean on Q/Z, deliberately not Q/E -- E belongs to Interact. An earlier
		// Alt+A chord was dropped: it shared the A key with strafing, which is
		// exactly the kind of overlap that produces a bug you cannot reproduce.
		InputMapping->MapKey(IA_LeanLeft, EKeys::Q);
		InputMapping->MapKey(IA_LeanRight, EKeys::Z);
	}
}

void ASWCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	BuildFallbackInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Sub =
				LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Sub->ClearAllMappings();
				Sub->AddMappingContext(InputMapping, 0);
			}
		}
	}

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}

	EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASWCharacter::Input_Move);
	EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASWCharacter::Input_Look);
	EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &ASWCharacter::Input_SprintStart);
	EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASWCharacter::Input_SprintStop);
	EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ASWCharacter::Input_CrouchToggle);
	EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ASWCharacter::Input_Interact);
	EIC->BindAction(IA_Flashlight, ETriggerEvent::Started, this, &ASWCharacter::Input_FlashlightToggle);
	EIC->BindAction(IA_LeanLeft, ETriggerEvent::Triggered, this, &ASWCharacter::Input_LeanLeft);
	EIC->BindAction(IA_LeanLeft, ETriggerEvent::Completed, this, &ASWCharacter::Input_LeanLeft);
	EIC->BindAction(IA_LeanRight, ETriggerEvent::Triggered, this, &ASWCharacter::Input_LeanRight);
	EIC->BindAction(IA_LeanRight, ETriggerEvent::Completed, this, &ASWCharacter::Input_LeanRight);
	EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &ASWCharacter::Input_JumpStart);
	EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ASWCharacter::Input_JumpStop);
}

void ASWCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero() || !Controller)
	{
		return;
	}

	const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.X);
}

void ASWCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * LookSensitivity);
	AddControllerPitchInput(-Axis.Y * LookSensitivity);
}

void ASWCharacter::Input_SprintStart()
{
	if (CurrentStamina < MinStaminaToSprint || bIsCrouched)
	{
		return;
	}
	bIsSprinting = true;   // predict locally so it feels instant
	UpdateMaxSpeed();
	ServerSetSprinting(true);
}

void ASWCharacter::Input_SprintStop()
{
	bIsSprinting = false;
	UpdateMaxSpeed();
	ServerSetSprinting(false);
}

void ASWCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	// The server is the one that decides, and it re-checks the conditions.
	bIsSprinting = bNewSprinting && CurrentStamina >= MinStaminaToSprint && !bIsCrouched;
	UpdateMaxSpeed();
}

void ASWCharacter::Input_CrouchToggle()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		if (bIsSprinting)
		{
			Input_SprintStop();
		}
		Crouch();
	}
}

void ASWCharacter::Input_Interact()
{
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * InteractRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SWInteract), false, this);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(InteractRadius), Params);

	if (bHit)
	{
		OnInteractHit(Hit);
	}
}

void ASWCharacter::Input_FlashlightToggle()
{
	SetFlashlightEnabled(!bFlashlightOn);
}

void ASWCharacter::SetFlashlightEnabled(bool bEnabled)
{
	bFlashlightOn = bEnabled && (BatteryDrainPerSecond <= 0.f || CurrentBattery > 0.f);
	Flashlight->SetVisibility(bFlashlightOn);
}

void ASWCharacter::Input_LeanLeft(const FInputActionValue& Value)
{
	LeanTarget = Value.Get<bool>() ? -1.f : 0.f;
}

void ASWCharacter::Input_LeanRight(const FInputActionValue& Value)
{
	LeanTarget = Value.Get<bool>() ? 1.f : 0.f;
}

// ---------------------------------------------------------------------------
// Jump
//
// "Smooth" here is mostly forgiveness, not height. Three things do the work:
// coyote time (a jump pressed just after leaving a ledge still counts), input
// buffering (a jump pressed just before landing fires on touchdown instead of
// being swallowed), and a variable arc (release early to cut it short). Without
// them a jump feels like it ignores you roughly one press in five.
// ---------------------------------------------------------------------------

bool ASWCharacter::CanJumpInternal_Implementation() const
{
	if (Super::CanJumpInternal_Implementation())
	{
		return true;
	}

	// Coyote time: briefly after walking off an edge, still allow the jump.
	return !bIsCrouched
		&& GetCharacterMovement()->IsFalling()
		&& TimeSinceLeftGround <= CoyoteTime
		&& !bHasJumpedSinceGrounded;
}

void ASWCharacter::Input_JumpStart()
{
	// Crouched: stand up first, and remember the press so it fires as soon as
	// the capsule has room. Swallowing it would feel like a dropped input.
	if (bIsCrouched)
	{
		UnCrouch();
		JumpBufferTimer = JumpBufferTime;
		return;
	}

	if (!CanJump())
	{
		JumpBufferTimer = JumpBufferTime;
		return;
	}

	const bool bSprintJump = bIsSprinting && GetVelocity().Size2D() > WalkSpeed * 1.1f;
	const bool bMoving = GetVelocity().Size2D() > 10.f;

	float Velocity = JumpVelocityIdle;
	float Cost = JumpStaminaCost;
	if (bSprintJump)
	{
		Velocity = JumpVelocitySprint;
		Cost = JumpStaminaCostSprint;
	}
	else if (bMoving)
	{
		Velocity = JumpVelocityWalk;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->JumpZVelocity = Velocity;
	Move->AirControl = bSprintJump ? AirControlSprint : AirControlDefault;

	CurrentStamina = FMath::Max(0.f, CurrentStamina - Cost);
	TimeSinceStaminaSpend = 0.f;

	bHasJumpedSinceGrounded = true;
	JumpBufferTimer = 0.f;
	Jump();
}

void ASWCharacter::Input_JumpStop()
{
	StopJumping();

	// Variable height: cut upward velocity on release so a tap is a hop.
	if (bVariableJumpHeight)
	{
		FVector& Vel = GetCharacterMovement()->Velocity;
		if (Vel.Z > 0.f)
		{
			Vel.Z *= JumpCutMultiplier;
		}
	}
}

void ASWCharacter::UpdateJump(float DeltaSeconds)
{
	const bool bFalling = GetCharacterMovement()->IsFalling();

	if (bFalling)
	{
		TimeSinceLeftGround += DeltaSeconds;
	}
	else
	{
		TimeSinceLeftGround = 0.f;
		bHasJumpedSinceGrounded = false;
	}

	if (JumpBufferTimer > 0.f)
	{
		JumpBufferTimer -= DeltaSeconds;
		if (!bFalling && !bIsCrouched && CanJump())
		{
			JumpBufferTimer = 0.f;
			Input_JumpStart();
		}
	}
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void ASWCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateStamina(DeltaSeconds);
	UpdateJump(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
	UpdateCamera(DeltaSeconds);

	if (bFlashlightOn && BatteryDrainPerSecond > 0.f && HasAuthority())
	{
		CurrentBattery = FMath::Max(0.f, CurrentBattery - BatteryDrainPerSecond * DeltaSeconds);
		if (CurrentBattery <= 0.f)
		{
			SetFlashlightEnabled(false);
		}
	}
}

float ASWCharacter::GetStaminaFraction() const
{
	return MaxStamina > 0.f ? FMath::Clamp(CurrentStamina / MaxStamina, 0.f, 1.f) : 0.f;
}

void ASWCharacter::UpdateStamina(float DeltaSeconds)
{
	// Authority owns the number; clients predict it so the HUD does not stutter.
	const bool bActuallySprinting =
		bIsSprinting && GetVelocity().Size2D() > WalkSpeed * 1.1f && !GetCharacterMovement()->IsFalling();

	if (bActuallySprinting)
	{
		CurrentStamina = FMath::Max(0.f, CurrentStamina - SprintDrainRate * DeltaSeconds);
		TimeSinceStaminaSpend = 0.f;

		if (CurrentStamina <= 0.f)
		{
			// Out of breath: the sprint is taken away rather than politely faded.
			bIsSprinting = false;
			UpdateMaxSpeed();
			if (!HasAuthority())
			{
				ServerSetSprinting(false);
			}
		}
	}
	else
	{
		TimeSinceStaminaSpend += DeltaSeconds;
		if (TimeSinceStaminaSpend >= StaminaRegenDelay)
		{
			CurrentStamina = FMath::Min(MaxStamina, CurrentStamina + StaminaRegenRate * DeltaSeconds);
		}
	}
}

void ASWCharacter::UpdateMaxSpeed()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	Move->MaxWalkSpeedCrouched = CrouchSpeed;
	Move->MaxAcceleration = MaxAcceleration;
	Move->BrakingDecelerationWalking = BrakingDeceleration;
	Move->GroundFriction = GroundFriction;
}

void ASWCharacter::UpdateFootsteps(float DeltaSeconds)
{
	if (GetCharacterMovement()->IsFalling())
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();
	if (Speed < 10.f)
	{
		return;
	}

	// Distance-driven, not time-driven, so steps stay in sync at any speed.
	DistanceSinceStep += Speed * DeltaSeconds;
	BobPhase += (Speed * DeltaSeconds / FMath::Max(StepLength, 1.f)) * PI;

	if (DistanceSinceStep >= StepLength)
	{
		DistanceSinceStep -= StepLength;

		float Range = NoiseRangeWalk;
		if (bIsCrouched)          { Range = NoiseRangeCrouch; }
		else if (bIsSprinting)    { Range = NoiseRangeSprint; }

		EmitNoise(Range);
		OnFootstep(Range, bIsSprinting, bIsCrouched);
	}
}

void ASWCharacter::EmitNoise(float Range)
{
	if (Range <= 0.f)
	{
		return;
	}
	// Loudness is expressed as a fraction of the walk baseline so an AI can
	// compare "how loud" independently of "how far".
	const float Loudness = Range / FMath::Max(NoiseRangeWalk, 1.f);
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), Loudness, this, Range, TEXT("Footstep"));
}

void ASWCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	const float FallSpeed = FMath::Abs(GetCharacterMovement()->Velocity.Z);
	LandingDip = FMath::Min(FallSpeed * LandingDipScale, LandingDipMax);

	TimeSinceLeftGround = 0.f;
	bHasJumpedSinceGrounded = false;
	GetCharacterMovement()->AirControl = AirControlDefault;

	EmitNoise(NoiseRangeLand);
}

void ASWCharacter::UpdateCamera(float DeltaSeconds)
{
	// --- eye height (standing <-> crouched) --------------------------------
	const float TargetEye = bIsCrouched ? EyeHeightCrouched : EyeHeightStanding;
	CurrentEyeHeight = FMath::FInterpTo(CurrentEyeHeight, TargetEye, DeltaSeconds, EyeHeightInterpSpeed);

	// --- head bob ----------------------------------------------------------
	FVector TargetBob = FVector::ZeroVector;
	if (bEnableHeadBob && !GetCharacterMovement()->IsFalling())
	{
		const float Speed = GetVelocity().Size2D();
		const float SpeedAlpha = FMath::Clamp(Speed / FMath::Max(WalkSpeed, 1.f), 0.f, 3.f);
		if (SpeedAlpha > 0.05f)
		{
			// Vertical runs at double frequency: two dips per stride, one per foot.
			TargetBob.Z = FMath::Sin(BobPhase * 2.f) * BobVerticalAmplitude * SpeedAlpha;
			TargetBob.Y = FMath::Sin(BobPhase) * BobLateralAmplitude * SpeedAlpha;
		}
	}
	CurrentBobOffset = FMath::VInterpTo(CurrentBobOffset, TargetBob, DeltaSeconds, BobInterpSpeed);

	// --- breathing ---------------------------------------------------------
	// Amplitude scales with how spent you are. At full stamina it is nearly
	// invisible; empty, the view swims.
	BreathPhase += DeltaSeconds * BreathFrequency * PI * 2.f;
	const float Exhaustion = 1.f - GetStaminaFraction();
	const float BreathAmp = BreathAmplitude * (1.f + Exhaustion * BreathExhaustionMultiplier);
	const float BreathZ = FMath::Sin(BreathPhase) * BreathAmp;
	const float BreathRoll = FMath::Sin(BreathPhase * 0.5f) * BreathAmp * 0.25f;

	// --- landing dip -------------------------------------------------------
	LandingDip = FMath::FInterpTo(LandingDip, 0.f, DeltaSeconds, LandingDipRecoverySpeed);

	// --- lean --------------------------------------------------------------
	// Trace before committing so you cannot lean your head through concrete.
	float AllowedLean = LeanTarget;
	if (!FMath::IsNearlyZero(LeanTarget))
	{
		const FVector Origin = Camera->GetComponentLocation();
		const FVector Dir = Camera->GetRightVector() * FMath::Sign(LeanTarget);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SWLean), false, this);
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Origin, Origin + Dir * (LeanOffset + 12.f),
			FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
		{
			AllowedLean = LeanTarget * FMath::Clamp(Hit.Distance / (LeanOffset + 12.f), 0.f, 1.f);
		}
	}
	CurrentLean = FMath::FInterpTo(CurrentLean, AllowedLean, DeltaSeconds, LeanInterpSpeed);

	// --- compose -----------------------------------------------------------
	// Crouching shrinks the capsule and drops the actor so the feet stay planted,
	// but FirstPersonBody keeps a fixed offset sized for standing. Without this
	// compensation the crouched eye ends up far below where EyeHeightCrouched asks
	// for. Adding back the half-height difference keeps both heights measured from
	// the floor, which is what they claim to be.
	const float HalfHeightCompensation =
		GetDefaultHalfHeight() - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	const FVector Offset(
		CurrentBobOffset.X,
		CurrentBobOffset.Y + CurrentLean * LeanOffset,
		CurrentEyeHeight + HalfHeightCompensation + CurrentBobOffset.Z + BreathZ - LandingDip);

	Camera->SetRelativeLocation(Offset);
	Camera->SetRelativeRotation(FRotator(0.f, 0.f, CurrentLean * LeanAngle + BreathRoll));

	// --- FOV ---------------------------------------------------------------
	// The FOV push is most of what actually sells a sprint.
	const bool bSprintingNow = bIsSprinting && GetVelocity().Size2D() > WalkSpeed * 1.1f;
	const float TargetFOV = bSprintingNow ? SprintFOV : BaseFOV;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaSeconds, FOVInterpSpeed));
}
