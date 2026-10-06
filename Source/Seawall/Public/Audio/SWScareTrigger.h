// SEAWALL -- a place in the level where something happens to you.
//
// Every scare in the game is one of a handful of recipes (see ESWScareRecipe),
// and a scare trigger is just a box that says "run this recipe here". Keeping
// the recipes in the director rather than in each trigger means the cooldown,
// the ducking and the aftermath behave identically everywhere -- and means a
// scare can be re-tuned once instead of twenty times.

#pragma once

#include "Audio/SWAudioDirectorSubsystem.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SWScareTrigger.generated.h"

class UBoxComponent;

UCLASS()
class SEAWALL_API ASWScareTrigger : public AActor
{
	GENERATED_BODY()

public:
	ASWScareTrigger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seawall|Scare")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Scare")
	ESWScareRecipe Recipe = ESWScareRecipe::SilenceDrop;

	/** Most scares are ruined the second time. Off only for ambient dread triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Scare")
	bool bOnce = true;

	/** Where the sound comes from. Empty means the trigger's own location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Scare")
	TObjectPtr<AActor> SoundSource;

	/** While inside, fear cannot drop below this. Use it to make a whole area feel wrong. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Scare", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FearFloorWhileInside = 0.f;

	/** Lights killed when the recipe is LightSurge -- the pop and the darkness are one event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Scare")
	TArray<TObjectPtr<AActor>> LightsToKill;

	/** Seconds after the surge before the lights actually die. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seawall|Scare", meta = (ClampMin = "0.0"))
	float LightKillDelay = 0.35f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void KillLights();

	/** True for the local player pawn only -- a physics prop must not trip a scare. */
	bool IsPlayer(const AActor* Other) const;

private:
	bool bFired = false;
	FTimerHandle LightTimer;
};
