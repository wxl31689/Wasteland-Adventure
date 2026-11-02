#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/WAHealthComponent.h"
#include "Tests/WAHealthTestObserver.h"
#include "WastelandAdventureCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace WastelandAdventure::Tests
{
class FHealthPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FHealthPIECommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - CreatedAt > 180.0) { Test->AddError(TEXT("Health rendered PIE timed out.")); Cleanup(); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World) { return false; }
		Pawn = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Pawn || !PC) { return false; }
		if (!bInitialized)
		{
			Health = Pawn->GetHealthComponent();
			if (!Test->TestNotNull(TEXT("Actual Blueprint pawn inherits native health component"), Health)) { return true; }
			Observer.Reset(NewObject<UWAHealthTestObserver>());
			Health->OnHealthChanged.AddUniqueDynamic(Observer.Get(), &UWAHealthTestObserver::RecordChange);
			Health->OnDeath.AddUniqueDynamic(Observer.Get(), &UWAHealthTestObserver::RecordDeath);
			Frames = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/Phase3_1"), FString::Printf(TEXT("Frames-%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
			IFileManager::Get().MakeDirectory(*Frames, true);
			Test->AddInfo(FString::Printf(TEXT("HEALTH_PIE_ACTUAL pawn=%s component=%s animation=%s map=%s dodge_key=LeftControl"),
				*Pawn->GetClass()->GetPathName(), *Health->GetPathName(), *GetNameSafe(Pawn->GetMesh()->GetAnimInstance()), *World->GetPathName()));
			BuildSteps(World); StepStarted = World->GetTimeSeconds(); bInitialized = true;
		}
		if (Step >= Steps.Num()) { Cleanup(); return true; }
		if (World->GetTimeSeconds() - StepStarted < Steps[Step].Delay) { return false; }
		Test->AddInfo(FString::Printf(TEXT("%s hp=%.2f max=%.2f dead=%d changes=%d deaths=%d speed=%.2f"),
			*Steps[Step].Name, Health->GetCurrentHealth(), Health->GetMaxHealth(), Health->IsDead(), Observer->ChangeCount, Observer->DeathCount, Pawn->GetVelocity().Size2D()));
		Steps[Step].Run(); ++Step; StepStarted = World->GetTimeSeconds(); return false;
	}

