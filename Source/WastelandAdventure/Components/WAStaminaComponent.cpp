#include "Components/WAStaminaComponent.h"

#include "Engine/World.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"

UWAStaminaComponent::UWAStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(false);
}

void UWAStaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeStamina();
	QueueRecovery();
}

void UWAStaminaComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRecovery();
	bSprinting = bDodging = false;
	SetComponentTickEnabled(false);
	Super::EndPlay(EndPlayReason);
}

void UWAStaminaComponent::NormalizeConfiguration()
{
	if (!FMath::IsFinite(MaxStamina) || MaxStamina < 1.0f) { MaxStamina = 100.0f; }
	if (!FMath::IsFinite(InitialStamina)) { InitialStamina = MaxStamina; }
	InitialStamina = FMath::Clamp(InitialStamina, 0.0f, MaxStamina);
	if (!FMath::IsFinite(SprintDrainRate) || SprintDrainRate < 0.0f) { SprintDrainRate = 20.0f; }
	if (!FMath::IsFinite(DodgeCost) || DodgeCost < 0.0f) { DodgeCost = 25.0f; }
	if (!FMath::IsFinite(StaminaRegenRate) || StaminaRegenRate < 0.0f) { StaminaRegenRate = 15.0f; }
	if (!FMath::IsFinite(StaminaRegenDelay) || StaminaRegenDelay < 0.0f) { StaminaRegenDelay = 1.0f; }
}

void UWAStaminaComponent::InitializeStamina()
{
	if (bInitialized || bNotifying) { return; }
	NormalizeConfiguration(); bInitialized = true;
	SetStaminaAndNotify(InitialStamina);
	QueueRecovery();
}

bool UWAStaminaComponent::CanAffordStamina(float Amount) const
{
	return FMath::IsFinite(Amount) && Amount >= 0.0f && CurrentStamina >= Amount;
}

bool UWAStaminaComponent::TryConsumeStamina(float Amount)
{
	if (bNotifying || !FMath::IsFinite(Amount) || Amount < 0.0f) { return false; }
	InitializeStamina();
	if (!CanAffordStamina(Amount)) { return false; }
	if (Amount == 0.0f) { return true; }
	SetStaminaAndNotify(CurrentStamina - Amount);
	QueueRecovery();
	return true;
}

bool UWAStaminaComponent::StartSprinting()
{
	if (bNotifying) { return false; }
	InitializeStamina();
	if (IsStaminaDepleted()) { return false; }
	ClearRecovery(); bSprinting = true;
	UpdateTickState();
	return true;
}

void UWAStaminaComponent::StopSprinting()
{
	if (!bSprinting) { return; }
	bSprinting = false;
	QueueRecovery();
}

void UWAStaminaComponent::SetDodging(bool bActive)
{
	if (bDodging == bActive) { return; }
	bDodging = bActive;
	QueueRecovery();
}

float UWAStaminaComponent::RestoreStamina(float Amount)
{
	if (bNotifying || bSprinting || bDodging || !FMath::IsFinite(Amount) || Amount <= 0.0f) { return 0.0f; }
	InitializeStamina();
	const float Restored = FMath::Min(Amount, MaxStamina - CurrentStamina);
	SetStaminaAndNotify(CurrentStamina + Restored);
	if (CurrentStamina >= MaxStamina) { ClearRecovery(); UpdateTickState(); }
	else if (!bRecovering && GetRecoveryDelayRemaining() <= 0.0f) { QueueRecovery(); }
	return Restored;
}

void UWAStaminaComponent::ResetStamina()
{
	if (bNotifying) { return; }
	NormalizeConfiguration(); bInitialized = true;
	ClearRecovery(); SetStaminaAndNotify(MaxStamina); UpdateTickState();
}

float UWAStaminaComponent::GetStaminaPercent() const
{
	return FMath::IsFinite(MaxStamina) && MaxStamina > 0.0f ? FMath::Clamp(CurrentStamina / MaxStamina, 0.0f, 1.0f) : 0.0f;
}

float UWAStaminaComponent::GetRecoveryDelayRemaining() const
{
	return GetWorld() ? FMath::Max(0.0f, GetWorld()->GetTimerManager().GetTimerRemaining(RecoveryTimer)) : 0.0f;
}

void UWAStaminaComponent::ClearRecovery()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RecoveryTimer); }
	bRecovering = false;
}

void UWAStaminaComponent::QueueRecovery()
{
	ClearRecovery();
	if (!bSprinting && !bDodging && CurrentStamina < MaxStamina && StaminaRegenRate > 0.0f && GetWorld())
	{
		if (StaminaRegenDelay <= 0.0f) { BeginRecovery(); }
		else { GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this, &UWAStaminaComponent::BeginRecovery, StaminaRegenDelay, false); }
	}
	UpdateTickState();
}

void UWAStaminaComponent::BeginRecovery()
{
	bRecovering = !bSprinting && !bDodging && CurrentStamina < MaxStamina && StaminaRegenRate > 0.0f;
	UpdateTickState();
}

void UWAStaminaComponent::UpdateTickState()
{
	SetComponentTickEnabled((bSprinting && !bDodging && SprintDrainRate > 0.0f) || bRecovering);
}

void UWAStaminaComponent::SetStaminaAndNotify(float Value)
{
	const float Previous = CurrentStamina;
	CurrentStamina = FMath::Clamp(Value, 0.0f, MaxStamina);
	if (Previous == CurrentStamina) { return; }
	const bool bDepleted = Previous > 0.0f && CurrentStamina == 0.0f;
	const bool bRecovered = Previous == 0.0f && CurrentStamina > 0.0f;
	if (bDepleted) { bSprinting = false; }
	TGuardValue<bool> Guard(bNotifying, true);
	OnStaminaChanged.Broadcast(this, Previous, CurrentStamina, CurrentStamina - Previous);
	if (bDepleted) { OnStaminaDepleted.Broadcast(this); }
	if (bRecovered) { OnStaminaRecovered.Broadcast(this); }
}

void UWAStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f) { return; }
	if (bSprinting && !bDodging)
	{
		const float Cost = static_cast<float>(FMath::Min(static_cast<double>(CurrentStamina), static_cast<double>(SprintDrainRate) * DeltaTime));
		SetStaminaAndNotify(CurrentStamina - Cost);
		if (IsStaminaDepleted()) { QueueRecovery(); }
	}
	else if (bRecovering && !bDodging)
	{
		const float Gain = static_cast<float>(FMath::Min(static_cast<double>(MaxStamina - CurrentStamina), static_cast<double>(StaminaRegenRate) * DeltaTime));
		SetStaminaAndNotify(CurrentStamina + Gain);
		if (CurrentStamina >= MaxStamina) { ClearRecovery(); UpdateTickState(); }
	}
}
