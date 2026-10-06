#include "Audio/SWAudioDirectorSubsystem.h"

#include "Audio/SWAudioSettings.h"
#include "Audio/SWSoundSet.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundSubmix.h"

DEFINE_LOG_CATEGORY_STATIC(LogSWAudio, Log, All);

bool USWAudioDirectorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only in an actually running world. An editor preview world ticking a
	// heartbeat in the background would be a genuinely confusing bug.
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE;
	}
	return false;
}

void USWAudioDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Fear = FearFloor = FearSpike = ThreatFear = 0.f;
	TimeSinceLastScare = 1000.f;
	UE_LOG(LogSWAudio, Log, TEXT("Audio director ready."));
}

void USWAudioDirectorSubsystem::Deinitialize()
{
	if (SubDroneBed) { SubDroneBed->Stop(); }
	if (ClusterBed)  { ClusterBed->Stop(); }
	SubDroneBed = nullptr;
	ClusterBed = nullptr;
	PendingSounds.Reset();
	Super::Deinitialize();
}

TStatId USWAudioDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USWAudioDirectorSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------------------
// Fear
// ---------------------------------------------------------------------------

void USWAudioDirectorSubsystem::AddFearSpike(float Amount)
{
	FearSpike = FMath::Clamp(FearSpike + Amount, 0.f, 1.f);
}

void USWAudioDirectorSubsystem::SetFearFloor(float Floor)
{
	FearFloor = FMath::Clamp(Floor, 0.f, 1.f);
}

void USWAudioDirectorSubsystem::ReportThreat(const FVector& ThreatLocation, float Intensity)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();
	const FVector Listener = GetListenerLocation();
	const float Distance = FVector::Dist(Listener, ThreatLocation);

	// Full fear inside the radius, nothing at twice the radius, smooth between.
	const float Radius = FMath::Max(Settings.ThreatFearRadius, 1.f);
	const float Closeness = 1.f - FMath::Clamp((Distance - Radius) / Radius, 0.f, 1.f);

	// Highest report this frame wins; Tick bleeds it away if nothing reports again.
	ThreatFear = FMath::Max(ThreatFear, FMath::Clamp(Closeness * Intensity, 0.f, 1.f));
}

void USWAudioDirectorSubsystem::UpdateFear(float DeltaTime)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();

	FearSpike = FMath::Max(0.f, FearSpike - Settings.FearDecayPerSecond * DeltaTime);
	// Threat fear decays faster than a scare: once it walks away, relief is quick.
	ThreatFear = FMath::Max(0.f, ThreatFear - Settings.FearDecayPerSecond * 2.f * DeltaTime);

	const float Target = FMath::Clamp(FMath::Max3(FearFloor, FearSpike, ThreatFear), 0.f, 1.f);

	// Rise fast, fall slow. Panic arrives all at once and leaves reluctantly.
	const float Speed = Target > Fear ? 6.f : 0.8f;
	Fear = FMath::FInterpTo(Fear, Target, DeltaTime, Speed);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void USWAudioDirectorSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastScare += DeltaTime;
	SilenceTimer = FMath::Max(0.f, SilenceTimer - DeltaTime);
	FollowerTimeRemaining = FMath::Max(0.f, FollowerTimeRemaining - DeltaTime);

	UpdateFear(DeltaTime);
	UpdateTensionBeds(DeltaTime);
	UpdateWorldDuck(DeltaTime);
	UpdatePendingEvents(DeltaTime);
}

UAudioComponent* USWAudioDirectorSubsystem::SpawnBed(USWSoundSet* Set)
{
	if (!Set || !Set->IsPlayable())
	{
		return nullptr;
	}

	// Starts silent: a bed that fades up from nothing is never heard arriving.
	UAudioComponent* Component = Set->Play2D(this, 0.f);
	if (Component)
	{
		Component->bAutoDestroy = false;
	}
	return Component;
}

void USWAudioDirectorSubsystem::UpdateTensionBeds(float DeltaTime)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();

	if (!SubDroneBed)
	{
		SubDroneBed = SpawnBed(Settings.TensionSubDroneSet.LoadSynchronous());
	}
	if (!ClusterBed)
	{
		ClusterBed = SpawnBed(Settings.TensionClusterSet.LoadSynchronous());
	}

	// Silence drop mutes the beds along with everything else.
	const float SilenceScale = SilenceTimer > 0.f ? Settings.SilenceDropLevel : 1.f;

	if (SubDroneBed)
	{
		const float Target = FMath::Lerp(Settings.TensionSubDroneVolume.X, Settings.TensionSubDroneVolume.Y, Fear) * SilenceScale;
		SubDroneBed->SetVolumeMultiplier(Target);
		// If the wave is not flagged as looping the component simply stops; restart it.
		if (Target > 0.01f && !SubDroneBed->IsPlaying())
		{
			SubDroneBed->Play();
		}
	}

	if (ClusterBed)
	{
		const float Above = FMath::GetRangePct(Settings.TensionClusterFearThreshold, 1.f, Fear);
		const float Target = FMath::Lerp(Settings.TensionClusterVolume.X, Settings.TensionClusterVolume.Y,
			FMath::Clamp(Above, 0.f, 1.f)) * SilenceScale;
		ClusterBed->SetVolumeMultiplier(Target);
		if (Target > 0.01f && !ClusterBed->IsPlaying())
		{
			ClusterBed->Play();
		}
	}
}