private:
	struct FStep { FString Name; double Delay; TFunction<void()> Run; };
	void Add(const TCHAR* Name, double Delay, TFunction<void()> Run) { Steps.Add({Name, Delay, MoveTemp(Run)}); }
	void Key(const FKey& KeyValue, EInputEvent Event) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(KeyValue, Event, Event == IE_Released ? 0.0f : 1.0f)); }
	void CheckHP(float Expected) { Test->TestEqual(TEXT("Actual runtime HP"), Health->GetCurrentHealth(), Expected); }
	void Capture(const TCHAR* Name)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(DebugKey, 3.0f, FColor::Yellow, FString::Printf(TEXT("WA 3.1 | HP %.0f / %.0f | Dead %d | Death events %d"),
				Health->GetCurrentHealth(), Health->GetMaxHealth(), Health->IsDead(), Observer->DeathCount));
		}
		const FString Filename = FPaths::Combine(Frames, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Filename, false, false);
		Test->AddInfo(FString::Printf(TEXT("HEALTH_RENDERED_FRAME %s"), *Filename));
	}
	void BuildSteps(UWorld* World)
	{
		Add(TEXT("Initial real pawn health"), 0.5, [this]
		{
			TArray<UWAHealthComponent*> Components; Pawn->GetComponents(Components);
			Test->TestEqual(TEXT("Exactly one health authority on actual pawn"), Components.Num(), 1);
			Test->TestTrue(TEXT("Health component began play and registered"), Health->HasBegunPlay() && Health->IsRegistered());
			CheckHP(100.0f); Test->TestEqual(TEXT("Actual maximum"), Health->GetMaxHealth(), 100.0f);
			Test->TestFalse(TEXT("Actual health initially alive"), Health->IsDead());
			Test->TestFalse(TEXT("No health Tick"), Health->IsComponentTickEnabled());
			Test->TestFalse(TEXT("No health replication"), Health->GetIsReplicated());
			Capture(TEXT("Initial100"));
		});
		Add(TEXT("Engine actor damage route"), 0.4, [this]
		{
			UGameplayStatics::ApplyDamage(Pawn, 20.0f, PC, Pawn, UDamageType::StaticClass()); CheckHP(80.0f);
			Test->TestEqual(TEXT("One actor hit changes HP exactly once"), Observer->ChangeCount, 1);
			Test->TestEqual(TEXT("Actor hit delta is minus twenty, not minus forty"), Observer->LastDelta, -20.0f);
			Health->InitializeHealth(); Health->InitializeHealth(); CheckHP(80.0f); Capture(TEXT("Damage80"));
		});
		Add(TEXT("Actual runtime healing and bounds"), 0.4, [this]
		{
			Health->RestoreHealth(10.0f); CheckHP(90.0f); Health->RestoreHealth(1000.0f); CheckHP(100.0f);
			UGameplayStatics::ApplyDamage(Pawn, -20.0f, PC, Pawn, UDamageType::StaticClass());
			Health->RestoreHealth(-10.0f); CheckHP(100.0f);
			Test->TestEqual(TEXT("Rejected negative operations create no extra change"), Observer->ChangeCount, 3);
			UGameplayStatics::ApplyDamage(Pawn, 1000.0f, PC, Pawn, UDamageType::StaticClass()); CheckHP(0.0f);
			Test->TestTrue(TEXT("Actual pawn component is dead"), Health->IsDead());
			Test->TestEqual(TEXT("Runtime death delegate fires once"), Observer->DeathCount, 1); Capture(TEXT("Dead0"));
		});
		Add(TEXT("Dead runtime rejection"), 0.4, [this]
		{
			UGameplayStatics::ApplyDamage(Pawn, 20.0f, PC, Pawn, UDamageType::StaticClass());
			Health->RestoreHealth(1000.0f); Health->InitializeHealth(); CheckHP(0.0f);
			Test->TestEqual(TEXT("Dead damage/heal/initialize do not notify"), Observer->ChangeCount, 4);
			Test->TestEqual(TEXT("Dead actor never repeats death"), Observer->DeathCount, 1);
			Test->TestTrue(TEXT("Health death does not enable ragdoll"), !Pawn->GetMesh()->IsSimulatingPhysics());
			Test->TestTrue(TEXT("Health death does not disable accepted movement mode"), Pawn->GetCharacterMovement()->MovementMode != MOVE_None);
			Health->ResetHealth(); CheckHP(100.0f); Test->TestFalse(TEXT("Runtime reset explicitly revives"), Health->IsDead()); Capture(TEXT("Reset100"));
		});
		Add(TEXT("New life accepts exactly one subsequent hit"), 0.4, [this, World]
		{
			UGameplayStatics::ApplyDamage(Pawn, 20.0f, PC, Pawn, UDamageType::StaticClass()); CheckHP(80.0f);
			Test->TestEqual(TEXT("Reset did not duplicate damage binding"), Observer->ChangeCount, 6);
			Test->TestTrue(TEXT("Dynamic callbacks see committed state"), Observer->bConsistent);
			AActor* Generic = World->SpawnActor<AActor>(); Generic->SetFlags(RF_Transient);
			UWAHealthComponent* GenericHealth = NewObject<UWAHealthComponent>(Generic);
			Generic->AddInstanceComponent(GenericHealth); GenericHealth->RegisterComponent();
			Test->TestTrue(TEXT("Independent component begins play on an ordinary actor"), GenericHealth->HasBegunPlay());
			UGameplayStatics::ApplyPointDamage(Generic, 20.0f, FVector::ForwardVector, FHitResult(), PC, Pawn, UDamageType::StaticClass());
			Test->TestEqual(TEXT("Point and generic notifications do not double deduct"), GenericHealth->GetCurrentHealth(), 80.0f);
			GenericHealth->DestroyComponent();
			Test->TestFalse(TEXT("Component destruction unbinds actor damage"), Generic->OnTakeAnyDamage.IsBound()); Generic->Destroy();
			Floor = World->SpawnActor<AActor>(); Floor->SetFlags(RF_Transient);
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor.Get()); Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
			Box->SetBoxExtent(FVector(4000.0, 4000.0, 50.0)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
			Floor->SetActorLocation(FVector(30000.0, 0.0, -50.0));
			Test->TestTrue(TEXT("Isolated locomotion setup placement succeeds"), Pawn->TeleportTo(FVector(30000.0, 0.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.0), FRotator::ZeroRotator));
			PC->SetControlRotation(FRotator::ZeroRotator);
		});
		Add(TEXT("Health coexistence movement starts"), 0.6, [this] { Origin = Pawn->GetActorLocation(); Key(EKeys::W, IE_Pressed); });
		Add(TEXT("Health coexistence W"), 0.7, [this]
		{
			Test->TestTrue(TEXT("W moves with health component"), Pawn->GetActorLocation().X > Origin.X + 100.0);
			Test->TestTrue(TEXT("Walk reaches 500 with health component"), FMath::IsNearlyEqual(Pawn->GetVelocity().Size2D(), 500.0, 15.0)); Key(EKeys::LeftShift, IE_Pressed);
		});
		Add(TEXT("Health coexistence Shift"), 0.6, [this]
		{
			Test->TestTrue(TEXT("Shift reaches actual 750"), Pawn->GetVelocity().Size2D() > 700.0); CheckHP(80.0f); Key(EKeys::LeftShift, IE_Released);
		});
		Add(TEXT("Health coexistence Shift release"), 0.35, [this]
		{
			Test->TestEqual(TEXT("Release returns max to 500"), Pawn->GetCharacterMovement()->MaxWalkSpeed, 500.0f);
			Key(EKeys::W, IE_Released); Key(EKeys::D, IE_Pressed); Origin = Pawn->GetActorLocation();
		});
		Add(TEXT("Health coexistence D"), 0.45, [this]
		{
			Test->TestTrue(TEXT("D moves right"), Pawn->GetActorLocation().Y > Origin.Y + 50.0); Key(EKeys::D, IE_Released); Key(EKeys::A, IE_Pressed); Origin = Pawn->GetActorLocation();
		});
		Add(TEXT("Health coexistence A"), 0.6, [this]
		{
			Test->TestTrue(TEXT("A moves left"), Pawn->GetActorLocation().Y < Origin.Y - 40.0); Key(EKeys::A, IE_Released); Key(EKeys::S, IE_Pressed); Origin = Pawn->GetActorLocation();
		});
		Add(TEXT("Health coexistence S"), 0.5, [this]
		{
			Test->TestTrue(TEXT("S moves backward"), Pawn->GetActorLocation().X < Origin.X - 40.0); Key(EKeys::S, IE_Released);
		});
		Add(TEXT("Health coexistence jump starts"), 0.4, [this] { Origin = Pawn->GetActorLocation(); Key(EKeys::SpaceBar, IE_Pressed); });
		Add(TEXT("Health coexistence Space"), 0.15, [this]
		{
			Test->TestTrue(TEXT("Space jumps normally"), Pawn->GetCharacterMovement()->IsFalling() && Pawn->GetActorLocation().Z > Origin.Z + 20.0); Key(EKeys::SpaceBar, IE_Released);
		});
		Add(TEXT("Health coexistence landing and Left Ctrl"), 1.3, [this]
		{
			Test->TestTrue(TEXT("Jump lands normally"), Pawn->GetCharacterMovement()->IsMovingOnGround()); Origin = Pawn->GetActorLocation(); Key(EKeys::LeftControl, IE_Pressed);
		});
		Add(TEXT("Health coexistence accepted dodge"), 0.08, [this]
		{
			Test->TestTrue(TEXT("Accepted Left Ctrl still dodges"), Pawn->IsDodging()); Key(EKeys::LeftControl, IE_Released);
		});
		Add(TEXT("Health coexistence dodge recovery"), 0.4, [this]
		{
			Test->TestFalse(TEXT("Dodge finishes normally"), Pawn->IsDodging()); Test->TestTrue(TEXT("Dodge produces actual displacement"), FVector::Dist2D(Origin, Pawn->GetActorLocation()) > 400.0);
			ViewBefore = PC->GetControlRotation(); PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, 20.0f));
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, 10.0f));
		});
		Add(TEXT("Health coexistence camera and animation"), 0.25, [this]
		{
			const FRotator View = PC->GetControlRotation();
			Test->TestTrue(TEXT("Mouse still changes yaw and pitch"), FMath::Abs(FRotator::NormalizeAxis(View.Yaw-ViewBefore.Yaw)) > 1.0 && FMath::Abs(FRotator::NormalizeAxis(View.Pitch-ViewBefore.Pitch)) > 1.0);
			UAnimInstance* Animation = Pawn->GetMesh()->GetAnimInstance(); Test->TestNotNull(TEXT("Original animation instance remains active"), Animation);
			if (Animation) { Test->TestTrue(TEXT("Original ABP_Unarmed remains bound"), Animation->GetClass()->GetPathName().Contains(TEXT("ABP_Unarmed"))); }
			CheckHP(80.0f); Test->TestFalse(TEXT("Movement does not mutate health or death"), Health->IsDead());
			Test->TestEqual(TEXT("No unexpected health change during locomotion"), Observer->ChangeCount, 6);
			Test->AddInfo(TEXT("HEALTH_ACTOR_PIE_FINISHED: actual Blueprint pawn native component; actor and point damage deduct once, runtime reset and dynamic delegates; WASD, Shift, Space, Left Ctrl, mouse and original animation coexist; no assets saved."));
		});
	}
	void Cleanup()
	{
		if (bCleaned) { return; } bCleaned = true;
		if (PC) { PC->FlushPressedKeys(); }
		if (Health && Observer.IsValid())
		{
			Health->OnHealthChanged.RemoveDynamic(Observer.Get(), &UWAHealthTestObserver::RecordChange);
			Health->OnDeath.RemoveDynamic(Observer.Get(), &UWAHealthTestObserver::RecordDeath);
		}
		if (Floor.IsValid()) { Floor->Destroy(); }
		if (GEngine) { GEngine->RemoveOnScreenDebugMessage(DebugKey); }
	}
	static constexpr uint64 DebugKey = 310031;
	FAutomationTestBase* Test;
	AWastelandAdventureCharacter* Pawn = nullptr;
	APlayerController* PC = nullptr;
	UWAHealthComponent* Health = nullptr;
	TStrongObjectPtr<UWAHealthTestObserver> Observer;
	TWeakObjectPtr<AActor> Floor;
	TArray<FStep> Steps;
	FString Frames;
	FVector Origin;
	FRotator ViewBefore;
	double CreatedAt = FPlatformTime::Seconds(), StepStarted = 0.0;
	int32 Step = 0;
	bool bInitialized = false, bCleaned = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWAHealthPIETest, "WastelandAdventure.Health.ActorPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWAHealthPIETest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Stop existing PIE before health acceptance; user sessions are not replaced.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(WastelandAdventure::Tests::FHealthPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
