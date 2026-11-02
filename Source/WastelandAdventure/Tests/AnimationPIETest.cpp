// Inspect the actual Blueprint animation instance and evaluated bones in rendered PIE.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "WastelandAdventureCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"

namespace WastelandAdventure::Tests
{
class FAnimationPIECommand final : public IAutomationLatentCommand
{
public:
	explicit FAnimationPIECommand(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - CreatedAt > 180.0) { Test->AddError(TEXT("Animation PIE timed out.")); Cleanup(); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World) { return false; }
		Pawn = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Pawn || !PC) { return false; }
		Anim = Pawn->GetMesh()->GetAnimInstance();
		if (!Anim) { return false; }
		if (!bInitialized)
		{
			Main = Anim->GetStateMachineInstanceFromName(TEXT("Main States"));
			Locomotion = Anim->GetStateMachineInstanceFromName(TEXT("Locomotion"));
			for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
			{
				if (It->Struct == FAnimNode_BlendSpacePlayer::StaticStruct()) { BlendPlayer = It->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayer>(Anim); break; }
			}
			if (!Main || !Locomotion || !BlendPlayer) { Test->AddError(TEXT("Inspected template state machines / BlendSpace player are missing.")); return true; }
			Test->AddInfo(FString::Printf(TEXT("Actual animation=%s mesh=%s blendspace=%s"), *Anim->GetClass()->GetPathName(), *Pawn->GetMesh()->GetSkeletalMeshAsset()->GetPathName(), *BlendPlayer->GetBlendSpace()->GetPathName()));
			Floor = World->SpawnActor<AActor>();
			Floor->SetFlags(RF_Transient);
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor.Get());
			Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
			Box->SetBoxExtent(FVector(5000.0, 5000.0, 50.0)); Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
			Floor->SetActorLocation(FVector(20000.0, 0.0, -50.0));
			Pawn->TeleportTo(FVector(20000.0, 0.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator::ZeroRotator);
			PC->SetControlRotation(FRotator::ZeroRotator);
			FrameDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/Phase2_4"), FString::Printf(TEXT("Frames-%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
			IFileManager::Get().MakeDirectory(*FrameDirectory, true);
			BuildSteps(); bInitialized = true; StepStarted = World->GetTimeSeconds();
		}
		if (bTrackAir) { AirStates.Add(Main->GetCurrentStateName()); }
		if (bTrackBlend)
		{
			const FVector Weights = SampleWeights();
			bSawMixedBlend |= Weights.X > 0.02 && Weights.Y > 0.02;
		}
		if (bTrackDodge)
		{
			++DodgeSamples;
			MaxRootOffset = FMath::Max(MaxRootOffset, Pawn->GetMesh()->GetSocketTransform(TEXT("root"), RTS_Component).GetLocation().Size());
			MaxDodgePoseAngle = FMath::Max(MaxDodgePoseAngle, FMath::RadiansToDegrees(PoseBefore.AngularDistance(PelvisRotation())));
			bDodgeGrounded &= Pawn->GetCharacterMovement()->IsMovingOnGround();
		}
		if (Step >= Steps.Num()) { Cleanup(); return true; }
		if (World->GetTimeSeconds() - StepStarted < Steps[Step].Delay) { return false; }
		Test->AddInfo(FString::Printf(TEXT("%s main=%s locomotion=%s speed=%.2f animSpeed=%.2f filtered=%.2f slotWeight=%.3f"),
			*Steps[Step].Name, *Main->GetCurrentStateName().ToString(), *Locomotion->GetCurrentStateName().ToString(), Pawn->GetVelocity().Size2D(),
			GroundSpeed(), BlendPlayer->GetFilteredPosition().Y, Anim->GetSlotMontageLocalWeight(TEXT("DefaultSlot"))));
		Steps[Step].Run(); ++Step; StepStarted = World->GetTimeSeconds(); return false;
	}
