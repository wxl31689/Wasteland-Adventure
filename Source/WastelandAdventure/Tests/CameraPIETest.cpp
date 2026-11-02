// Real rendered PIE input, camera and collision checks. Fixtures are transient and never saved.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "WastelandAdventureCharacter.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

namespace WastelandAdventure::Tests
{
class FCameraPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FCameraPIECommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - CreatedAt > 150.0)
		{
			Test->AddError(TEXT("Camera PIE timed out.")); Cleanup(); return true;
		}
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World) { return false; }
		Pawn = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Pawn || !PC || !PC->PlayerCameraManager) { return false; }
		if (!bInitialized)
		{
			Boom = Pawn->GetCameraBoom(); Camera = Pawn->GetFollowCamera();
			if (!Boom || !Camera) { Test->AddError(TEXT("Original camera components missing.")); return true; }
			const FStructProperty* Property = FindFProperty<FStructProperty>(Pawn->GetClass(), TEXT("DodgeKey"));
			if (!Property) { Test->AddError(TEXT("Dodge input property missing.")); return true; }
			DodgeKey = *Property->ContainerPtrToValuePtr<FKey>(Pawn);
			Box(FVector(20000.0, 0.0, -50.0), FVector(5000.0, 5000.0, 50.0));
			Pawn->TeleportTo(FVector(20000.0, 0.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator::ZeroRotator);
			PC->SetControlRotation(FRotator::ZeroRotator);
			BuildSteps(); bInitialized = true; StepStartedAt = World->GetTimeSeconds();
		}
		if (bSampleMotion)
		{
			++MotionSamples;
			const double Lag = LagDistance();
			MaxLag = FMath::Max(MaxLag, Lag);
			bMotionFinite &= !Camera->GetComponentLocation().ContainsNaN() && !Camera->GetComponentRotation().ContainsNaN();
			bMotionRotationStable &= Camera->GetComponentRotation().Equals(PC->GetControlRotation(), 0.1f);
			bMotionLagBounded &= Lag <= Boom->CameraLagMaxDistance + 2.0;
		}
		if (PitchDrive != 0.0f) { Axis(EKeys::MouseY, PitchDrive); }
		if (Step >= Steps.Num()) { Cleanup(); return true; }
		if (World->GetTimeSeconds() - StepStartedAt < Steps[Step].Delay) { return false; }
		Test->AddInfo(FString::Printf(TEXT("%s pawn=%s camera=%s pitch=%.2f lag=%.2f collision=%d"),
			*Steps[Step].Name, *Pawn->GetActorLocation().ToString(), *Camera->GetComponentLocation().ToString(),
			FRotator::NormalizeAxis(PC->GetControlRotation().Pitch), LagDistance(), Boom->IsCollisionFixApplied()));
		Steps[Step].Run(); ++Step; StepStartedAt = World->GetTimeSeconds();
		return false;
	}

