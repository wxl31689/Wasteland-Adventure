#include "Tests/WAHealthTestObserver.h"
#include "Components/WAHealthComponent.h"

void UWAHealthTestObserver::RecordChange(UWAHealthComponent* Health, float PreviousHealth, float NewHealth, float Delta)
{
	++ChangeCount;
	LastDelta = Delta;
	bConsistent &= Health && Health->GetCurrentHealth() == NewHealth
		&& FMath::IsNearlyEqual(NewHealth - PreviousHealth, Delta)
		&& (NewHealth != 0.0f || Health->IsDead());
	if (Health && bAttemptReentrantMutation)
	{
		Health->ApplyDamage(1.0f);
		Health->RestoreHealth(1.0f);
	}
}

void UWAHealthTestObserver::RecordDeath(UWAHealthComponent* Health)
{
	++DeathCount;
	bConsistent &= Health && Health->IsDead() && Health->GetCurrentHealth() == 0.0f;
	if (Health && bAttemptReentrantMutation) { Health->ResetHealth(); }
}
