#include "Audio/SWSoundSet.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

bool USWSoundSet::IsPlayable() const
{
	return Sounds.Num() > 0 && Volume > 0.f;
}

USoundBase* USWSoundSet::Pick()
{
	if (!IsPlayable())
	{
		return nullptr;
	}

	if (Probability < 1.f && FMath::FRand() > Probability)
	{
		return nullptr;
	}

	// Build the list of takes that are not in recent memory. If the memory is as
	// long as the set (or longer), every take is barred -- so fall back to the
	// whole set rather than going silent.
	TArray<int32> Candidates;
	Candidates.Reserve(Sounds.Num());
	for (int32 Index = 0; Index < Sounds.Num(); ++Index)
	{
		if (Sounds[Index] && !RecentIndices.Contains(Index))
		{
			Candidates.Add(Index);
		}
	}
	if (Candidates.Num() == 0)
	{
		RecentIndices.Reset();
		for (int32 Index = 0; Index < Sounds.Num(); ++Index)
		{
			if (Sounds[Index])
			{
				Candidates.Add(Index);
			}
		}
	}
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	const int32 Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];

	RecentIndices.Add(Chosen);
	const int32 Memory = FMath::Clamp(NoRepeatDepth, 0, FMath::Max(0, Sounds.Num() - 1));
	while (RecentIndices.Num() > Memory)
	{
		RecentIndices.RemoveAt(0);
	}

	return Sounds[Chosen];
}

UAudioComponent* USWSoundSet::PlayAtLocation(const UObject* WorldContextObject, const FVector& Location,
	float VolumeScale, float PitchScale)
{
	USoundBase* Sound = Pick();
	if (!Sound || !WorldContextObject)
	{
		return nullptr;
	}

	const float FinalVolume = Volume * VolumeScale * FMath::FRandRange(VolumeRange.X, VolumeRange.Y);
	const float FinalPitch = PitchScale * FMath::FRandRange(PitchRange.X, PitchRange.Y);

	return UGameplayStatics::SpawnSoundAtLocation(WorldContextObject, Sound, Location, FRotator::ZeroRotator,
		FinalVolume, FinalPitch, 0.f, Attenuation, Concurrency);
}

UAudioComponent* USWSoundSet::Play2D(const UObject* WorldContextObject, float VolumeScale, float PitchScale)
{
	USoundBase* Sound = Pick();
	if (!Sound || !WorldContextObject)
	{
		return nullptr;
	}

	const float FinalVolume = Volume * VolumeScale * FMath::FRandRange(VolumeRange.X, VolumeRange.Y);
	const float FinalPitch = PitchScale * FMath::FRandRange(PitchRange.X, PitchRange.Y);

	return UGameplayStatics::SpawnSound2D(WorldContextObject, Sound, FinalVolume, FinalPitch, 0.f, Concurrency);
}
