#include "Audio/SWPlayerAudioComponent.h"

#include "Audio/SWAudioDirectorSubsystem.h"
#include "Audio/SWAudioSettings.h"
#include "Audio/SWFootstepAudioData.h"
#include "Audio/SWSoundSet.h"
#include "Audio/SWSurfaceVolume.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/SWCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSWAudio, Log, All);

USWPlayerAudioComponent::USWPlayerAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USWPlayerAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());

	// Load the content named in Project Settings > Game > Seawall Audio. Anything
	// left unset simply stays silent, so the game runs from the first compile.
	const USWAudioSettings& Settings = USWAudioSettings::Get();
	FootstepData          = Settings.FootstepData.LoadSynchronous();
	ClothSet              = Settings.ClothSet.LoadSynchronous();
	FlashlightClickSet    = Settings.FlashlightClickSet.LoadSynchronous();
	BreathCalmSet         = Settings.BreathCalmSet.LoadSynchronous();
	BreathExertedSet      = Settings.BreathExertedSet.LoadSynchronous();
	BreathExhaustedSet    = Settings.BreathExhaustedSet.LoadSynchronous();
	BreathFearSet         = Settings.BreathFearSet.LoadSynchronous();
	HeartbeatCalmSet      = Settings.HeartbeatCalmSet.LoadSynchronous();
	HeartbeatRaisedSet    = Settings.HeartbeatRaisedSet.LoadSynchronous();
	HeartbeatPoundingSet  = Settings.HeartbeatPoundingSet.LoadSynchronous();

	if (const AActor* Owner = GetOwner())
	{
		PreviousYaw = Owner->GetActorRotation().Yaw;
	}

	UE_LOG(LogSWAudio, Log, TEXT("Player audio ready (footstep data: %s)"),
		FootstepData ? *FootstepData->GetName() : TEXT("none"));
}

void USWPlayerAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TurnRustleCooldown = FMath::Max(0.f, TurnRustleCooldown - DeltaTime);
	ScuffCooldown = FMath::Max(0.f, ScuffCooldown - DeltaTime);

	UpdateExertion(DeltaTime);
	UpdateBreath(DeltaTime);
	UpdateHeartbeat(DeltaTime);
	UpdateTurnRustle(DeltaTime);
	UpdateScuff(DeltaTime);
}

// ---------------------------------------------------------------------------
// Surface
// ---------------------------------------------------------------------------

EPhysicalSurface USWPlayerAudioComponent::TraceSurface(FVector& OutFootLocation)
{
	const ACharacter* Character = OwningCharacter.Get();
	if (!Character)
	{
		OutFootLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		return SurfaceType_Default;
	}

	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	OutFootLocation = Feet;

	// A box or puddle marked in the level beats whatever material is underneath it.
	EPhysicalSurface Override = SurfaceType_Default;
	if (ASWSurfaceVolume::FindOverrideAt(GetWorld(), Feet, Override))
	{
		CurrentSurface = Override;
		return Override;
	}

	const USWAudioSettings& Settings = USWAudioSettings::Get();
	const FVector Start = Feet + FVector(0.f, 0.f, 20.f);
	const FVector End = Feet - FVector(0.f, 0.f, Settings.SurfaceTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SWFootstepSurface), /*bTraceComplex*/ true, Character);
	Params.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutFootLocation = Hit.ImpactPoint;
		CurrentSurface = UGameplayStatics::GetSurfaceType(Hit);
	}
	else
	{
		CurrentSurface = SurfaceType_Default;
	}

	return CurrentSurface;
}

const FSWSurfaceFootsteps* USWPlayerAudioComponent::ResolveSurfaceSounds(EPhysicalSurface Surface) const
{
	return FootstepData ? &FootstepData->Resolve(Surface) : nullptr;
}

// ---------------------------------------------------------------------------
// Events from the character
// ---------------------------------------------------------------------------

void USWPlayerAudioComponent::HandleFootstep(bool bSprinting, bool bCrouched)
{
	FVector Foot;
	const EPhysicalSurface Surface = TraceSurface(Foot);
	const FSWSurfaceFootsteps* Sounds = ResolveSurfaceSounds(Surface);
	if (!Sounds)
	{
		return;
	}

	const USWAudioSettings& Settings = USWAudioSettings::Get();

	USWSoundSet* Set = Sounds->Walk;
	float Volume = Settings.FootstepVolumeWalk;
	if (bCrouched)
	{
		Set = Sounds->Crouch ? Sounds->Crouch : Sounds->Walk;
		Volume = Settings.FootstepVolumeCrouch;
	}
	else if (bSprinting)
	{
		Set = Sounds->Sprint ? Sounds->Sprint : Sounds->Walk;
		Volume = Settings.FootstepVolumeSprint;
	}

	if (Set)
	{
		Set->PlayAtLocation(this, Foot, Volume);
	}

	// The loose-grit layer, rolled per step so it never becomes part of the rhythm.
	if (Sounds->Sweetener)
	{
		Sounds->Sweetener->PlayAtLocation(this, Foot, Volume);
	}

	if (UWorld* World = GetWorld())
	{
		if (USWAudioDirectorSubsystem* Director = World->GetSubsystem<USWAudioDirectorSubsystem>())
		{
			Director->NotifyPlayerFootstep(Foot, bSprinting, bCrouched);
		}
	}
}

