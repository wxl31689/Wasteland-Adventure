// Runs only in development editor builds. No game assets are created or saved.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "WastelandAdventureCharacter.h"
#include "WastelandAdventurePlayerController.h"
#include "Components/BoxComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

namespace WastelandAdventure::Tests
{
class FSprintPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FSprintPIECommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World || !UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			if (FPlatformTime::Seconds() - StartedAt > 60.0)
			{
				Test->AddError(TEXT("PIE failed to create the real player within 60 seconds."));
				return true;
			}
			return false;
		}

		Character = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		Controller = Cast<AWastelandAdventurePlayerController>(UGameplayStatics::GetPlayerController(World, 0));
		if (!Character || !Controller)
		{
			Test->AddError(TEXT("The loaded map is not using the inspected project Character/PlayerController."));
			return true;
		}
		if (FPlatformTime::Seconds() - StartedAt > 150.0)
		{
			Test->AddError(TEXT("PIE input/physics regression timed out."));
			Controller->FlushPressedKeys();
			if (Wall.IsValid()) { Wall->Destroy(); }
			return true;
		}

		const double Now = World->GetTimeSeconds();
		if (Stage == 0)
		{
			if (!Character->GetCharacterMovement()->IsMovingOnGround()) { return false; }
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Origin = Character->GetActorLocation();
			Test->AddInfo(FString::Printf(TEXT("PIE pawn=%s controller=%s origin=%s"),
				*Character->GetClass()->GetPathName(), *Controller->GetClass()->GetPathName(), *Origin.ToString()));
			CheckSpeed(500.0f, TEXT("Initial normal speed"));
			Key(EKeys::W, IE_Pressed);
			Advance(Now);
			return false;
		}

		if (Now - StageStartedAt < Duration()) { return false; }
		const FVector Position = Character->GetActorLocation();
		const float Speed = Character->GetVelocity().Size2D();
		Test->AddInfo(FString::Printf(TEXT("stage=%d position=%s speed=%.2f max=%.2f"),
			Stage, *Position.ToString(), Speed, Character->GetCharacterMovement()->MaxWalkSpeed));
		switch (Stage)
		{
		case 1:
			Test->TestTrue(TEXT("W moves forward through the existing IMC"), Position.X > Origin.X + 50.0);
			Test->TestTrue(TEXT("Walking reaches the original 500 cm/s"), Speed > 475.0f && Speed < 525.0f);
			Key(EKeys::LeftShift, IE_Pressed);
			break;
		case 2:
			CheckSpeed(750.0f, TEXT("Shift Started selects sprint speed"));
			Test->TestTrue(TEXT("Held Shift + W actually moves faster"), Speed > 700.0f && Speed < 780.0f);
			Key(EKeys::W, IE_Released);
			Key(EKeys::D, IE_Pressed);
			TurnOrigin = Position;
			break;
		case 3:
			Test->TestTrue(TEXT("Sprint permits changing direction with D"), Position.Y > TurnOrigin.Y + 50.0);
			CheckSpeed(750.0f, TEXT("Direction changes do not accumulate speed"));
			Key(EKeys::LeftShift, IE_Released);
			break;
		case 4:
			CheckSpeed(500.0f, TEXT("Shift Completed restores normal speed"));
			Test->TestTrue(TEXT("Release returns actual velocity to normal"), Speed <= 525.0f);
			Key(EKeys::D, IE_Released);
			Key(EKeys::A, IE_Pressed);
			TurnOrigin = Position;
			break;
		case 5:
			Test->TestTrue(TEXT("A moves left"), Position.Y < TurnOrigin.Y - 30.0);
			Key(EKeys::A, IE_Released);
			Key(EKeys::S, IE_Pressed);
			TurnOrigin = Position;
			break;
		case 6:
			Test->TestTrue(TEXT("S moves backward"), Position.X < TurnOrigin.X - 30.0);
			Key(EKeys::S, IE_Released);
			break;
		case 7:
			Test->TestTrue(TEXT("Released movement brakes to a stop without sliding"), Speed < 2.0f);
			Key(EKeys::LeftShift, IE_Pressed);
			Key(EKeys::SpaceBar, IE_Pressed);
			JumpOrigin = Position;
			break;
		case 8:
			Test->TestTrue(TEXT("Space still jumps while sprint is held"),
				Position.Z > JumpOrigin.Z + 20.0 && Character->GetCharacterMovement()->IsFalling());
			Key(EKeys::SpaceBar, IE_Released);
			Key(EKeys::LeftShift, IE_Released);
			break;
		case 9:
			Test->TestTrue(TEXT("Jump lands on the real template collision"), Character->GetCharacterMovement()->IsMovingOnGround());
			CheckSpeed(500.0f, TEXT("Airborne Shift release does not leave sprint stuck"));
			Key(EKeys::LeftShift, IE_Pressed);
			break;
		case 10:
			CheckSpeed(750.0f, TEXT("Sprint can be started again"));
			Controller->FlushPressedKeys();
			CheckSpeed(500.0f, TEXT("Input flush/focus-loss path immediately resets sprint"));
			Key(EKeys::LeftShift, IE_Released);
			break;
		case 11:
			// The first two rapid cycles are real pressed/released events on separate simulation frames.
			Key(EKeys::LeftShift, IE_Pressed);
			break;
		case 12:
			CheckSpeed(750.0f, TEXT("Rapid Shift press remains bounded"));
			Key(EKeys::LeftShift, IE_Released);
			break;
		case 13:
			CheckSpeed(500.0f, TEXT("Rapid Shift release restores speed"));
			Key(EKeys::LeftShift, IE_Pressed);
			break;
		case 14:
			CheckSpeed(750.0f, TEXT("Repeated Shift press does not multiply speed"));
			Key(EKeys::LeftShift, IE_Released);
			break;
		case 15:
			CheckSpeed(500.0f, TEXT("Repeated Shift release is safe"));
			CameraBefore = Controller->GetControlRotation();
			// PlayerInput assembles Mouse2D from its physical one-dimensional paired axes.
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, 20.0f));
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseY, IE_Axis, 10.0f));
			break;
		case 16:
			Test->AddInfo(FString::Printf(TEXT("Camera before=%s after=%s"), *CameraBefore.ToString(), *Controller->GetControlRotation().ToString()));
			Test->TestTrue(TEXT("Existing Mouse2D mapping still changes camera yaw"),
				!FMath::IsNearlyEqual(Controller->GetControlRotation().Yaw, CameraBefore.Yaw, 0.01f));
			Test->TestTrue(TEXT("Existing Mouse2D mapping still changes camera pitch"),
				!FMath::IsNearlyEqual(Controller->GetControlRotation().Pitch, CameraBefore.Pitch, 0.01f));
			Controller->SetControlRotation(FRotator::ZeroRotator);
			{
				FActorSpawnParameters Params;
				Params.ObjectFlags |= RF_Transient;
				Wall = World->SpawnActor<AActor>(AActor::StaticClass(), Params);
				UBoxComponent* Box = NewObject<UBoxComponent>(Wall.Get());
				Wall->SetRootComponent(Box);
				Wall->AddInstanceComponent(Box);
				Box->SetBoxExtent(FVector(25.0, 500.0, 500.0));
				Box->SetCollisionProfileName(TEXT("BlockAll"));
				Box->RegisterComponent();
				WallX = Position.X + 450.0;
				Wall->SetActorLocation(FVector(WallX, Position.Y, Position.Z + 100.0));
			}
			Key(EKeys::LeftShift, IE_Pressed);
			Key(EKeys::W, IE_Pressed);
			break;
		case 17:
			Test->TestTrue(TEXT("Sprint collision blocks movement through a wall"),
				Position.X > WallX - 150.0 && Position.X < WallX - 60.0 && Speed < 5.0f);
			Test->TestTrue(TEXT("The capsule remains above the real floor"), Character->GetCharacterMovement()->IsMovingOnGround());
			Controller->FlushPressedKeys();
			Wall->Destroy();
			break;
		case 18:
			CheckSpeed(500.0f, TEXT("Collision test restores normal state"));
			Test->TestTrue(TEXT("Collision test leaves no sliding"), Speed < 2.0f);
			Test->AddInfo(TEXT("SPRINT_PIE_REGRESSION_FINISHED: real PIE world, Enhanced Input keys, movement physics, and temporary collision wall; no assets saved."));
			return true;
		default:
			return true;
		}
		Advance(Now);
		return false;
	}

