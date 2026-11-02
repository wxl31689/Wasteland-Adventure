#include "Components/WAHealthComponent.h"

#include "GameFramework/Actor.h"
#include "Templates/UnrealTemplate.h"

UWAHealthComponent::UWAHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UWAHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeHealth();
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UWAHealthComponent::HandleOwnerDamage);
	}
}

void UWAHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.RemoveDynamic(this, &UWAHealthComponent::HandleOwnerDamage);
	}
	Super::EndPlay(EndPlayReason);
}

void UWAHealthComponent::NormalizeMaxHealth()
{
	if (!FMath::IsFinite(MaxHealth) || MaxHealth < 1.0f)
	{
		MaxHealth = 100.0f;
	}
}

void UWAHealthComponent::InitializeHealth()
{
	if (bInitialized || bNotifying) { return; }
	NormalizeMaxHealth();
	bInitialized = true;
	SetHealthAndNotify(MaxHealth);
}

float UWAHealthComponent::ApplyDamage(float DamageAmount)
{
	if (bNotifying || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f) { return 0.0f; }
	InitializeHealth();
	if (bIsDead) { return 0.0f; }
	const float AppliedDamage = FMath::Min(DamageAmount, CurrentHealth);
	SetHealthAndNotify(CurrentHealth - AppliedDamage);
	return AppliedDamage;
}

float UWAHealthComponent::RestoreHealth(float HealingAmount)
{
	if (bNotifying || !FMath::IsFinite(HealingAmount) || HealingAmount <= 0.0f) { return 0.0f; }
	InitializeHealth();
	if (bIsDead) { return 0.0f; }
	// Clamp the amount before adding; even very large finite healing cannot overflow HP.
	const float AppliedHealing = FMath::Min(HealingAmount, MaxHealth - CurrentHealth);
	SetHealthAndNotify(CurrentHealth + AppliedHealing);
	return AppliedHealing;
}

float UWAHealthComponent::GetHealthPercent() const
{
	return FMath::IsFinite(MaxHealth) && MaxHealth > 0.0f
		? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

void UWAHealthComponent::ResetHealth()
{
	if (bNotifying) { return; }
	NormalizeMaxHealth();
	bInitialized = true;
	bIsDead = false;
	SetHealthAndNotify(MaxHealth);
}

void UWAHealthComponent::SetHealthAndNotify(float NewHealth)
{
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	const bool bJustDied = !bIsDead && CurrentHealth <= 0.0f;
	if (bJustDied) { bIsDead = true; }
	if (PreviousHealth == CurrentHealth) { return; }

	// Keep both notifications about one committed transition. Listeners may query immediately;
	// any requested mutation (including reset) must be performed after this notification returns.
	TGuardValue<bool> NotificationGuard(bNotifying, true);
	OnHealthChanged.Broadcast(this, PreviousHealth, CurrentHealth, CurrentHealth - PreviousHealth);
	if (bJustDied) { OnDeath.Broadcast(this); }
}

void UWAHealthComponent::HandleOwnerDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	AController* InstigatedBy, AActor* DamageCauser)
{
	if (DamagedActor == GetOwner()) { ApplyDamage(Damage); }
}
