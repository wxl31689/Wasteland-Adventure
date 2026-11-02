#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WAHealthComponent.generated.h"

class AController;
class UDamageType;
class UWAHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FWAHealthChanged, UWAHealthComponent*, HealthComponent,
	float, PreviousHealth, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWAHealthDeath, UWAHealthComponent*, HealthComponent);

/** Single local health authority. Does not change movement, animation, or the owner's lifetime. */
UCLASS(ClassGroup=(WastelandAdventure), meta=(BlueprintSpawnableComponent))
class WASTELANDADVENTURE_API UWAHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWAHealthComponent();

	/** Initializes once; repeated calls cannot heal or resurrect. BeginPlay calls this automatically. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void InitializeHealth();

	/** Numeric damage authority. Returns actual HP removed, not the actor damage event's requested amount. */
	UFUNCTION(BlueprintCallable, Category="Health")
	float ApplyDamage(float DamageAmount);

	/** Returns actual HP restored; dead actors require an explicit ResetHealth. */
	UFUNCTION(BlueprintCallable, Category="Health")
	float RestoreHealth(float HealingAmount);

	UFUNCTION(BlueprintPure, Category="Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return bIsDead; }

	/** Explicitly begins a new life at full HP. Mutations during delegate notification are rejected. */
	UFUNCTION(BlueprintCallable, Category="Health")
	void ResetHealth();

	/** Fires only for an actual HP change, after HP and the death latch are committed. Delta is signed. */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FWAHealthChanged OnHealthChanged;

	/** Fires once per life, after the lethal OnHealthChanged. No death gameplay is performed here. */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FWAHealthDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health", meta=(AllowPrivateAccess="true", ClampMin="1.0", UIMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Health", meta=(AllowPrivateAccess="true"))
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Health", meta=(AllowPrivateAccess="true"))
	bool bIsDead = false;

	bool bInitialized = false;
	bool bNotifying = false;

	void NormalizeMaxHealth();
	void SetHealthAndNotify(float NewHealth);

	/** Only adapter from the engine actor damage pipeline. Do not also bind point/radial damage. */
	UFUNCTION()
	void HandleOwnerDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