void USWPlayerAudioComponent::HandleJump(bool bSprintJump)
{
	FVector Foot;
	const FSWSurfaceFootsteps* Sounds = ResolveSurfaceSounds(TraceSurface(Foot));
	if (Sounds && Sounds->JumpTakeoff)
	{
		Sounds->JumpTakeoff->PlayAtLocation(this, Foot, bSprintJump ? 1.f : 0.8f);
	}
	if (ClothSet)
	{
		ClothSet->Play2D(this, 0.5f);
	}
}

void USWPlayerAudioComponent::HandleLanded(float FallSpeed)
{
	FVector Foot;
	const FSWSurfaceFootsteps* Sounds = ResolveSurfaceSounds(TraceSurface(Foot));
	if (!Sounds)
	{
		return;
	}

	const USWAudioSettings& Settings = USWAudioSettings::Get();
	const bool bHard = FallSpeed >= Settings.HardLandingSpeed;

	USWSoundSet* Set = bHard
		? (Sounds->LandHard ? Sounds->LandHard : Sounds->Walk)
		: (Sounds->LandSoft ? Sounds->LandSoft : Sounds->Walk);

	if (Set)
	{
		// Scale with how far they actually fell, so a hop and a drop are not the same sound.
		const float Scale = FMath::GetMappedRangeValueClamped(
			FVector2D(120.f, Settings.HardLandingSpeed * 1.8f), FVector2D(0.45f, 1.2f), FallSpeed);
		Set->PlayAtLocation(this, Foot, Scale);
	}

	// The weight of the body, under the sound of the shoe.
	if (bHard && Sounds->BodyImpact)
	{
		Sounds->BodyImpact->PlayAtLocation(this, Foot, 1.f);
	}

	if (bHard && ClothSet)
	{
		ClothSet->Play2D(this, 0.8f);
	}
}

void USWPlayerAudioComponent::HandleFlashlight(bool bOn)
{
	if (FlashlightClickSet)
	{
		// The click is the same switch either way; a small pitch offset is what
		// makes on and off feel like different directions of the same mechanism.
		FlashlightClickSet->Play2D(this, 1.f, bOn ? 1.f : 0.94f);
	}
}

void USWPlayerAudioComponent::HandleCrouch(bool bCrouched)
{
	if (ClothSet)
	{
		ClothSet->Play2D(this, bCrouched ? 0.6f : 0.5f, bCrouched ? 0.95f : 1.05f);
	}
}

void USWPlayerAudioComponent::HandleLean(float Direction)
{
	if (ClothSet && !FMath::IsNearlyZero(Direction))
	{
		ClothSet->Play2D(this, 0.35f);
	}
}

// ---------------------------------------------------------------------------
// Continuous body audio
// ---------------------------------------------------------------------------

float USWPlayerAudioComponent::GetFear() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const USWAudioDirectorSubsystem* Director = World->GetSubsystem<USWAudioDirectorSubsystem>())
		{
			return Director->GetFear();
		}
	}
	return 0.f;
}

float USWPlayerAudioComponent::GetStaminaFraction() const
{
	if (const ASWCharacter* Character = Cast<ASWCharacter>(OwningCharacter.Get()))
	{
		return Character->GetStaminaFraction();
	}
	return 1.f;
}

void USWPlayerAudioComponent::UpdateExertion(float DeltaTime)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();
	const ASWCharacter* Character = Cast<ASWCharacter>(OwningCharacter.Get());

	// Actually running, not just holding the key while walking into a wall.
	const bool bSprinting = Character
		&& Character->bIsSprinting
		&& Character->GetVelocity().Size2D() > Character->WalkSpeed * 1.1f;

	if (bSprinting)
	{
		Exertion += Settings.ExertionRisePerSecond * DeltaTime;
	}
	else
	{
		// The bump lands the frame the sprint ends, so the heart is still climbing
		// as the player slows -- then it falls away slowly.
		if (bWasSprinting)
		{
			Exertion += Settings.ExertionSprintStopBump;
		}
		Exertion -= Settings.ExertionDecayPerSecond * DeltaTime;
	}

	// An empty stamina bar means the body is working hard whatever the feet are
	// doing, so it acts as a floor rather than replacing the model.
	Exertion = FMath::Clamp(FMath::Max(Exertion, 1.f - GetStaminaFraction()), 0.f, 1.f);
	bWasSprinting = bSprinting;
}