private:
	struct FStep { FString Name; double Delay; TFunction<void()> Run; };
	void Add(const TCHAR* Name, double Delay, TFunction<void()> Run) { Steps.Add({Name, Delay, MoveTemp(Run)}); }
	void Key(const FKey& Value, EInputEvent Event) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value, Event, Event == IE_Released ? 0.0f : 1.0f)); }
	double GroundSpeed() const
	{
		const FDoubleProperty* Property = FindFProperty<FDoubleProperty>(Anim->GetClass(), TEXT("GroundSpeed"));
		return Property ? Property->GetPropertyValue_InContainer(Anim) : -1.0;
	}
	FQuat PelvisRotation() const { return Pawn->GetMesh()->GetSocketTransform(TEXT("pelvis"), RTS_Component).GetRotation(); }
	void Capture(const TCHAR* Name)
	{
		const FString Filename = FPaths::Combine(FrameDirectory, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Filename, false, false);
		Test->AddInfo(FString::Printf(TEXT("Requested actual rendered viewport frame: %s"), *Filename));
	}
	FVector SampleWeights() const
	{
		// Use the running node's filtered coordinates and the engine's own interpolation routine.
		TArray<FBlendSampleData> Samples; int32 Cache = -1; FVector Weights = FVector::ZeroVector;
		UBlendSpace* Space = BlendPlayer->GetBlendSpace();
		Space->GetSamplesFromBlendInput(BlendPlayer->GetFilteredPosition(), Samples, Cache, true);
		for (const FBlendSampleData& Sample : Samples)
		{
			const FBlendSample& Source = Space->GetBlendSample(Sample.SampleDataIndex);
			if (!Source.Animation) { continue; }
			const FString Name = Source.Animation->GetName();
			if (Name.Contains(TEXT("Walk"))) { Weights.X += Sample.TotalWeight; }
			if (Name.Contains(TEXT("Jog"))) { Weights.Y += Sample.TotalWeight; }
		}
		return Weights;
	}
	void CheckLocomotion(double ExpectedSpeed, bool bRun)
	{
		Test->TestEqual(TEXT("Grounded outer state"), Main->GetCurrentStateName(), FName(TEXT("Locomotion")));
		Test->TestEqual(TEXT("Movement animation state"), Locomotion->GetCurrentStateName(), FName(TEXT("Walk / Run")));
		Test->TestTrue(TEXT("Animation speed follows actual velocity"), FMath::Abs(GroundSpeed() - ExpectedSpeed) < 3.0);
		Test->TestTrue(TEXT("Running BlendSpace receives actual speed"), FMath::Abs(BlendPlayer->GetPosition().Y - ExpectedSpeed) < 3.0);
		const FVector Weights = SampleWeights();
		Test->AddInfo(FString::Printf(TEXT("Effective samples at runtime filtered coordinate: walk=%.3f jog=%.3f"), Weights.X, Weights.Y));
		Test->TestTrue(TEXT("Correct walk/run sample selected"), bRun ? Weights.Y > 0.95 : Weights.X > 0.95);
		Test->TestTrue(TEXT("Locomotion node is actually relevant"), BlendPlayer->GetCachedBlendWeight() > 0.5f);
	}
	void Cleanup() { if (PC) { PC->FlushPressedKeys(); } if (Floor.IsValid()) { Floor->Destroy(); } }
	void BuildSteps()
	{
		Add(TEXT("Idle"), 0.6, [this]
		{
			Test->TestEqual(TEXT("Original animation Blueprint reused"), Anim->GetClass()->GetName(), FString(TEXT("ABP_Unarmed_C")));
			Test->TestEqual(TEXT("Idle state evaluates"), Locomotion->GetCurrentStateName(), FName(TEXT("Idle")));
			Test->TestTrue(TEXT("Stationary animation speed is zero"), FMath::Abs(GroundSpeed()) < 0.1);
			Key(EKeys::W, IE_Pressed);
		});
		Add(TEXT("Walk"), 1.0, [this] { CheckLocomotion(500.0, false); Capture(TEXT("Walk")); WalkTime = BlendPlayer->GetCurrentAssetTime(); PoseBefore = PelvisRotation(); });
		Add(TEXT("Walk animation advances"), 0.12, [this]
		{
			Test->TestTrue(TEXT("Locomotion animation time advances"), FMath::Abs(BlendPlayer->GetCurrentAssetTime() - WalkTime) > 0.01f);
			Test->TestTrue(TEXT("Evaluated skeleton pose changes while walking"), PoseBefore.AngularDistance(PelvisRotation()) > 0.005);
			bTrackBlend = true; Key(EKeys::LeftShift, IE_Pressed);
		});
		Add(TEXT("Run"), 0.8, [this]
		{
			CheckLocomotion(750.0, true);
			Capture(TEXT("Run"));
			Test->TestTrue(TEXT("Walk/run transition includes mixed sample weights"), bSawMixedBlend);
			bTrackBlend = false; Key(EKeys::LeftShift, IE_Released);
		});
		Add(TEXT("Return to walk"), 0.8, [this] { CheckLocomotion(500.0, false); Key(EKeys::W, IE_Released); });
		Add(TEXT("Return to idle"), 0.7, [this]
		{
			Test->TestEqual(TEXT("Released input returns to Idle"), Locomotion->GetCurrentStateName(), FName(TEXT("Idle")));
			Test->TestTrue(TEXT("No residual movement"), Pawn->GetVelocity().Size2D() < 2.0);
			bTrackAir = true; Key(EKeys::SpaceBar, IE_Pressed);
		});
		Add(TEXT("Jump and airborne dodge rejection"), 0.18, [this]
		{
			Test->TestTrue(TEXT("Real jump leaves ground"), Pawn->GetCharacterMovement()->IsFalling());
			Capture(TEXT("Jump"));
			const FBoolProperty* Falling = FindFProperty<FBoolProperty>(Anim->GetClass(), TEXT("IsFalling"));
			if (Test->TestNotNull(TEXT("Existing IsFalling animation property"), Falling))
			{
				Test->TestTrue(TEXT("Animation reads falling from actual movement"), Falling->GetPropertyValue_InContainer(Anim));
			}
			Key(EKeys::SpaceBar, IE_Released); Key(EKeys::LeftControl, IE_Pressed);
		});
		Add(TEXT("Airborne rejection preserves jump animation"), 0.12, [this]
		{
			Test->TestFalse(TEXT("No airborne dodge movement"), Pawn->IsDodging());
			Test->TestNull(TEXT("No airborne dodge montage"), Anim->GetCurrentActiveMontage()); Key(EKeys::LeftControl, IE_Released);
		});
		Add(TEXT("Jump fall land recovery"), 2.0, [this]
		{
			bTrackAir = false;
			for (FName State : AirStates) { Test->AddInfo(FString::Printf(TEXT("Observed real air/landing state=%s"), *State.ToString())); }
			Test->TestTrue(TEXT("Jump state observed"), AirStates.Contains(TEXT("Jump")));
			Test->TestTrue(TEXT("Fall Loop state observed"), AirStates.Contains(TEXT("Fall Loop")));
			Test->TestTrue(TEXT("Land state observed"), AirStates.Contains(TEXT("Land")));
			Test->TestEqual(TEXT("Landing recovers locomotion"), Main->GetCurrentStateName(), FName(TEXT("Locomotion")));
			Test->TestTrue(TEXT("Landed capsule"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			PoseBefore = PelvisRotation(); Origin = Pawn->GetActorLocation(); bTrackDodge = true; Key(EKeys::LeftControl, IE_Pressed);
		});
		Add(TEXT("Dodge pose and movement"), 0.1, [this]
		{
			DodgeMontage = Anim->GetCurrentActiveMontage();
			Test->TestTrue(TEXT("Left Ctrl starts real dodge movement"), Pawn->IsDodging());
			if (Test->TestNotNull(TEXT("Dodge animation actually plays through existing slot"), DodgeMontage))
			{
				Test->TestTrue(TEXT("Transient dynamic montage"), DodgeMontage->IsDynamicMontage());
				Test->TestEqual(TEXT("Actual in-place asset reference"), DodgeMontage->GetFirstAnimReference()->GetName(), FString(TEXT("AS_Dodge_InPlace")));
				Test->TestFalse(TEXT("No asset root-motion contribution"), DodgeMontage->HasRootMotion());
				Test->TestTrue(TEXT("Animation duration matches movement window"), FMath::IsNearlyEqual(DodgeMontage->GetPlayLength() / Anim->Montage_GetPlayRate(DodgeMontage), 0.25f, 0.01f));
			}
			Test->TestTrue(TEXT("DefaultSlot contributes to final pose"), Anim->GetSlotMontageLocalWeight(TEXT("DefaultSlot")) > 0.5f);
			Capture(TEXT("Dodge"));
			Test->TestFalse(TEXT("Repeated request cannot restart animation"), Pawn->TryDodge());
			Test->TestTrue(TEXT("Montage instance preserved on rejected dodge"), Anim->GetCurrentActiveMontage() == DodgeMontage);
			Key(EKeys::LeftControl, IE_Released);
		});
		Add(TEXT("Dodge animation recovery"), 0.35, [this]
		{
			bTrackDodge = false;
			Test->AddInfo(FString::Printf(TEXT("DODGE_POSE samples=%d max_root_offset=%.3f max_pelvis_angle=%.3f displacement=%.3f"), DodgeSamples, MaxRootOffset, MaxDodgePoseAngle, FVector::Dist2D(Origin, Pawn->GetActorLocation())));
			Test->TestTrue(TEXT("Multiple evaluated dodge samples"), DodgeSamples > 3);
			Test->TestTrue(TEXT("Evaluated dodge skeleton is visibly different"), MaxDodgePoseAngle > 5.0);
			Test->TestTrue(TEXT("Mesh root stays with capsule"), MaxRootOffset < 2.0);
			Test->TestTrue(TEXT("Ground dodge does not add vertical launch"), bDodgeGrounded);
			Test->TestTrue(TEXT("No doubled displacement from animation"), FVector::Dist2D(Origin, Pawn->GetActorLocation()) > 405.0 && FVector::Dist2D(Origin, Pawn->GetActorLocation()) < 455.0);
			Test->TestFalse(TEXT("Dodge state ends"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Dodge slot fades out completely"), Anim->GetSlotMontageLocalWeight(TEXT("DefaultSlot")) < 0.01f);
			Test->TestNull(TEXT("No stuck montage"), Anim->GetCurrentActiveMontage());
			Key(EKeys::W, IE_Pressed);
		});
		Add(TEXT("Movement animation resumes after dodge"), 0.8, [this] { CheckLocomotion(500.0, false); Key(EKeys::W, IE_Released); });
		Add(TEXT("Animation cancellation start"), 0.5, [this] { Key(EKeys::LeftControl, IE_Pressed); });
		Add(TEXT("Cancel via input flush"), 0.08, [this]
		{
			Test->TestNotNull(TEXT("Dodge montage active before cancellation"), Anim->GetCurrentActiveMontage()); PC->FlushPressedKeys();
			Test->TestFalse(TEXT("Cancellation ends dodge state"), Pawn->IsDodging());
			Test->TestTrue(TEXT("Cancellation retains cooldown"), Pawn->GetDodgeCooldownRemaining() > 0.5f);
		});
		Add(TEXT("Cancellation recovery"), 0.3, [this]
		{
			Test->TestNull(TEXT("Interrupted dodge montage does not remain active"), Anim->GetCurrentActiveMontage());
			Test->TestTrue(TEXT("Interrupted slot has no residual weight"), Anim->GetSlotMontageLocalWeight(TEXT("DefaultSlot")) < 0.01f);
			Test->TestEqual(TEXT("Animation recovers Idle after interruption"), Locomotion->GetCurrentStateName(), FName(TEXT("Idle")));
			Test->AddInfo(TEXT("ANIMATION_PIE_REGRESSION_FINISHED: actual ABP state machines, live BlendSpace node, evaluated bones, real keyboard input, dynamic montage, interruption, and collision-safe displacement; fixtures only, no assets saved."));
		});
	}
	FAutomationTestBase* Test;
	AWastelandAdventureCharacter* Pawn = nullptr;
	APlayerController* PC = nullptr;
	UAnimInstance* Anim = nullptr;
	const FAnimNode_StateMachine* Main = nullptr;
	const FAnimNode_StateMachine* Locomotion = nullptr;
	FAnimNode_BlendSpacePlayer* BlendPlayer = nullptr;
	UAnimMontage* DodgeMontage = nullptr;
	TWeakObjectPtr<AActor> Floor;
	TArray<FStep> Steps;
	TSet<FName> AirStates;
	FString FrameDirectory;
	FQuat PoseBefore;
	FVector Origin;
	double CreatedAt = FPlatformTime::Seconds(), StepStarted = 0.0, MaxRootOffset = 0.0, MaxDodgePoseAngle = 0.0;
	float WalkTime = 0.0f;
	int32 Step = 0, DodgeSamples = 0;
	bool bInitialized = false, bTrackAir = false, bTrackBlend = false, bSawMixedBlend = false, bTrackDodge = false, bDodgeGrounded = true;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWastelandAnimationPIETest, "WastelandAdventure.Character.AnimationPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWastelandAnimationPIETest::RunTest(const FString& Parameters)
{
	if (GEditor->PlayWorld) { AddError(TEXT("Stop existing PIE before running AnimationPIE.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(WastelandAdventure::Tests::FAnimationPIECommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