private:
	struct FStep { FString Name; double Delay; TFunction<void()> Run; };
	void Add(const TCHAR* Name, double Delay, TFunction<void()> Run) { Steps.Add({Name, Delay, MoveTemp(Run)}); }
	void Key(const FKey& Value, EInputEvent Event)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value, Event, Event == IE_Released ? 0.0f : 1.0f));
	}
	void Axis(const FKey& Value, float Amount) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value, IE_Axis, Amount)); }
	double LagDistance() const
	{
		const FRotator Rotation = Boom->GetTargetRotation();
		const FVector LaggedOrigin = Boom->GetUnfixedCameraPosition() + Rotation.Vector() * Boom->TargetArmLength - Rotation.RotateVector(Boom->SocketOffset);
		return FVector::Distance(LaggedOrigin, Boom->GetComponentLocation() + Boom->TargetOffset);
	}
	AActor* Box(const FVector& Location, const FVector& Extent)
	{
		FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
		AActor* Actor = Pawn->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Params);
		UBoxComponent* Collision = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Collision); Actor->AddInstanceComponent(Collision);
		Collision->SetBoxExtent(Extent); Collision->SetCollisionProfileName(TEXT("BlockAll"));
		Collision->RegisterComponent(); Actor->SetActorLocation(Location); Fixtures.Add(Actor); return Actor;
	}
	void Sample(const TCHAR* Name)
	{
		Test->AddInfo(FString::Printf(TEXT("%s samples=%d max_lag=%.3f"), Name, MotionSamples, MaxLag));
		Test->TestTrue(TEXT("Motion sampling covers multiple actual simulation updates"), MotionSamples > 2);
		Test->TestTrue(TEXT("Camera transform remains finite"), bMotionFinite);
		Test->TestTrue(TEXT("No rotation lag or unexpected flips during motion"), bMotionRotationStable);
		Test->TestTrue(TEXT("Follow lag remains within its configured distance bound"), bMotionLagBounded);
		bSampleMotion = false;
	}
	void StartSampling()
	{
		MotionSamples = 0; MaxLag = 0.0; bMotionFinite = bMotionRotationStable = bMotionLagBounded = true; bSampleMotion = true;
	}
	void Cleanup()
	{
		if (PC) { PC->FlushPressedKeys(); }
		for (auto& Fixture : Fixtures) { if (Fixture.IsValid()) { Fixture->Destroy(); } }
		Fixtures.Empty();
	}
	void BuildSteps()
	{
		Add(TEXT("Inspected Blueprint camera instance"), 1.0, [this]
		{
			TArray<UCameraComponent*> Cameras; Pawn->GetComponents(Cameras);
			TArray<USpringArmComponent*> Arms; Pawn->GetComponents(Arms);
			Test->TestEqual(TEXT("Exactly one original CameraComponent"), Cameras.Num(), 1);
			Test->TestEqual(TEXT("Exactly one original SpringArmComponent"), Arms.Num(), 1);
			Test->TestTrue(TEXT("Original camera remains attached to original arm"), Camera->GetAttachParent() == Boom);
			Test->TestEqual(TEXT("Template arm length preserved"), Boom->TargetArmLength, 400.0f);
			Test->TestTrue(TEXT("Arm uses pawn view rotation"), Boom->bUsePawnControlRotation != 0);
			Test->TestTrue(TEXT("Position smoothing enabled without rotation lag"), Boom->bEnableCameraLag && !Boom->bEnableCameraRotationLag);
			Test->TestTrue(TEXT("Lag substepping enabled"), Boom->bUseCameraLagSubstepping != 0);
			Test->TestEqual(TEXT("Configured smoothing speed reaches Blueprint instance"), Boom->CameraLagSpeed, 12.0f);
			Test->TestEqual(TEXT("Configured lag cap reaches Blueprint instance"), Boom->CameraLagMaxDistance, 75.0f);
			Test->TestTrue(TEXT("Original camera collision enabled on Camera channel"), Boom->bDoCollisionTest && Boom->ProbeChannel == ECC_Camera);
			Test->TestEqual(TEXT("Runtime CameraManager lower pitch limit"), PC->PlayerCameraManager->ViewPitchMin, -65.0f);
			Test->TestEqual(TEXT("Runtime CameraManager upper pitch limit"), PC->PlayerCameraManager->ViewPitchMax, 65.0f);
			Test->AddInfo(FString::Printf(TEXT("pawn=%s controller=%s manager=%s dodge=%s"), *Pawn->GetClass()->GetPathName(), *PC->GetClass()->GetPathName(), *PC->PlayerCameraManager->GetClass()->GetPathName(), *DodgeKey.ToString()));
			Axis(EKeys::MouseX, 20.0f); Axis(EKeys::MouseY, 10.0f);
		});
		Add(TEXT("Real paired mouse axes"), 0.15, [this]
		{
			const FRotator Rotation = PC->GetControlRotation();
			Test->TestTrue(TEXT("MouseX changes horizontal view"), FMath::Abs(FRotator::NormalizeAxis(Rotation.Yaw)) > 1.0f);
			Test->TestTrue(TEXT("MouseY changes vertical view"), FMath::Abs(FRotator::NormalizeAxis(Rotation.Pitch)) > 1.0f);
			Test->TestTrue(TEXT("Original follow camera follows mouse rotation immediately"), Camera->GetComponentRotation().Equals(Rotation, 0.1f));
			bPositiveMouseRaisesPitch = FRotator::NormalizeAxis(Rotation.Pitch) > 0.0f; PitchDrive = 20.0f;
		});
		Add(TEXT("First pitch endpoint"), 2.0, [this]
		{
			Test->TestTrue(TEXT("Repeated mouse input clamps at first configured endpoint"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch), bPositiveMouseRaisesPitch ? 65.0f : -65.0f, 0.1f));
			PitchDrive = -20.0f;
		});
		Add(TEXT("Opposite pitch endpoint"), 2.0, [this]
		{
			Test->TestTrue(TEXT("Opposite mouse input clamps at the other endpoint"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch), bPositiveMouseRaisesPitch ? -65.0f : 65.0f, 0.1f));
			PitchDrive = 0.0f;
		});
		// Drain the last simulated mouse delta before leveling the collision fixture view.
		Add(TEXT("Release mouse axes"), 0.15, [this] { PC->SetControlRotation(FRotator::ZeroRotator); });
		Add(TEXT("Movement start"), 0.15, [this]
		{
			Origin = Pawn->GetActorLocation();
			Key(EKeys::W, IE_Pressed); StartSampling();
		});
		Add(TEXT("Walking follow"), 0.8, [this]
		{
			Test->TestTrue(TEXT("Real W input moves the pawn"), Pawn->GetActorLocation().X > Origin.X + 100.0);
			Test->TestTrue(TEXT("Actual moving camera exhibits bounded smoothing"), LagDistance() > 2.0 && LagDistance() <= 77.0);
			Test->TestTrue(TEXT("Camera remains behind the moving pawn"), Camera->GetComponentLocation().X < Pawn->GetActorLocation().X - 350.0);
			Sample(TEXT("WALK_CAMERA")); Key(EKeys::W, IE_Released);
		});
		Add(TEXT("Follow converges after stop"), 0.7, [this]
		{
			Test->TestTrue(TEXT("Follow lag settles after input release"), LagDistance() < 1.0);
			Test->TestTrue(TEXT("Camera extends to unobstructed socket"), Camera->GetComponentLocation().Equals(Boom->GetUnfixedCameraPosition(), 0.1));
			WallFaceX = Pawn->GetActorLocation().X - 175.0;
			Wall = Box(Pawn->GetActorLocation() - FVector(200.0, 0.0, 0.0), FVector(25.0, 500.0, 500.0));
		});
		Add(TEXT("Camera wall sweep"), 0.25, [this]
		{
			Test->TestTrue(TEXT("SpringArm detects the blocking wall"), Boom->IsCollisionFixApplied());
			Test->TestTrue(TEXT("Collision retracts the camera substantially"), FVector::Distance(Camera->GetComponentLocation(), Boom->GetComponentLocation()) < 200.0);
			Test->TestTrue(TEXT("Camera sphere stays in front of wall face"), Camera->GetComponentLocation().X >= WallFaceX + Boom->ProbeSize - 1.0);
			Wall->Destroy();
		});
		Add(TEXT("Camera recovers from obstruction"), 0.3, [this]
		{
			Test->TestFalse(TEXT("Collision correction clears when obstruction is removed"), Boom->IsCollisionFixApplied());
			Test->TestTrue(TEXT("Camera returns to full follow length"), FMath::IsNearlyEqual(FVector::Distance(Camera->GetComponentLocation(), Boom->GetComponentLocation()), 400.0, 1.0));
			Origin = Pawn->GetActorLocation(); StartSampling(); Key(EKeys::SpaceBar, IE_Pressed);
		});
		Add(TEXT("Jump camera"), 0.18, [this]
		{
			Test->TestTrue(TEXT("Space causes actual jump"), Pawn->GetCharacterMovement()->IsFalling() && Pawn->GetActorLocation().Z > Origin.Z + 20.0);
			Test->TestTrue(TEXT("Camera follows jump upward"), Camera->GetComponentLocation().Z > Origin.Z + 5.0);
			Key(EKeys::SpaceBar, IE_Released);
		});
		Add(TEXT("Landing camera"), 1.3, [this]
		{
			Test->TestTrue(TEXT("Jump lands normally"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Sample(TEXT("JUMP_CAMERA")); Test->TestTrue(TEXT("Camera settles after landing"), LagDistance() < 1.0);
			Pawn->SetActorRotation(FRotator::ZeroRotator); Origin = Pawn->GetActorLocation(); StartSampling(); Key(DodgeKey, IE_Pressed);
		});
		Add(TEXT("Left Ctrl dodge camera"), 0.1, [this]
		{
			Test->TestEqual(TEXT("User-configured dodge key preserved"), DodgeKey, EKeys::LeftControl);
			Test->TestTrue(TEXT("Real Left Ctrl starts dodge"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Camera follows real dodge displacement"), Camera->GetComponentLocation().X > Origin.X - 350.0);
			Key(DodgeKey, IE_Released);
		});
		Add(TEXT("Dodge finish camera"), 0.3, [this]
		{
			Test->TestFalse(TEXT("Dodge finishes without camera state interference"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Dodge retains original distance"), Pawn->GetActorLocation().X - Origin.X > 405.0 && Pawn->GetActorLocation().X - Origin.X < 455.0);
			Sample(TEXT("DODGE_CAMERA"));
		});
		Add(TEXT("Dodge camera settles"), 0.6, [this]
		{
			Test->TestTrue(TEXT("Camera has no residual follow drift after dodge"), LagDistance() < 1.0);
			Test->AddInfo(TEXT("CAMERA_PIE_REGRESSION_FINISHED: real rendered PIE, actual Blueprint pawn, paired mouse input, pitch limits, bounded smoothing, wall sweep, jump and Left Ctrl dodge; no assets saved."));
		});
	}
	FAutomationTestBase* Test;
	AWastelandAdventureCharacter* Pawn = nullptr;
	APlayerController* PC = nullptr;
	USpringArmComponent* Boom = nullptr;
	UCameraComponent* Camera = nullptr;
	TArray<FStep> Steps;
	TArray<TWeakObjectPtr<AActor>> Fixtures;
	TWeakObjectPtr<AActor> Wall;
	FKey DodgeKey;
	FVector Origin;
	double WallFaceX = 0.0, MaxLag = 0.0, StepStartedAt = 0.0;
	double CreatedAt = FPlatformTime::Seconds();
	float PitchDrive = 0.0f;
	int32 Step = 0, MotionSamples = 0;
	bool bInitialized = false, bPositiveMouseRaisesPitch = false, bSampleMotion = false;
	bool bMotionFinite = true, bMotionRotationStable = true, bMotionLagBounded = true;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWastelandCameraPIETest, "WastelandAdventure.Character.CameraPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWastelandCameraPIETest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Stop existing PIE before running CameraPIE.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(WastelandAdventure::Tests::FCameraPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
