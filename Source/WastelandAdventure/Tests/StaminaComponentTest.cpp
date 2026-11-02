#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/WAStaminaComponent.h"
#include "Tests/WAStaminaTestObserver.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWAStaminaRulesTest, "WastelandAdventure.Stamina.ComponentRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWAStaminaRulesTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
	if (!TestNotNull(TEXT("Isolated real unit-test world"), TestWorld)) { return false; }
	ON_SCOPE_EXIT { TestWorld->DestroyWorld(false); };
	AActor* Owner = TestWorld->SpawnActor<AActor>();
	TStrongObjectPtr<UWAStaminaComponent> S(NewObject<UWAStaminaComponent>(Owner));
	Owner->AddInstanceComponent(S.Get()); S->RegisterComponent();
	TStrongObjectPtr<UWAStaminaTestObserver> O(NewObject<UWAStaminaTestObserver>());
	S->OnStaminaChanged.AddUniqueDynamic(O.Get(), &UWAStaminaTestObserver::Changed);
	S->OnStaminaDepleted.AddUniqueDynamic(O.Get(), &UWAStaminaTestObserver::Depleted);
	S->OnStaminaRecovered.AddUniqueDynamic(O.Get(), &UWAStaminaTestObserver::Recovered);
	S->InitializeStamina();
	TestEqual(TEXT("Initial stamina 100"), S->GetCurrentStamina(), 100.0f);
	TestEqual(TEXT("Maximum 100"), S->GetMaxStamina(), 100.0f);
	TestEqual(TEXT("Default dodge cost 25"), S->GetDodgeCost(), 25.0f);
	TestTrue(TEXT("Consume 25 succeeds"), S->TryConsumeStamina(25.0f));
	TestEqual(TEXT("Consume leaves 75"), S->GetCurrentStamina(), 75.0f);
	TestFalse(TEXT("Cannot afford overspend"), S->CanAffordStamina(76.0f));
	TestFalse(TEXT("Overspend rejected atomically"), S->TryConsumeStamina(76.0f));
	TestEqual(TEXT("Overspend leaves stamina unchanged"), S->GetCurrentStamina(), 75.0f);
	S->InitializeStamina(); TestEqual(TEXT("Initialization cannot refill"), S->GetCurrentStamina(), 75.0f);
	TestEqual(TEXT("Explicit restore reports actual gain"), S->RestoreStamina(1000.0f), 25.0f);
	TestEqual(TEXT("Restoration caps at 100"), S->GetCurrentStamina(), 100.0f);
	const int32 BeforeInvalid = O->Changes;
	TestFalse(TEXT("Negative consume invalid"), S->TryConsumeStamina(-1.0f));
	TestEqual(TEXT("Negative restore invalid"), S->RestoreStamina(-1.0f), 0.0f);
	TestFalse(TEXT("NaN consume invalid"), S->TryConsumeStamina(std::numeric_limits<float>::quiet_NaN()));
	S->RestoreStamina(std::numeric_limits<float>::infinity());
	TestFalse(TEXT("Infinite consume invalid"), S->TryConsumeStamina(std::numeric_limits<float>::infinity()));
	TestTrue(TEXT("Zero cost succeeds without changing state"), S->TryConsumeStamina(0.0f));
	TestEqual(TEXT("Invalid/no-op operations do not notify"), O->Changes, BeforeInvalid);
	TestTrue(TEXT("Exact remaining stamina can be consumed"), S->TryConsumeStamina(100.0f));
	TestEqual(TEXT("Stamina never below zero"), S->GetCurrentStamina(), 0.0f);
	TestEqual(TEXT("One depletion event"), O->Depletions, 1);
	TestFalse(TEXT("Empty sprint is refused"), S->StartSprinting());
	S->TryConsumeStamina(1.0f);
	TestEqual(TEXT("Rejected empty consume does not repeat depletion"), O->Depletions, 1);
	S->RestoreStamina(5.0f); TestEqual(TEXT("Zero-to-positive recovery event"), O->Recoveries, 1);
	S->RestoreStamina(5.0f); TestEqual(TEXT("Positive recovery does not repeat threshold event"), O->Recoveries, 1);
	S->ResetStamina();
	TestTrue(TEXT("Sprint starts when affordable"), S->StartSprinting());
	S->TickComponent(2.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Two seconds of real component drain removes 40"), S->GetCurrentStamina(), 60.0f);
	TestEqual(TEXT("No simultaneous explicit healing while sprinting"), S->RestoreStamina(10.0f), 0.0f);
	S->SetDodging(true); S->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Dodge pauses continuous sprint drain"), S->GetCurrentStamina(), 60.0f);
	TestEqual(TEXT("No restoration while dodging"), S->RestoreStamina(10.0f), 0.0f);
	S->SetDodging(false); S->TickComponent(3.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Drain depletes exactly without going negative"), S->GetCurrentStamina(), 0.0f);
	TestFalse(TEXT("Depletion clears component sprint state"), S->IsSprinting());
	TestTrue(TEXT("Real dynamic events remain consistent"), O->bConsistent);
	TestFalse(TEXT("No replication"), S->GetIsReplicated());

	TStrongObjectPtr<UWAStaminaComponent> C(NewObject<UWAStaminaComponent>(Owner));
	Owner->AddInstanceComponent(C.Get()); C->RegisterComponent();
	const auto Set = [this, &C](const TCHAR* Name, float Value)
	{
		FFloatProperty* P = FindFProperty<FFloatProperty>(C->GetClass(), Name);
		if (TestNotNull(Name, P)) { P->SetPropertyValue_InContainer(C.Get(), Value); }
	};
	Set(TEXT("MaxStamina"), 50.0f); Set(TEXT("InitialStamina"), 200.0f);
	Set(TEXT("SprintDrainRate"), -20.0f); Set(TEXT("DodgeCost"), std::numeric_limits<float>::quiet_NaN());
	Set(TEXT("StaminaRegenDelay"), -1.0f); C->InitializeStamina();
	TestEqual(TEXT("Configured initial is bounded by maximum"), C->GetCurrentStamina(), 50.0f);
	TestEqual(TEXT("Invalid dodge cost normalizes"), C->GetDodgeCost(), 25.0f);
	C->StartSprinting(); C->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Invalid rate falls back to 20 per second"), C->GetCurrentStamina(), 30.0f);
	TestTrue(TEXT("Configured percent"), FMath::IsNearlyEqual(C->GetStaminaPercent(), 0.6f));
	AddInfo(TEXT("STAMINA_COMPONENT_RULES_FINISHED: bounds, atomic costs, invalid values, real dynamic events, configured defaults and deterministic component drain; world delay and Tick scheduling verified separately in PIE."));
	return true;
}
#endif
