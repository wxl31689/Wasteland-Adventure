#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WAStaminaTestObserver.generated.h"

class UWAStaminaComponent;
UCLASS(Transient, NotBlueprintable)
class UWAStaminaTestObserver : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void Changed(UWAStaminaComponent* Stamina, float Previous, float Current, float Delta);
	UFUNCTION()
	void Depleted(UWAStaminaComponent* Stamina);
	UFUNCTION()
	void Recovered(UWAStaminaComponent* Stamina);
	int32 Changes = 0, Depletions = 0, Recoveries = 0;
	float Spent = 0.0f;
	bool bConsistent = true;
};
