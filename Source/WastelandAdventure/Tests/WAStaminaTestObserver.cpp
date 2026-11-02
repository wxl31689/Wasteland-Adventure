#include "Tests/WAStaminaTestObserver.h"
#include "Components/WAStaminaComponent.h"
void UWAStaminaTestObserver::Changed(UWAStaminaComponent* Stamina, float Previous, float Current, float Delta)
{
	++Changes;
	if (Delta < 0.0f) { Spent -= Delta; }
	bConsistent &= Stamina && Stamina->GetCurrentStamina() == Current && FMath::IsNearlyEqual(Current-Previous, Delta)
		&& Current >= 0.0f && Current <= Stamina->GetMaxStamina();
}
void UWAStaminaTestObserver::Depleted(UWAStaminaComponent* Stamina)
{
	++Depletions; bConsistent &= Stamina && Stamina->IsStaminaDepleted() && !Stamina->IsSprinting();
}
void UWAStaminaTestObserver::Recovered(UWAStaminaComponent* Stamina)
{
	++Recoveries; bConsistent &= Stamina && !Stamina->IsStaminaDepleted();
}
