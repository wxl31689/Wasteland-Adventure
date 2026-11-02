#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WAHealthTestObserver.generated.h"

class UWAHealthComponent;

/** Transient dynamic-delegate receiver for automation; never spawned by gameplay. */
UCLASS(Transient, NotBlueprintable)
class UWAHealthTestObserver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void RecordChange(UWAHealthComponent* Health, float PreviousHealth, float NewHealth, float Delta);

	UFUNCTION()
	void RecordDeath(UWAHealthComponent* Health);

	int32 ChangeCount = 0;
	int32 DeathCount = 0;
	float LastDelta = 0.0f;
	bool bConsistent = true;
	bool bAttemptReentrantMutation = false;
};
