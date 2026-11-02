#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "WAStaminaComponent.generated.h"

class UWAStaminaComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FWAStaminaChanged, UWAStaminaComponent*, StaminaComponent,
	float, PreviousStamina, float, NewStamina, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWAStaminaStateEvent, UWAStaminaComponent*, StaminaComponent);

/** Local stamina authority. Tick runs only for active drain or recovery; delay uses a world timer. */
UCLASS(ClassGroup=(WastelandAdventure), meta=(BlueprintSpawnableComponent))
class WASTELANDADVENTURE_API UWAStaminaComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UWAStaminaComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Stamina")
	void InitializeStamina();
	/** Atomic: insufficient/invalid cost has no side effects. A zero cost succeeds without notifying. */
	UFUNCTION(BlueprintCallable, Category="Stamina")
	bool TryConsumeStamina(float Amount);
	UFUNCTION(BlueprintCallable, Category="Stamina")
	bool StartSprinting();
	UFUNCTION(BlueprintCallable, Category="Stamina")
	void StopSprinting();
	/** Explicit refill, allowed only outside sprint/dodge. Does not bypass active-action blocking. */
	UFUNCTION(BlueprintCallable, Category="Stamina")
	float RestoreStamina(float Amount);
	/** Refills to maximum while preserving active action flags; does not release the character's Shift latch. */
	UFUNCTION(BlueprintCallable, Category="Stamina")
	void ResetStamina();
	/** Character sets this only for an accepted dodge and clears it on finish/cancellation. */
	void SetDodging(bool bActive);

	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetCurrentStamina() const { return CurrentStamina; }
	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetMaxStamina() const { return MaxStamina; }
	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetStaminaPercent() const;
	UFUNCTION(BlueprintPure, Category="Stamina")
	bool CanAffordStamina(float Amount) const;
	UFUNCTION(BlueprintPure, Category="Stamina")
	bool IsStaminaDepleted() const { return CurrentStamina <= 0.0f; }
	UFUNCTION(BlueprintPure, Category="Stamina")
	bool IsSprinting() const { return bSprinting; }
	UFUNCTION(BlueprintPure, Category="Stamina")
	bool IsRecovering() const { return bRecovering; }
	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetDodgeCost() const { return DodgeCost; }
	UFUNCTION(BlueprintPure, Category="Stamina")
	float GetRecoveryDelayRemaining() const;

	UPROPERTY(BlueprintAssignable, Category="Stamina")
	FWAStaminaChanged OnStaminaChanged;
	/** Crossed from positive stamina to zero. */
	UPROPERTY(BlueprintAssignable, Category="Stamina")
	FWAStaminaStateEvent OnStaminaDepleted;
	/** Crossed from zero to positive stamina; does not automatically restart sprint. */
	UPROPERTY(BlueprintAssignable, Category="Stamina")
	FWAStaminaStateEvent OnStaminaRecovered;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(AllowPrivateAccess="true", ClampMin="1.0"))
	float MaxStamina = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float InitialStamina = 100.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float SprintDrainRate = 20.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float DodgeCost = 25.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float StaminaRegenRate = 15.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(AllowPrivateAccess="true", ClampMin="0.0", Units="s"))
	float StaminaRegenDelay = 1.0f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category="Stamina", meta=(AllowPrivateAccess="true"))
	float CurrentStamina = 100.0f;

	bool bInitialized = false, bNotifying = false;
	bool bSprinting = false, bDodging = false, bRecovering = false;
	FTimerHandle RecoveryTimer;
	void NormalizeConfiguration();
	void SetStaminaAndNotify(float Value);
	void QueueRecovery();
	void BeginRecovery();
	void ClearRecovery();
	void UpdateTickState();
};