void USWPlayerAudioComponent::UpdateBreath(float DeltaTime)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();

	const float Stamina = GetStaminaFraction();
	const float Fear = GetFear();

	// Which lungs we are hearing: spent beats frightened beats calm.
	USWSoundSet* Set = BreathCalmSet;
	if (Stamina <= Settings.BreathExhaustedStamina)
	{
		Set = BreathExhaustedSet ? BreathExhaustedSet : BreathExertedSet;
	}
	else if (Stamina <= Settings.BreathExertedStamina)
	{
		Set = BreathExertedSet ? BreathExertedSet : BreathCalmSet;
	}
	else if (Fear > 0.55f && BreathFearSet)
	{
		Set = BreathFearSet;
	}

	BreathTimer -= DeltaTime;
	if (BreathTimer > 0.f || !Set)
	{
		return;
	}

	// Breathing speeds up with exertion first, fear second.
	const float Effort = FMath::Max(Exertion, Fear * 0.7f);
	const float Interval = FMath::Lerp(Settings.BreathIntervalRange.X, Settings.BreathIntervalRange.Y, Effort);
	BreathTimer = FMath::Max(0.35f, Interval * FMath::FRandRange(0.9f, 1.1f));

	Set->Play2D(this, Settings.BreathVolume * FMath::Lerp(0.6f, 1.f, Effort));
}

void USWPlayerAudioComponent::UpdateHeartbeat(float DeltaTime)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();

	const float Fear = GetFear();
	const float Drive = FMath::Max(Fear, Exertion);

	HeartTimer -= DeltaTime;
	if (HeartTimer > 0.f)
	{
		return;
	}

	const float BPM = FMath::Lerp(Settings.HeartbeatBPMRange.X, Settings.HeartbeatBPMRange.Y, Drive);
	HeartTimer = 60.f / FMath::Max(BPM, 30.f);

	// Below the threshold the heart is simply not part of the mix. A heartbeat
	// you can always hear stops meaning anything.
	if (Fear < Settings.HeartbeatFearThreshold && Exertion < 0.5f)
	{
		return;
	}

	USWSoundSet* Set = HeartbeatCalmSet;
	if (Drive > 0.75f && HeartbeatPoundingSet)
	{
		Set = HeartbeatPoundingSet;
	}
	else if (Drive > 0.45f && HeartbeatRaisedSet)
	{
		Set = HeartbeatRaisedSet;
	}

	if (Set)
	{
		const float Volume = Settings.HeartbeatVolume * FMath::GetMappedRangeValueClamped(
			FVector2D(Settings.HeartbeatFearThreshold, 1.f), FVector2D(0.25f, 1.f), Drive);
		Set->Play2D(this, Volume);
	}
}

void USWPlayerAudioComponent::UpdateTurnRustle(float DeltaTime)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !ClothSet || DeltaTime <= 0.f)
	{
		return;
	}

	const float Yaw = Owner->GetActorRotation().Yaw;
	const float YawRate = FMath::Abs(FMath::FindDeltaAngleDegrees(PreviousYaw, Yaw)) / DeltaTime;
	PreviousYaw = Yaw;

	if (YawRate > USWAudioSettings::Get().TurnRustleSpeed && TurnRustleCooldown <= 0.f)
	{
		ClothSet->Play2D(this, 0.4f);
		TurnRustleCooldown = 0.8f;
	}
}

void USWPlayerAudioComponent::UpdateScuff(float DeltaTime)
{
	const ACharacter* Character = OwningCharacter.Get();
	if (!Character)
	{
		return;
	}

	const float Speed = Character->GetVelocity().Size2D();

	// Braking hard from a run drags the foot: the sound of stopping, which is
	// missing from almost every indie game and instantly noticeable when present.
	if (PreviousSpeed > 300.f && Speed < PreviousSpeed * 0.35f && ScuffCooldown <= 0.f
		&& !Character->GetCharacterMovement()->IsFalling())
	{
		FVector Foot;
		const FSWSurfaceFootsteps* Sounds = ResolveSurfaceSounds(TraceSurface(Foot));
		if (Sounds && Sounds->Scuff)
		{
			Sounds->Scuff->PlayAtLocation(this, Foot, 0.8f);
			ScuffCooldown = 0.6f;
		}
	}

	PreviousSpeed = Speed;
}
