#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/WAHealthComponent.h"
#include "Tests/WAHealthTestObserver.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWAHealthRulesTest, "WastelandAdventure.Health.ComponentRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWAHealthRulesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWAHealthComponent> Health(NewObject<UWAHealthComponent>());
	TStrongObjectPtr<UWAHealthTestObserver> Observer(NewObject<UWAHealthTestObserver>());
	Health->OnHealthChanged.AddUniqueDynamic(Observer.Get(), &UWAHealthTestObserver::RecordChange);
	Health->OnDeath.AddUniqueDynamic(Observer.Get(), &UWAHealthTestObserver::RecordDeath);
	Health->InitializeHealth();
	TestEqual(TEXT("01 Initial HP is 100"), Health->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Initial max is 100"), Health->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Initial percent is one"), Health->GetHealthPercent(), 1.0f);
	TestFalse(TEXT("Initially alive"), Health->IsDead());
	TestEqual(TEXT("Damage returns actual loss"), Health->ApplyDamage(20.0f), 20.0f);
	TestEqual(TEXT("02 Damage 20 leaves 80"), Health->GetCurrentHealth(), 80.0f);
	TestEqual(TEXT("Damage broadcasts signed delta"), Observer->LastDelta, -20.0f);
	Health->InitializeHealth();
	TestEqual(TEXT("Repeated initialization cannot heal"), Health->GetCurrentHealth(), 80.0f);
	TestEqual(TEXT("Healing returns actual gain"), Health->RestoreHealth(10.0f), 10.0f);
	TestEqual(TEXT("03 Healing 10 leaves 90"), Health->GetCurrentHealth(), 90.0f);
	TestEqual(TEXT("Healing broadcasts signed delta"), Observer->LastDelta, 10.0f);
	TestEqual(TEXT("Overheal reports only missing HP"), Health->RestoreHealth(1000.0f), 10.0f);
	TestEqual(TEXT("04 Overheal clamps to 100"), Health->GetCurrentHealth(), 100.0f);
	TestEqual(TEXT("Only actual changes notify"), Observer->ChangeCount, 3);
	TestEqual(TEXT("Overkill returns only remaining HP"), Health->ApplyDamage(1000.0f), 100.0f);
	TestEqual(TEXT("05 Overkill clamps to zero"), Health->GetCurrentHealth(), 0.0f);
	TestTrue(TEXT("Lethal damage latches death"), Health->IsDead());
	TestEqual(TEXT("06 Death event fires once"), Observer->DeathCount, 1);
	const int32 ChangesAtDeath = Observer->ChangeCount;
	TestEqual(TEXT("07 Damage after death removes no HP"), Health->ApplyDamage(20.0f), 0.0f);
	TestEqual(TEXT("Repeated lethal damage remains harmless"), Health->ApplyDamage(1000.0f), 0.0f);
	TestEqual(TEXT("08 Dead ordinary healing is ineffective"), Health->RestoreHealth(1000.0f), 0.0f);
	Health->InitializeHealth();
	TestTrue(TEXT("Initialization cannot resurrect"), Health->IsDead());
	TestEqual(TEXT("Dead HP remains zero"), Health->GetCurrentHealth(), 0.0f);
	TestEqual(TEXT("Dead events do not repeat"), Observer->DeathCount, 1);
	TestEqual(TEXT("Rejected dead operations do not notify"), Observer->ChangeCount, ChangesAtDeath);
	Health->ResetHealth();
	TestEqual(TEXT("09 Explicit reset restores 100"), Health->GetCurrentHealth(), 100.0f);
	TestFalse(TEXT("Explicit reset clears death"), Health->IsDead());
	Health->ApplyDamage(20.0f);
	Health->RestoreHealth(10.0f);
	TestEqual(TEXT("Damage and healing work after reset"), Health->GetCurrentHealth(), 90.0f);
	const int32 ChangesBeforeInvalid = Observer->ChangeCount;
	TestEqual(TEXT("10 Negative damage rejected"), Health->ApplyDamage(-20.0f), 0.0f);
	TestEqual(TEXT("10 Negative healing rejected"), Health->RestoreHealth(-10.0f), 0.0f);
	Health->ApplyDamage(0.0f); Health->RestoreHealth(0.0f);
	Health->ApplyDamage(std::numeric_limits<float>::quiet_NaN());
	Health->RestoreHealth(std::numeric_limits<float>::infinity());
	Health->ApplyDamage(std::numeric_limits<float>::infinity());
	Health->RestoreHealth(std::numeric_limits<float>::quiet_NaN());
	TestEqual(TEXT("Invalid numeric operations preserve HP"), Health->GetCurrentHealth(), 90.0f);
	TestEqual(TEXT("Invalid operations never notify"), Observer->ChangeCount, ChangesBeforeInvalid);
	TestTrue(TEXT("All dynamic delegates see committed consistent state"), Observer->bConsistent);
	Observer->bAttemptReentrantMutation = true;
	Health->ApplyDamage(1000.0f);
	TestEqual(TEXT("A new life can emit exactly one new death"), Observer->DeathCount, 2);
	TestTrue(TEXT("Synchronous delegate mutation cannot resurrect during death notification"), Health->IsDead());
	Observer->bAttemptReentrantMutation = false;
	Health->ResetHealth();
	TestFalse(TEXT("Reset after notification works normally"), Health->IsDead());
	TestFalse(TEXT("Component never ticks"), Health->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Component is not replicated"), Health->GetIsReplicated());

	TStrongObjectPtr<UWAHealthComponent> Configured(NewObject<UWAHealthComponent>());
	FFloatProperty* MaxProperty = FindFProperty<FFloatProperty>(Configured->GetClass(), TEXT("MaxHealth"));
	if (!TestNotNull(TEXT("Editor max HP property exists"), MaxProperty)) { return false; }
	MaxProperty->SetPropertyValue_InContainer(Configured.Get(), 250.0f);
	Configured->InitializeHealth();
	TestEqual(TEXT("Configured maximum initializes full health"), Configured->GetCurrentHealth(), 250.0f);
	Configured->ApplyDamage(125.0f);
	TestEqual(TEXT("Configured max percent is correct"), Configured->GetHealthPercent(), 0.5f);
	Configured->RestoreHealth(std::numeric_limits<float>::max());
	TestEqual(TEXT("Very large finite heal stays bounded"), Configured->GetCurrentHealth(), 250.0f);
	MaxProperty->SetPropertyValue_InContainer(Configured.Get(), std::numeric_limits<float>::quiet_NaN());
	Configured->ResetHealth();
	TestEqual(TEXT("Invalid editor max resets to safe default"), Configured->GetMaxHealth(), 100.0f);
	TestEqual(TEXT("Invalid max never propagates NaN to HP"), Configured->GetCurrentHealth(), 100.0f);
	AddInfo(TEXT("HEALTH_COMPONENT_RULES_FINISHED: all ten required cases, actual dynamic delegates, idempotent initialization, numeric bounds, reset and notification reentrancy."));
	return true;
}
#endif
