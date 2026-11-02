// Real PIE input/physics tests. Fixtures and per-instance parameter changes are never saved.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "WastelandAdventureCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace WastelandAdventure::Tests
{
class FDodgePIECommand final : public IAutomationLatentCommand
{
public:
	explicit FDodgePIECommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (FPlatformTime::Seconds() - CreatedAt > 180.0)
		{
			Test->AddError(TEXT("Dodge PIE timed out."));
			Cleanup();
			return true;
		}
		if (!World) { return false; }
		Pawn = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Pawn || !PC) { return false; }
		if (!bInitialized)
		{
			const FStructProperty* KeyProperty = FindFProperty<FStructProperty>(Pawn->GetClass(), TEXT("DodgeKey"));
			if (!KeyProperty) { Test->AddError(TEXT("DodgeKey is not an editor-configurable property.")); return true; }
			DodgeKey = *KeyProperty->ContainerPtrToValuePtr<FKey>(Pawn);
			Test->AddInfo(FString::Printf(TEXT("PIE pawn=%s key=%s"), *Pawn->GetClass()->GetPathName(), *DodgeKey.ToString()));
			// Isolate physics at a temporary floor. This is fixture setup, not the dodge implementation.
			Box(FVector(20000.0, 0.0, -50.0), FVector(5000.0, 5000.0, 50.0));
			Pawn->TeleportTo(FVector(20000.0, 0.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator::ZeroRotator);
			PC->SetControlRotation(FRotator(0.0, 90.0, 0.0)); // Idle fallback must use actor facing, not camera yaw.
			BuildSteps();
			bInitialized = true;
			StepStartedAt = World->GetTimeSeconds();
		}
		if (Step >= Steps.Num()) { Cleanup(); return true; }
		if (World->GetTimeSeconds() - StepStartedAt < Steps[Step].Delay) { return false; }
		Test->AddInfo(FString::Printf(TEXT("%s position=%s speed=%.2f dodge=%d cooldown=%.3f"),
			*Steps[Step].Name, *Pawn->GetActorLocation().ToString(), Pawn->GetVelocity().Size2D(),
			Pawn->IsDodging(), Pawn->GetDodgeCooldownRemaining()));
		Steps[Step].Run();
		++Step;
		StepStartedAt = World->GetTimeSeconds();
		return false;
	}

private:
	struct FStep { FString Name; double Delay; TFunction<void()> Run; };
	void Add(const TCHAR* Name, double Delay, TFunction<void()> Run) { Steps.Add({Name, Delay, MoveTemp(Run)}); }
	void Key(const FKey& KeyValue, EInputEvent Event)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(KeyValue, Event, Event == IE_Released ? 0.0f : 1.0f));
	}
	void SetParameter(const TCHAR* Name, float Value)
	{
		FFloatProperty* Property = FindFProperty<FFloatProperty>(Pawn->GetClass(), Name);
		if (Test->TestNotNull(Name, Property)) { Property->SetPropertyValue_InContainer(Pawn, Value); }
	}
	AActor* Box(const FVector& Location, const FVector& Extent)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		AActor* Fixture = Pawn->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Params);
		UBoxComponent* Collision = NewObject<UBoxComponent>(Fixture);
		Fixture->SetRootComponent(Collision);
		Fixture->AddInstanceComponent(Collision);
		Collision->SetBoxExtent(Extent);
		Collision->SetCollisionProfileName(TEXT("BlockAll"));
		Collision->RegisterComponent();
		Fixture->SetActorLocation(Location);
		Fixtures.Add(Fixture);
		return Fixture;
	}
	void Cleanup()
	{
		if (PC) { PC->FlushPressedKeys(); }
		for (TWeakObjectPtr<AActor>& Fixture : Fixtures) { if (Fixture.IsValid()) { Fixture->Destroy(); } }
		Fixtures.Empty();
	}
	void BuildSteps()
	{
		Add(TEXT("Grounded initial fixture"), 0.3, [this]
		{
			Test->TestTrue(TEXT("Real capsule is grounded"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Origin = Pawn->GetActorLocation();
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Idle forward dodge"), 0.08, [this]
		{
			Test->TestTrue(TEXT("Configured dodge key starts movement"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Idle dodge follows actor forward, not camera"), Pawn->GetActorLocation().X > Origin.X + 50.0 && FMath::Abs(Pawn->GetActorLocation().Y - Origin.Y) < 5.0);
			Test->TestTrue(TEXT("Actual dodge speed is 1800"), FMath::IsNearlyEqual(Pawn->GetVelocity().Size2D(), 1800.0f, 10.0f));
			Key(DodgeKey, IE_Released);
			Key(EKeys::SpaceBar, IE_Pressed);
		});
		Add(TEXT("Active dodge rejects jump and repeated press"), 0.08, [this]
		{
			Test->TestTrue(TEXT("Space does not launch an active dodge"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Test->TestFalse(TEXT("Repeated dodge request is rejected while active"), Pawn->TryDodge());
			Key(EKeys::SpaceBar, IE_Released);
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Fixed duration finish"), 0.2, [this]
		{
			const FVector Delta = Pawn->GetActorLocation() - Origin;
			Test->TestFalse(TEXT("Dodge ends after the configured duration"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Unobstructed dodge travels approximately 450 cm"), Delta.X > 405.0 && Delta.X <= 455.0);
			Test->TestTrue(TEXT("Dodge finish has no sliding"), Pawn->GetVelocity().Size2D() < 2.0f);
			Test->TestTrue(TEXT("Cooldown remains after movement ends"), Pawn->GetDodgeCooldownRemaining() > 0.4f);
			Checkpoint = Pawn->GetActorLocation();
		});
		Add(TEXT("Held key does not auto-repeat after cooldown"), 0.85, [this]
		{
			Test->TestFalse(TEXT("Holding dodge does not trigger again when cooldown expires"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Held key does not produce extra displacement"), FVector::Dist2D(Checkpoint, Pawn->GetActorLocation()) < 2.0);
			Test->TestEqual(TEXT("Cooldown resets"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			Key(DodgeKey, IE_Released);
			PC->SetControlRotation(FRotator::ZeroRotator);
			Key(EKeys::D, IE_Pressed);
		});
		Add(TEXT("Directional dodge start"), 0.12, [this]
		{
			Origin = Pawn->GetActorLocation();
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Direction is locked during dodge"), 0.08, [this]
		{
			Test->TestTrue(TEXT("Direction input starts dodge"), Pawn->IsDodging());
			Key(DodgeKey, IE_Released);
			Key(EKeys::D, IE_Released);
			Key(EKeys::W, IE_Pressed);
			Key(EKeys::LeftShift, IE_Pressed);
		});
		Add(TEXT("Movement and held sprint resume"), 0.25, [this]
		{
			const FVector Delta = Pawn->GetActorLocation() - Origin;
			Test->TestTrue(TEXT("Directional dodge follows D input"), Delta.Y > 405.0 && Delta.Y <= 455.0 && FMath::Abs(Delta.X) < 100.0);
			Test->TestFalse(TEXT("Directional dodge finishes"), Pawn->IsDodging());
			Test->TestTrue(TEXT("W movement resumes after dodge"), Pawn->GetVelocity().X > 10.0);
			Test->TestEqual(TEXT("Held sprint speed is preserved"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 750.0f);
			Key(EKeys::W, IE_Released);
			Key(EKeys::LeftShift, IE_Released);
		});
		Add(TEXT("Cooldown blocks fresh key press"), 0.12, [this] { Key(DodgeKey, IE_Pressed); });
		Add(TEXT("Cooldown rejection result"), 0.1, [this]
		{
			Test->TestFalse(TEXT("A new press during cooldown is rejected"), Pawn->IsDodging());
			Test->TestEqual(TEXT("Sprint release restores 500"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.0f);
			Key(DodgeKey, IE_Released);
		});
		Add(TEXT("Jump before dodge"), 0.6, [this] { Key(EKeys::SpaceBar, IE_Pressed); });
		Add(TEXT("Airborne dodge attempt"), 0.1, [this]
		{
			Test->TestTrue(TEXT("Original jump is functional"), Pawn->GetCharacterMovement()->IsFalling());
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Airborne rejection result"), 0.1, [this]
		{
			Test->TestFalse(TEXT("Airborne dodge is rejected"), Pawn->IsDodging());
			Test->TestEqual(TEXT("Rejected dodge spends no cooldown"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			Key(DodgeKey, IE_Released);
			Key(EKeys::SpaceBar, IE_Released);
		});
		Add(TEXT("Input cancellation start"), 1.3, [this]
		{
			Test->TestTrue(TEXT("Jump lands normally"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Input cancellation"), 0.08, [this]
		{
			Test->TestTrue(TEXT("Dodge active before input flush"), Pawn->IsDodging());
			PC->FlushPressedKeys();
			Test->TestFalse(TEXT("Input flush cancels dodge immediately"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Cancellation retains cooldown"), Pawn->GetDodgeCooldownRemaining() > 0.5f);
			Key(DodgeKey, IE_Released);
			Checkpoint = Pawn->GetActorLocation();
		});
		Add(TEXT("Cancellation has no residual movement"), 0.4, [this]
		{
			Test->TestTrue(TEXT("Cancelled root motion does not continue"), FVector::Dist2D(Checkpoint, Pawn->GetActorLocation()) < 2.0);
			Test->TestFalse(TEXT("Cancel cannot bypass cooldown"), Pawn->TryDodge());
		});
		Add(TEXT("Wall collision start"), 0.7, [this]
		{
			Pawn->SetActorRotation(FRotator::ZeroRotator);
			WallX = Pawn->GetActorLocation().X + 250.0;
			Wall = Box(FVector(WallX, Pawn->GetActorLocation().Y, 150.0), FVector(25.0, 500.0, 400.0));
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Wall collision result"), 0.4, [this]
		{
			const double X = Pawn->GetActorLocation().X;
			Test->TestTrue(TEXT("Dodge capsule is blocked by wall"), X < WallX - 55.0 && X > WallX - 120.0);
			Test->TestTrue(TEXT("Wall collision leaves no sliding"), Pawn->GetVelocity().Size2D() < 2.0f);
			Test->TestTrue(TEXT("Wall collision keeps floor contact"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Key(DodgeKey, IE_Released);
			Wall->Destroy();
		});
		Add(TEXT("Unpossess interruption and cooldown reset"), 0.7, [this]
		{
			Test->TestTrue(TEXT("Ready dodge accepted before unpossess"), Pawn->TryDodge());
			PC->UnPossess();
			Test->TestFalse(TEXT("Unpossess clears active dodge"), Pawn->IsDodging());
			Test->TestEqual(TEXT("New possession lifecycle clears cooldown"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			PC->Possess(Pawn);
		});
		Add(TEXT("Speed cap configuration"), 0.2, [this]
		{
			SetParameter(TEXT("DodgeSpeed"), 900.0f);
			Pawn->SetActorRotation(FRotator::ZeroRotator);
			Origin = Pawn->GetActorLocation();
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Speed cap result"), 0.4, [this]
		{
			const double Distance = Pawn->GetActorLocation().X - Origin.X;
			Test->TestTrue(TEXT("Configured speed cap produces approximately 225 cm"), Distance > 195.0 && Distance <= 230.0);
			Key(DodgeKey, IE_Released);
		});
		Add(TEXT("Distance and duration configuration"), 0.7, [this]
		{
			SetParameter(TEXT("DodgeSpeed"), 1800.0f);
			SetParameter(TEXT("DodgeDistance"), 100.0f);
			SetParameter(TEXT("DodgeDuration"), 0.5f);
			SetParameter(TEXT("DodgeCooldown"), 0.1f);
			Origin = Pawn->GetActorLocation();
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Configured duration remains active"), 0.3, [this]
		{
			Test->TestTrue(TEXT("Configured 0.5 second movement is still active at 0.3"), Pawn->IsDodging());
			Test->TestFalse(TEXT("Cooldown shorter than duration cannot permit repeated dodge"), Pawn->TryDodge());
		});
		Add(TEXT("Distance cap and cooldown result"), 0.3, [this]
		{
			const double Distance = Pawn->GetActorLocation().X - Origin.X;
			Test->TestTrue(TEXT("Configured distance cap stays near 100 cm"), Distance > 85.0 && Distance <= 105.0);
			Test->TestFalse(TEXT("Configured duration ends"), Pawn->IsDodging());
			Test->TestEqual(TEXT("Configured short cooldown clamps to duration and resets"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			Key(DodgeKey, IE_Released);
			SetParameter(TEXT("DodgeDistance"), 450.0f);
			SetParameter(TEXT("DodgeDuration"), 0.25f);
			SetParameter(TEXT("DodgeCooldown"), 1.0f);
			const FVector Center = Pawn->GetActorLocation() + FVector(1000.0, 0.0, 325.0);
			Box(Center - FVector(0.0, 0.0, 25.0), FVector(150.0, 300.0, 25.0));
			Pawn->TeleportTo(Center + FVector(0.0, 0.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator::ZeroRotator);
		});
		Add(TEXT("Platform edge start"), 0.3, [this]
		{
			Test->TestTrue(TEXT("Edge fixture is grounded before dodge"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Origin = Pawn->GetActorLocation();
			Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Leaving edge cancels dodge"), 0.35, [this]
		{
			Test->TestFalse(TEXT("Dodge cancels when leaving ground"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Normal falling takes over at the edge"), Pawn->GetCharacterMovement()->IsFalling());
			Test->TestTrue(TEXT("Edge cancellation prevents a full airborne horizontal dodge"), Pawn->GetActorLocation().X - Origin.X < 350.0);
			Key(DodgeKey, IE_Released);
		});
		Add(TEXT("Final recovery"), 1.5, [this]
		{
			Test->TestTrue(TEXT("Edge fall lands on floor"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Test->TestEqual(TEXT("Cooldown recovers after interrupted dodge"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			Test->TestTrue(TEXT("No residual dodge velocity"), Pawn->GetVelocity().Size2D() < 2.0f);
			SetParameter(TEXT("DodgeDuration"), 0.0f);
			Test->TestFalse(TEXT("Invalid duration is rejected without state changes"), Pawn->TryDodge());
			SetParameter(TEXT("DodgeDuration"), 0.25f);
			Test->AddInfo(TEXT("DODGE_PIE_REGRESSION_FINISHED: real input, timers, root-motion movement and collision; fixtures only, no assets saved."));
		});
	}

	FAutomationTestBase* Test;
	AWastelandAdventureCharacter* Pawn = nullptr;
	APlayerController* PC = nullptr;
	TArray<FStep> Steps;
	TArray<TWeakObjectPtr<AActor>> Fixtures;
	TWeakObjectPtr<AActor> Wall;
	FKey DodgeKey;
	FVector Origin, Checkpoint;
	double WallX = 0.0;
	double StepStartedAt = 0.0;
	double CreatedAt = FPlatformTime::Seconds();
	int32 Step = 0;
	bool bInitialized = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWastelandDodgePIETest, "WastelandAdventure.Character.DodgePIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWastelandDodgePIETest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Stop the existing PIE session before running DodgePIE.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(WastelandAdventure::Tests::FDodgePIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