private:
	void Key(const FKey& InputKey, EInputEvent Event)
	{
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(InputKey, Event, Event == IE_Released ? 0.0f : 1.0f));
	}
	void CheckSpeed(float Expected, const TCHAR* Description)
	{
		Test->TestEqual(Description, Character->GetCharacterMovement()->MaxWalkSpeed, Expected);
	}
	void Advance(double Now) { ++Stage; StageStartedAt = Now; }
	double Duration() const
	{
		if (Stage >= 10 && Stage <= 16) { return 0.12; }
		if (Stage == 8) { return 0.18; }
		if (Stage == 9) { return 1.3; }
		if (Stage == 17) { return 1.6; }
		if (Stage == 1 || Stage == 2 || Stage == 7) { return 0.8; }
		return 0.5;
	}

	FAutomationTestBase* Test;
	AWastelandAdventureCharacter* Character = nullptr;
	AWastelandAdventurePlayerController* Controller = nullptr;
	TWeakObjectPtr<AActor> Wall;
	FVector Origin, TurnOrigin, JumpOrigin;
	FRotator CameraBefore;
	double WallX = 0.0;
	double StageStartedAt = 0.0;
	double StartedAt = FPlatformTime::Seconds();
	int32 Stage = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWastelandSprintPIETest, "WastelandAdventure.Character.SprintPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWastelandSprintPIETest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld)
	{
		AddError(TEXT("Stop existing PIE before running this test. User play sessions are never replaced."));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(WastelandAdventure::Tests::FSprintPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
