#include "Audio/SWScareTrigger.h"

#include "Components/BoxComponent.h"
#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSWAudio, Log, All);

ASWScareTrigger::ASWScareTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(150.f, 150.f, 120.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Bounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	Bounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Bounds->SetGenerateOverlapEvents(true);
	Bounds->bHiddenInGame = true;
	RootComponent = Bounds;
}

void ASWScareTrigger::BeginPlay()
{
	Super::BeginPlay();

	Bounds->OnComponentBeginOverlap.AddDynamic(this, &ASWScareTrigger::OnBeginOverlap);
	Bounds->OnComponentEndOverlap.AddDynamic(this, &ASWScareTrigger::OnEndOverlap);
}

bool ASWScareTrigger::IsPlayer(const AActor* Other) const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC && Other && Other == PC->GetPawn();
}

void ASWScareTrigger::OnBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (!IsPlayer(OtherActor) || (bOnce && bFired))
	{
		return;
	}

	UWorld* World = GetWorld();
	USWAudioDirectorSubsystem* Director = World ? World->GetSubsystem<USWAudioDirectorSubsystem>() : nullptr;
	if (!Director)
	{
		return;
	}

	bFired = true;

	const FVector Location = SoundSource ? SoundSource->GetActorLocation() : GetActorLocation();
	Director->PlayScare(Recipe, Location);

	if (FearFloorWhileInside > 0.f)
	{
		Director->SetFearFloor(FearFloorWhileInside);
	}

	if (Recipe == ESWScareRecipe::LightSurge && LightsToKill.Num() > 0)
	{
		World->GetTimerManager().SetTimer(LightTimer, this, &ASWScareTrigger::KillLights, LightKillDelay, false);
	}

	UE_LOG(LogSWAudio, Log, TEXT("Scare trigger '%s' fired recipe %d"), *GetName(), static_cast<int32>(Recipe));
}

void ASWScareTrigger::OnEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
	if (!IsPlayer(OtherActor) || FearFloorWhileInside <= 0.f)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (USWAudioDirectorSubsystem* Director = World->GetSubsystem<USWAudioDirectorSubsystem>())
		{
			// Leaving the area lifts the floor; the fear itself still fades out slowly.
			Director->SetFearFloor(0.f);
		}
	}
}

void ASWScareTrigger::KillLights()
{
	for (AActor* Actor : LightsToKill)
	{
		if (!Actor)
		{
			continue;
		}
		// Kill every light on the actor, whatever kind it is -- the fixtures are
		// blueprints with a spot light and an emissive mesh, not bare light actors.
		TArray<ULightComponent*> Lights;
		Actor->GetComponents<ULightComponent>(Lights);
		for (ULightComponent* Light : Lights)
		{
			Light->SetVisibility(false);
		}
	}
	UE_LOG(LogSWAudio, Log, TEXT("Scare trigger '%s' killed %d light actor(s)"), *GetName(), LightsToKill.Num());
}