void USWAudioDirectorSubsystem::UpdateWorldDuck(float DeltaTime)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();

	USoundSubmix* Submix = Settings.WorldSubmix.LoadSynchronous();
	if (!Submix)
	{
		return;   // no submix authored yet -- everything else still works
	}

	// As fear rises the world recedes: this is the "I can only hear my own
	// heartbeat" effect, and it is also what makes a stinger feel enormous.
	float Target = FMath::Lerp(Settings.WorldDuckRange.X, Settings.WorldDuckRange.Y, Fear);
	if (SilenceTimer > 0.f)
	{
		Target = FMath::Min(Target, Settings.SilenceDropLevel);
	}

	if (!FMath::IsNearlyEqual(Target, LastWorldVolume, 0.005f))
	{
		Submix->SetSubmixOutputVolume(this, Target);
		LastWorldVolume = Target;
	}
}

void USWAudioDirectorSubsystem::UpdatePendingEvents(float DeltaTime)
{
	for (int32 Index = PendingSounds.Num() - 1; Index >= 0; --Index)
	{
		FPendingSound& Pending = PendingSounds[Index];
		Pending.TimeRemaining -= DeltaTime;
		if (Pending.TimeRemaining > 0.f)
		{
			continue;
		}

		if (USWSoundSet* Set = Pending.Set.Get())
		{
			if (Pending.b2D)
			{
				Set->Play2D(this, Pending.VolumeScale);
			}
			else
			{
				Set->PlayAtLocation(this, Pending.Location, Pending.VolumeScale);
			}
		}
		PendingSounds.RemoveAtSwap(Index);
	}
}

FVector USWAudioDirectorSubsystem::GetListenerLocation() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				return Pawn->GetActorLocation();
			}
		}
	}
	return FVector::ZeroVector;
}

// ---------------------------------------------------------------------------
// Scares
// ---------------------------------------------------------------------------

bool USWAudioDirectorSubsystem::CanScare() const
{
	return TimeSinceLastScare >= USWAudioSettings::Get().ScareCooldownSeconds;
}

void USWAudioDirectorSubsystem::PlayScare(ESWScareRecipe Recipe, const FVector& Location)
{
	const USWAudioSettings& Settings = USWAudioSettings::Get();

	switch (Recipe)
	{
	case ESWScareRecipe::SilenceDrop:
		// No payoff on purpose. Teaching the player that silence means something
		// is what makes the next real silence work.
		SilenceDuration = Settings.SilenceDropSeconds;
		SilenceTimer = SilenceDuration;
		UE_LOG(LogSWAudio, Log, TEXT("Scare: silence drop (%.1fs)"), SilenceDuration);
		break;

	case ESWScareRecipe::Stinger:
	{
		if (!CanScare())
		{
			UE_LOG(LogSWAudio, Verbose, TEXT("Scare: stinger skipped, cooldown"));
			return;
		}
		// Silence first, hit second. The gap is most of the effect.
		SilenceDuration = Settings.SilenceDropSeconds * 0.8f;
		SilenceTimer = SilenceDuration;

		FPendingSound Hit;
		Hit.TimeRemaining = SilenceDuration;
		Hit.Set = Settings.StingerSet.LoadSynchronous();
		Hit.b2D = true;
		PendingSounds.Add(Hit);

		AddFearSpike(Settings.StingerFearSpike);
		TimeSinceLastScare = 0.f;
		UE_LOG(LogSWAudio, Log, TEXT("Scare: stinger in %.2fs"), SilenceDuration);
		break;
	}

	case ESWScareRecipe::FollowerSteps:
		FollowerTimeRemaining = 20.f;   // it follows for a while, then simply stops
		FollowerDelay = FMath::FRandRange(Settings.FollowerDelayRange.X, Settings.FollowerDelayRange.Y);
		AddFearSpike(0.25f);
		UE_LOG(LogSWAudio, Log, TEXT("Scare: follower steps armed (delay %.2fs)"), FollowerDelay);
		break;

	case ESWScareRecipe::DistantSlam:
	{
		FPendingSound Slam;
		Slam.TimeRemaining = 0.f;
		Slam.Location = Location;
		Slam.Set = Settings.StingerSet.LoadSynchronous();
		Slam.VolumeScale = 0.6f;        // it is far away and through a wall
		PendingSounds.Add(Slam);
		AddFearSpike(0.35f);
		TimeSinceLastScare = 0.f;
		UE_LOG(LogSWAudio, Log, TEXT("Scare: distant slam at %s"), *Location.ToCompactString());
		break;
	}

	case ESWScareRecipe::LightSurge:
	{
		FPendingSound Surge;
		Surge.TimeRemaining = 0.f;
		Surge.Location = Location;
		Surge.Set = Settings.StingerSet.LoadSynchronous();
		Surge.VolumeScale = 0.8f;
		PendingSounds.Add(Surge);
		AddFearSpike(0.5f);
		TimeSinceLastScare = 0.f;
		UE_LOG(LogSWAudio, Log, TEXT("Scare: light surge at %s"), *Location.ToCompactString());
		break;
	}
	}
}

void USWAudioDirectorSubsystem::NotifyPlayerFootstep(const FVector& Location, bool bSprinting, bool bCrouched)
{
	if (FollowerTimeRemaining <= 0.f)
	{
		return;
	}

	const USWAudioSettings& Settings = USWAudioSettings::Get();

	// Place the echo behind the player, on the line they are walking along, so it
	// reads as someone following rather than a sound effect somewhere.
	FVector Behind = Location;
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				Behind = Location - Pawn->GetActorForwardVector() * Settings.FollowerDistance;
			}
		}
	}

	FPendingSound Step;
	Step.TimeRemaining = FollowerDelay;
	Step.Location = Behind;
	Step.Set = Settings.StingerSet.LoadSynchronous();   // replaced by the follower's own steps in Phase 6
	Step.VolumeScale = 0.35f;
	PendingSounds.Add(Step);
}
