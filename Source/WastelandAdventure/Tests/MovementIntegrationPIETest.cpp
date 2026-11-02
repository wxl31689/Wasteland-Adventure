// Rendered PIE acceptance of combined input, animation, camera, and real level collision.
// Temporary fixtures and setup teleports are never saved and are not gameplay movement.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "WastelandAdventureCharacter.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"

namespace WastelandAdventure::Tests
{
class FMovementIntegrationCommand final : public IAutomationLatentCommand
{
public:
	FMovementIntegrationCommand(FAutomationTestBase* InTest, int32 InFrameCap) : Test(InTest), FrameCap(InFrameCap) {}
	virtual ~FMovementIntegrationCommand() override { Cleanup(); }
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds() - CreatedAt > 240.0) { Test->AddError(TEXT("Combined movement acceptance timed out.")); Cleanup(); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World) { return false; }
		Pawn = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Pawn.IsValid() || !PC.IsValid() || !Pawn->GetMesh()->GetAnimInstance()) { return false; }
		if (!bInitialized)
		{
			FpsVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
			if (!FpsVariable) { Test->AddError(TEXT("Frame cap console variable unavailable.")); return true; }
			PreviousFrameCap = FpsVariable->GetFloat(); FpsVariable->Set(FrameCap, ECVF_SetByConsole);
			Test->AddInfo(FString::Printf(TEXT("Combined acceptance cap=%d pawn=%s map=%s"), FrameCap, *Pawn->GetClass()->GetPathName(), *World->GetPathName()));
			if (!FindSlope(World) || !FindPlatform(World))
			{
				Test->AddError(TEXT("Could not identify the actual template ramp and a raised platform with a lower landing surface.")); Cleanup(); return true;
			}
			Box(FVector(10000.0, 0.0, -50.0), FVector(4000.0, 2000.0, 50.0));
			FrameDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/Phase2_5"), FString::Printf(TEXT("Frames-%s-%d"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), FrameCap));
			IFileManager::Get().MakeDirectory(*FrameDirectory, true);
			ResetAt(FVector(10000.0, 0.0, 0.0), FVector::ForwardVector);
			BuildSteps(); StepStarted = World->GetTimeSeconds(); bInitialized = true;
		}
		Observe(World);
		if (Step >= Steps.Num()) { Cleanup(); return true; }
		if (World->GetTimeSeconds() - StepStarted < Steps[Step].Delay) { return false; }
		Test->AddInfo(FString::Printf(TEXT("cap=%d %s position=%s speed=%.2f max=%.2f ground=%d dodge=%d cooldown=%.3f"), FrameCap,
			*Steps[Step].Name, *Pawn->GetActorLocation().ToString(), Pawn->GetVelocity().Size2D(), Movement()->MaxWalkSpeed,
			Movement()->IsMovingOnGround(), Pawn->IsDodging(), Pawn->GetDodgeCooldownRemaining()));
		Steps[Step].Run(); ++Step; StepStarted = World->GetTimeSeconds(); return false;
	}
private:
	struct FStep { FString Name; double Delay; TFunction<void()> Run; };
	void Add(const TCHAR* Name, double Delay, TFunction<void()> Run) { Steps.Add({Name, Delay, MoveTemp(Run)}); }
	UCharacterMovementComponent* Movement() const { return Pawn->GetCharacterMovement(); }
	UAnimInstance* Animation() const { return Pawn->GetMesh()->GetAnimInstance(); }
	void Key(const FKey& Value, EInputEvent Event) { PC->InputKey(FInputKeyEventArgs::CreateSimulated(Value, Event, Event == IE_Released ? 0.0f : 1.0f)); }
	void CheckSpeed(float Expected) { Test->TestEqual(TEXT("Held/released sprint selects the expected bounded speed"), Movement()->MaxWalkSpeed, Expected); }
	void Capture(const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(FPaths::Combine(FrameDirectory, FString(Name) + TEXT(".png")), false, false); }
	void ResetAt(const FVector& Surface, const FVector& Forward)
	{
		PC->FlushPressedKeys(); Movement()->StopMovementImmediately();
		PC->SetControlRotation(Forward.Rotation());
		const FVector Position = Surface + FVector(0.0, 0.0, Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 15.0);
		Test->TestTrue(TEXT("Collision-checked setup placement succeeds"), Pawn->TeleportTo(Position, Forward.Rotation()));
		Movement()->SetMovementMode(MOVE_Falling);
	}
	AActor* Box(const FVector& Center, const FVector& Extent)
	{
		FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
		AActor* Actor = Pawn->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Params);
		UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape);
		Shape->SetBoxExtent(Extent); Shape->SetCollisionProfileName(TEXT("BlockAll")); Shape->RegisterComponent();
		Actor->SetActorLocation(Center); Fixtures.Add(Actor); return Actor;
	}
	bool SurfaceAt(const FVector& XY, double ReferenceZ, FHitResult& Hit) const
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MovementAcceptanceSurface), false, Pawn.Get());
		return Pawn->GetWorld()->LineTraceSingleByChannel(Hit, FVector(XY.X, XY.Y, ReferenceZ + 500.0),
			FVector(XY.X, XY.Y, ReferenceZ - 2000.0), ECC_Visibility, Params);
	}
	bool FindSlope(UWorld* World)
	{
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() != World || !It->IsRegistered() || !It->GetStaticMesh() || It->GetStaticMesh()->GetName() != TEXT("SM_Ramp")) { continue; }
			const FBoxSphereBounds Bounds = It->Bounds;
			for (double X : {-0.35, 0.0, 0.35}) for (double Y : {-0.35, 0.0, 0.35})
			{
				FHitResult Hit, Start, End;
				const FVector Probe = Bounds.Origin + FVector(X * Bounds.BoxExtent.X, Y * Bounds.BoxExtent.Y, 0.0);
				if (!SurfaceAt(Probe, Bounds.Origin.Z + Bounds.BoxExtent.Z, Hit) || Hit.GetComponent() != *It || Hit.ImpactNormal.Z < 0.71 || Hit.ImpactNormal.Z > 0.98) { continue; }
				const FVector Uphill = FVector(-Hit.ImpactNormal.X, -Hit.ImpactNormal.Y, 0.0).GetSafeNormal();
				if (!SurfaceAt(Hit.ImpactPoint - Uphill * 110.0, Hit.ImpactPoint.Z, Start) || !SurfaceAt(Hit.ImpactPoint + Uphill * 110.0, Hit.ImpactPoint.Z, End)) { continue; }
				if (Start.GetComponent() != *It || End.GetComponent() != *It || End.ImpactPoint.Z < Start.ImpactPoint.Z + 40.0) { continue; }
				SlopeStart = Start.ImpactPoint; SlopeDirection = Uphill;
				Test->AddInfo(FString::Printf(TEXT("REAL_RAMP component=%s start=%s uphill=%s normal=%s"), *It->GetPathName(), *SlopeStart.ToString(), *Uphill.ToString(), *Hit.ImpactNormal.ToString()));
				return true;
			}
		}
		return false;
	}
	bool FindPlatform(UWorld* World)
	{
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() != World || !It->IsRegistered() || !It->GetStaticMesh()) { continue; }
			const FBoxSphereBounds Bounds = It->Bounds;
			if (Bounds.BoxExtent.X < 150.0 || Bounds.BoxExtent.Y < 150.0 || Bounds.BoxExtent.X > 800.0 || Bounds.BoxExtent.Y > 800.0) { continue; }
			FHitResult Center;
			if (!SurfaceAt(Bounds.Origin, Bounds.Origin.Z + Bounds.BoxExtent.Z, Center) || Center.GetComponent() != *It || Center.ImpactNormal.Z < 0.99) { continue; }
			for (FVector Direction : {FVector::ForwardVector, FVector::RightVector, -FVector::ForwardVector, -FVector::RightVector})
			{
				FVector Inside = Center.ImpactPoint;
				for (double Distance = 50.0; Distance < 1700.0; Distance += 50.0)
				{
					FHitResult Hit;
					FVector Outside = Center.ImpactPoint + Direction * Distance;
					if (!SurfaceAt(Outside, Center.ImpactPoint.Z, Hit)) { break; }
					if (Hit.GetComponent() == *It && Hit.ImpactNormal.Z > 0.99 && FMath::Abs(Hit.ImpactPoint.Z - Center.ImpactPoint.Z) < 3.0) { Inside = Outside; continue; }
					const double Drop = Center.ImpactPoint.Z - Hit.ImpactPoint.Z;
					if (Drop < 120.0 || Drop > 600.0) { break; }
					for (int32 Refine = 0; Refine < 8; ++Refine)
					{
						const FVector Mid = (Inside + Outside) * 0.5;
						FHitResult MidHit;
						if (SurfaceAt(Mid, Center.ImpactPoint.Z, MidHit) && MidHit.GetComponent() == *It && MidHit.ImpactNormal.Z > 0.99 && FMath::Abs(MidHit.ImpactPoint.Z - Center.ImpactPoint.Z) < 3.0) { Inside = Mid; } else { Outside = Mid; }
					}
					FHitResult Start;
					if (!SurfaceAt(Inside - Direction * 150.0, Center.ImpactPoint.Z, Start) || Start.GetComponent() != *It) { break; }
					EdgeStart = Start.ImpactPoint; EdgeDirection = Direction; EdgeDrop = Drop;
					Test->AddInfo(FString::Printf(TEXT("REAL_PLATFORM component=%s start=%s outward=%s drop=%.2f"), *It->GetPathName(), *EdgeStart.ToString(), *Direction.ToString(), Drop));
					return true;
				}
			}
		}
		return false;
	}
	void Observe(UWorld* World)
	{
		bFinite &= !Pawn->GetActorLocation().ContainsNaN() && !Pawn->GetVelocity().ContainsNaN() && !Pawn->GetFollowCamera()->GetComponentLocation().ContainsNaN();
		bBoundedVelocity &= Pawn->GetVelocity().Size2D() <= 1830.0;
		bNoAirDodge &= !(Pawn->IsDodging() && Movement()->IsFalling());
		if (bMeasureCadence && World->GetTimeSeconds() > LastWorldTime + 0.00001)
		{
			if (CadenceSamples == 0) { CadenceWallStart = FPlatformTime::Seconds(); }
			++CadenceSamples; SimulatedSeconds += World->GetDeltaSeconds(); CadenceWallEnd = FPlatformTime::Seconds();
		}
		LastWorldTime = World->GetTimeSeconds();
		if (bTrackStarts)
		{
			if (Pawn->IsDodging() && !bWasDodging)
			{
				DodgeStartTimes.Add(World->GetTimeSeconds() - (1.0 - Pawn->GetDodgeCooldownRemaining()));
				TrackedMontage = Animation()->GetCurrentActiveMontage();
			}
			if (Pawn->IsDodging() && bWasDodging && TrackedMontage && Animation()->GetCurrentActiveMontage()) { bNoMontageRestart &= TrackedMontage == Animation()->GetCurrentActiveMontage(); }
			bWasDodging = Pawn->IsDodging();
		}
		if (bTrackWall)
		{
			++WallSamples;
			bCameraOutsideWall &= Pawn->GetFollowCamera()->GetComponentLocation().X <= WallFace - Pawn->GetCameraBoom()->ProbeSize + 1.0;
			CameraCollisionSamples += Pawn->GetCameraBoom()->IsCollisionFixApplied() ? 1 : 0;
		}
		if (bTrackSlope) { ++SlopeSamples; bSlopeGrounded &= Movement()->IsMovingOnGround(); }
	}
	void Cleanup()
	{
		if (bCleaned) { return; } bCleaned = true;
		if (PC.IsValid()) { PC->FlushPressedKeys(); }
		for (auto& Fixture : Fixtures) { if (Fixture.IsValid()) { Fixture->Destroy(); } }
		if (FpsVariable) { FpsVariable->Set(PreviousFrameCap, ECVF_SetByConsole); }
	}
	void BuildSteps()
	{
		Add(TEXT("Walking starts from settled capsule"), 1.0, [this] { Test->TestTrue(TEXT("Fixture is grounded"), Movement()->IsMovingOnGround()); bMeasureCadence = true; Key(EKeys::W, IE_Pressed); });
		Add(TEXT("Walk to run"), 0.8, [this] { CheckSpeed(500.0f); Test->TestTrue(TEXT("Actual walking speed reaches 500"), FMath::IsNearlyEqual(Pawn->GetVelocity().Size2D(), 500.0f, 15.0f)); Key(EKeys::LeftShift, IE_Pressed); });
		Add(TEXT("Jump from actual running motion"), 0.6, [this] { CheckSpeed(750.0f); Test->TestTrue(TEXT("Actually moving at sprint speed before jumping"), Pawn->GetVelocity().Size2D() > 700.0); Origin = Pawn->GetActorLocation(); Key(EKeys::SpaceBar, IE_Pressed); });
		Add(TEXT("Running jump and airborne dodge attempt"), 0.15, [this]
		{
			Test->TestTrue(TEXT("Running jump has both horizontal movement and upward displacement"), Movement()->IsFalling() && Pawn->GetVelocity().Size2D() > 600.0 && Pawn->GetActorLocation().Z > Origin.Z + 20.0);
			Key(EKeys::SpaceBar, IE_Released); Key(EKeys::LeftControl, IE_Pressed);
		});
		Add(TEXT("Run to walk while airborne"), 0.1, [this] { Test->TestFalse(TEXT("Airborne input cannot start dodge"), Pawn->IsDodging()); Test->TestEqual(TEXT("Airborne rejection spends no cooldown"), Pawn->GetDodgeCooldownRemaining(), 0.0f); Key(EKeys::LeftControl, IE_Released); Key(EKeys::LeftShift, IE_Released); });
		Add(TEXT("Landing while moving"), 1.2, [this] { CheckSpeed(500.0f); Test->TestTrue(TEXT("Running jump lands normally"), Movement()->IsMovingOnGround()); Key(EKeys::W, IE_Released); });
		Add(TEXT("Dodge setup"), 0.4, [this] { Test->TestTrue(TEXT("Movement stops on release"), Pawn->GetVelocity().Size2D() < 2.0); ResetAt(FVector(10000.0, 0.0, 0.0), FVector::ForwardVector); });
		Add(TEXT("Dodge and buffered movement"), 0.5, [this] { Origin = Pawn->GetActorLocation(); Key(EKeys::LeftControl, IE_Pressed); });
		Add(TEXT("Movement key during dodge"), 0.09, [this] { Test->TestTrue(TEXT("Dodge is active"), Pawn->IsDodging()); Key(EKeys::LeftControl, IE_Released); Key(EKeys::W, IE_Pressed); });
		Add(TEXT("Immediate movement resumes"), 0.28, [this] { Test->TestFalse(TEXT("Dodge finishes"), Pawn->IsDodging()); Test->TestTrue(TEXT("Held W resumes immediately after dodge"), Pawn->GetVelocity().X > 30.0); CheckSpeed(500.0f); Key(EKeys::LeftShift, IE_Pressed); });
		Add(TEXT("Resumed movement can sprint"), 0.5, [this] { CheckSpeed(750.0f); Test->TestTrue(TEXT("Actual sprint resumes after dodge"), Pawn->GetVelocity().Size2D() > 700.0); PC->FlushPressedKeys(); CheckSpeed(500.0f); });
		Add(TEXT("Input flush stops held movement"), 0.4, [this] { Test->TestTrue(TEXT("No residual movement after input flush"), Pawn->GetVelocity().Size2D() < 2.0); ResetAt(FVector(10000.0, 0.0, 0.0), FVector::ForwardVector); });
		Add(TEXT("Rapid Shift setup"), 0.5, [] {});
		for (int32 Pulse = 0; Pulse < 12; ++Pulse)
		{
			Add(TEXT("Rapid Shift press"), 0.04, [this] { CheckSpeed(500.0f); Key(EKeys::LeftShift, IE_Pressed); });
			Add(TEXT("Rapid Shift release"), 0.04, [this] { CheckSpeed(750.0f); Key(EKeys::LeftShift, IE_Released); });
		}
		Add(TEXT("Rapid Shift recovery and Ctrl setup"), 0.08, [this] { CheckSpeed(500.0f); bTrackStarts = true; });
		for (int32 Pulse = 0; Pulse < 16; ++Pulse)
		{
			Add(TEXT("Rapid Left Ctrl press"), 0.04, [this] { Key(EKeys::LeftControl, IE_Pressed); });
			Add(TEXT("Rapid Left Ctrl release"), 0.04, [this] { Key(EKeys::LeftControl, IE_Released); });
		}
		Add(TEXT("Rapid Ctrl cooldown and pose recovery"), 1.2, [this]
		{
			bTrackStarts = false;
			Test->TestTrue(TEXT("Rapid Ctrl produces real accepted dodges"), DodgeStartTimes.Num() >= 2);
			for (int32 Index = 1; Index < DodgeStartTimes.Num(); ++Index) { Test->AddInfo(FString::Printf(TEXT("RAPID_CTRL interval=%.5f"), DodgeStartTimes[Index] - DodgeStartTimes[Index-1])); Test->TestTrue(TEXT("Rapid presses do not bypass one-second cooldown"), DodgeStartTimes[Index] - DodgeStartTimes[Index-1] >= 0.99); }
			Test->TestTrue(TEXT("Rapid presses do not restart an active montage"), bNoMontageRestart);
			Test->TestFalse(TEXT("Rapid Ctrl does not stick in dodge"), Pawn->IsDodging()); Test->TestNull(TEXT("Rapid Ctrl leaves no active montage"), Animation()->GetCurrentActiveMontage());
			Test->TestEqual(TEXT("Cooldown recovers after rapid input"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			ResetAt(FVector(10000.0, 0.0, 0.0), FVector::ForwardVector);
		});
		Add(TEXT("Simultaneous jump and dodge input"), 0.5, [this] { Key(EKeys::SpaceBar, IE_Pressed); Key(EKeys::LeftControl, IE_Pressed); });
		Add(TEXT("Simultaneous input resolves exclusively"), 0.1, [this] { Test->TestTrue(TEXT("Exactly one valid action wins simultaneous jump/dodge input"), Movement()->IsFalling() != Pawn->IsDodging()); Key(EKeys::SpaceBar, IE_Released); Key(EKeys::LeftControl, IE_Released); });
		Add(TEXT("Simultaneous input recovery"), 1.4, [this] { Test->TestTrue(TEXT("Simultaneous input recovers grounded movement"), Movement()->IsMovingOnGround() && !Pawn->IsDodging()); ResetAt(FVector(10000.0, 0.0, 0.0), FVector::ForwardVector); });
		Add(TEXT("Wall and opposing camera setup"), 0.5, [this]
		{
			Origin = Pawn->GetActorLocation(); PC->SetControlRotation(FRotator(0.0, 180.0, 0.0));
			WallFace = Origin.X + 225.0; Wall = Box(Origin + FVector(250.0, 0.0, 0.0), FVector(25.0, 500.0, 500.0)); bTrackWall = true;
		});
		Add(TEXT("Dodge toward wall with camera behind it"), 0.3, [this] { Key(EKeys::LeftControl, IE_Pressed); });
		Add(TEXT("Wall dodge collision result"), 0.4, [this]
		{
			const double Radius = Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
			Test->TestTrue(TEXT("Capsule is stopped on the correct side of the wall"), Pawn->GetActorLocation().X <= WallFace - Radius + 2.0 && Pawn->GetActorLocation().X > Origin.X + 100.0);
			Test->TestTrue(TEXT("Wall dodge ends without sliding or stuck animation"), !Pawn->IsDodging() && Pawn->GetVelocity().Size2D() < 2.0 && Animation()->GetSlotMontageLocalWeight(TEXT("DefaultSlot")) < 0.01f);
			Test->TestTrue(TEXT("Wall collision retains cooldown"), Pawn->GetDodgeCooldownRemaining() > 0.3f);
			Test->TestTrue(TEXT("Camera sphere remains outside the wall throughout approach"), bCameraOutsideWall && CameraCollisionSamples > 2);
			Key(EKeys::LeftControl, IE_Released); Key(EKeys::W, IE_Pressed);
		});
		Add(TEXT("Move away from wall"), 1.4, [this] { Test->TestFalse(TEXT("Camera extends after moving away from obstruction"), Pawn->GetCameraBoom()->IsCollisionFixApplied()); Key(EKeys::W, IE_Released); });
		Add(TEXT("Wall camera recovery"), 0.5, [this]
		{
			Test->TestTrue(TEXT("Full 400 cm arm recovers after stopping"), FMath::IsNearlyEqual(FVector::Distance(Pawn->GetFollowCamera()->GetComponentLocation(), Pawn->GetCameraBoom()->GetComponentLocation()), 400.0, 1.0));
			Test->AddInfo(FString::Printf(TEXT("WALL_CAMERA samples=%d collision_samples=%d outside_wall=%d"), WallSamples, CameraCollisionSamples, bCameraOutsideWall));
			bTrackWall = false; Wall->Destroy(); PC->SetControlRotation(FRotator::ZeroRotator);
			Key(EKeys::LeftShift, IE_Pressed); Key(EKeys::LeftControl, IE_Pressed);
		});
		Add(TEXT("Cancel simultaneous held sprint and dodge"), 0.1, [this]
		{
			Test->TestTrue(TEXT("Dodge is active before flush"), Pawn->IsDodging()); PC->FlushPressedKeys(); CheckSpeed(500.0f);
			Test->TestFalse(TEXT("Flush immediately cancels dodge"), Pawn->IsDodging()); Test->TestTrue(TEXT("Flush preserves spent cooldown"), Pawn->GetDodgeCooldownRemaining() > 0.5f);
		});
		Add(TEXT("Input cancellation final recovery"), 1.2, [this]
		{
			Test->TestNull(TEXT("No remaining montage after input cancellation"), Animation()->GetCurrentActiveMontage()); Test->TestTrue(TEXT("No remaining velocity"), Pawn->GetVelocity().Size2D() < 2.0);
			bMeasureCadence = false;
			Test->AddInfo(FString::Printf(TEXT("CADENCE cap=%d samples=%d simulation_fps=%.3f wall_fps=%.3f"), FrameCap, CadenceSamples, CadenceSamples / SimulatedSeconds, (CadenceSamples-1) / (CadenceWallEnd-CadenceWallStart)));
			Test->TestTrue(TEXT("Frame cadence sampled over sustained actual movement"), CadenceSamples > 60);
			ResetAt(SlopeStart, SlopeDirection);
		});
		Add(TEXT("Actual template ramp grounded"), 0.6, [this] { Test->TestTrue(TEXT("Capsule stands on actual ramp"), Movement()->IsMovingOnGround() && Movement()->CurrentFloor.HitResult.ImpactNormal.Z < 0.99); Origin = Pawn->GetActorLocation(); bTrackSlope = true; Key(EKeys::W, IE_Pressed); });
		Add(TEXT("Actual ramp uphill"), 0.35, [this]
		{
			const FVector Delta = Pawn->GetActorLocation() - Origin;
			Test->TestTrue(TEXT("W climbs the actual template ramp"), FVector::DotProduct(Delta, SlopeDirection) > 60.0 && Delta.Z > 20.0); Capture(TEXT("ActualRampUphill")); Key(EKeys::W, IE_Released);
		});
		Add(TEXT("Ramp release does not slide"), 0.35, [this] { Test->TestTrue(TEXT("Release stops on slope"), Pawn->GetVelocity().Size2D() < 2.0); Origin = Pawn->GetActorLocation(); Key(EKeys::S, IE_Pressed); });
		Add(TEXT("Actual ramp downhill"), 0.35, [this] { const FVector Delta = Pawn->GetActorLocation() - Origin; Test->TestTrue(TEXT("S descends the actual template ramp"), FVector::DotProduct(Delta, SlopeDirection) < -60.0 && Delta.Z < -20.0); Key(EKeys::S, IE_Released); });
		Add(TEXT("Ramp recovery and platform setup"), 0.35, [this]
		{
			Test->AddInfo(FString::Printf(TEXT("RAMP_TRAVERSAL samples=%d grounded=%d"), SlopeSamples, bSlopeGrounded)); Test->TestTrue(TEXT("Ramp traversal keeps capsule grounded"), bSlopeGrounded && SlopeSamples > 8);
			bTrackSlope = false; ResetAt(EdgeStart, EdgeDirection);
		});
		Add(TEXT("Actual platform edge dodge start"), 0.6, [this] { Test->TestTrue(TEXT("Actual platform is grounded before edge traversal"), Movement()->IsMovingOnGround()); Origin = Pawn->GetActorLocation(); Key(EKeys::LeftShift, IE_Pressed); Key(EKeys::LeftControl, IE_Pressed); });
		Add(TEXT("Actual edge departure"), 0.35, [this]
		{
			const double Distance = FVector::Dist2D(Origin, Pawn->GetActorLocation());
			Test->AddInfo(FString::Printf(TEXT("REAL_EDGE horizontal_distance=%.3f height_change=%.3f drop=%.3f"), Distance, Pawn->GetActorLocation().Z-Origin.Z, EdgeDrop));
			Test->TestTrue(TEXT("Leaving actual platform hands control to falling"), Movement()->IsFalling() && !Pawn->IsDodging());
			Test->TestTrue(TEXT("Edge stops the full airborne dodge distance"), Distance < 350.0);
			Test->TestTrue(TEXT("Edge departure fades out dodge animation"), Animation()->GetSlotMontageLocalWeight(TEXT("DefaultSlot")) < 0.01f); Capture(TEXT("ActualPlatformEdge"));
			Key(EKeys::LeftControl, IE_Released); Key(EKeys::LeftShift, IE_Released);
		});
		Add(TEXT("Actual edge landing and final invariants"), 1.5, [this]
		{
			Test->TestTrue(TEXT("Real platform fall lands on actual level collision"), Movement()->IsMovingOnGround()); CheckSpeed(500.0f);
			Test->TestTrue(TEXT("No residual movement after edge landing"), Pawn->GetVelocity().Size2D() < 2.0); Test->TestEqual(TEXT("Edge cooldown recovers"), Pawn->GetDodgeCooldownRemaining(), 0.0f);
			Test->TestTrue(TEXT("All observed transforms and velocities remain finite"), bFinite); Test->TestTrue(TEXT("Combined inputs never multiply velocity beyond dodge speed"), bBoundedVelocity); Test->TestTrue(TEXT("Dodge and falling never remain active together"), bNoAirDodge);
			Test->AddInfo(FString::Printf(TEXT("MOVEMENT_INTEGRATION_FINISHED cap=%d rapid_shift_cycles=12 rapid_ctrl_presses=16 accepted_dodges=%d actual_ramp_and_platform=true no_assets_saved=true"), FrameCap, DodgeStartTimes.Num()));
		});
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<AWastelandAdventureCharacter> Pawn;
	TWeakObjectPtr<APlayerController> PC;
	TArray<TWeakObjectPtr<AActor>> Fixtures;
	TWeakObjectPtr<AActor> Wall;
	UAnimMontage* TrackedMontage = nullptr;
	IConsoleVariable* FpsVariable = nullptr;
	TArray<FStep> Steps;
	TArray<double> DodgeStartTimes;
	FString FrameDirectory;
	FVector Origin, SlopeStart, SlopeDirection, EdgeStart, EdgeDirection;
	double CreatedAt = FPlatformTime::Seconds(), StepStarted = 0.0, WallFace = 0.0, EdgeDrop = 0.0;
	double SimulatedSeconds = 0.0, LastWorldTime = 0.0, CadenceWallStart = 0.0, CadenceWallEnd = 0.0;
	float PreviousFrameCap = 0.0f;
	int32 FrameCap, Step = 0, CadenceSamples = 0, WallSamples = 0, CameraCollisionSamples = 0, SlopeSamples = 0;
	bool bInitialized = false, bCleaned = false, bMeasureCadence = false, bTrackStarts = false, bWasDodging = false;
	bool bNoMontageRestart = true, bTrackWall = false, bCameraOutsideWall = true, bTrackSlope = false, bSlopeGrounded = true;
	bool bFinite = true, bBoundedVelocity = true, bNoAirDodge = true;
};

static bool RunMovementIntegration(FAutomationTestBase* Test, int32 FrameCap)
{
	if (GEditor->PlayWorld) { Test->AddError(TEXT("Stop existing PIE before combined acceptance.")); return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FMovementIntegrationCommand(Test, FrameCap));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementIntegration30Test, "WastelandAdventure.Character.Integration30FPS", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMovementIntegration30Test::RunTest(const FString& Parameters) { return WastelandAdventure::Tests::RunMovementIntegration(this, 30); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementIntegration60Test, "WastelandAdventure.Character.Integration60FPS", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMovementIntegration60Test::RunTest(const FString& Parameters) { return WastelandAdventure::Tests::RunMovementIntegration(this, 60); }
#endif
