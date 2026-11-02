#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Components/WAStaminaComponent.h"
#include "Tests/WAStaminaTestObserver.h"
#include "WastelandAdventureCharacter.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/StrongObjectPtr.h"

namespace WastelandAdventure::Tests
{
class FStaminaPIECommand final : public IAutomationLatentCommand
{
public:
	FStaminaPIECommand(FAutomationTestBase* InTest, int32 InCap) : Test(InTest), Cap(InCap) {}
	virtual ~FStaminaPIECommand() override { Cleanup(); }
	virtual bool Update() override
	{
		if (FPlatformTime::Seconds()-CreatedAt > 300.0) { Test->AddError(TEXT("Stamina PIE timed out")); Cleanup(); return true; }
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		if (!World) { return false; }
		Pawn = Cast<AWastelandAdventureCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
		PC = UGameplayStatics::GetPlayerController(World, 0);
		if (!Pawn.IsValid() || !PC.IsValid()) { return false; }
		if (!bInitialized)
		{
			S = Pawn->GetStaminaComponent();
			if (!Test->TestNotNull(TEXT("Real Blueprint pawn has native stamina component"), S.Get())) { return true; }
			Fps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
			if (!Fps) { Test->AddError(TEXT("No frame cap variable")); return true; }
			PreviousCap = Fps->GetFloat(); Fps->Set(Cap, ECVF_SetByConsole);
			O.Reset(NewObject<UWAStaminaTestObserver>());
			S->OnStaminaChanged.AddUniqueDynamic(O.Get(), &UWAStaminaTestObserver::Changed);
			S->OnStaminaDepleted.AddUniqueDynamic(O.Get(), &UWAStaminaTestObserver::Depleted);
			S->OnStaminaRecovered.AddUniqueDynamic(O.Get(), &UWAStaminaTestObserver::Recovered);
			Box(FVector(50000.0,0.0,-50.0), FVector(12000.0,4000.0,50.0));
			ResetFixture(); BuildSteps(); StepStarted = Now(); bInitialized = true;
			Test->AddInfo(FString::Printf(TEXT("STAMINA_PIE_ACTUAL cap=%d pawn=%s component=%s"),Cap,*Pawn->GetClass()->GetPathName(),*S->GetPathName()));
		}
		++Frames; Simulated += World->GetDeltaSeconds();
		if (bTrackRapid)
		{
			const bool bActive = Pawn->IsDodging();
			if (bActive && !bWasDodging) { Starts.Add(Now()-(1.0-Pawn->GetDodgeCooldownRemaining())); }
			bWasDodging = bActive;
		}
		if (Step >= Steps.Num()) { Cleanup(); return true; }
		if (Now()-StepStarted < Steps[Step].Delay) { return false; }
		Test->AddInfo(FString::Printf(TEXT("cap=%d %s stamina=%.4f speed=%.2f max=%.1f sprint=%d dodge=%d recovering=%d delay=%.3f tick=%d"),
			Cap,*Steps[Step].Name,S->GetCurrentStamina(),Pawn->GetVelocity().Size2D(),Movement()->MaxWalkSpeed,S->IsSprinting(),Pawn->IsDodging(),S->IsRecovering(),S->GetRecoveryDelayRemaining(),S->IsComponentTickEnabled()));
		Steps[Step].Run(); ++Step; StepStarted = Now(); return false;
	}
private:
	struct FStep { FString Name; double Delay; TFunction<void()> Run; };
	void Add(const TCHAR* Name,double Delay,TFunction<void()> Run){ Steps.Add({Name,Delay,MoveTemp(Run)}); }
	double Now() const { return Pawn->GetWorld()->GetTimeSeconds(); }
	UCharacterMovementComponent* Movement() const { return Pawn->GetCharacterMovement(); }
	void Key(const FKey& K,EInputEvent E){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,E,E==IE_Released?0.0f:1.0f));}
	void Check(float Expected,float Tolerance=0.01f){Test->TestTrue(TEXT("Expected bounded runtime stamina"),FMath::Abs(S->GetCurrentStamina()-Expected)<=Tolerance);}
	void Sample(){SampleTime=Now();SampleValue=S->GetCurrentStamina();}
	void ResetFixture()
	{
		PC->FlushPressedKeys(); Pawn->CancelDodge(); Movement()->StopMovementImmediately(); S->ResetStamina();
		PC->SetControlRotation(FRotator::ZeroRotator);
		Test->TestTrue(TEXT("Fixture setup placement succeeds"),Pawn->TeleportTo(FVector(50000.0,0.0,Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2.0),FRotator::ZeroRotator));
	}
	AActor* Box(const FVector& Location,const FVector& Extent)
	{
		FActorSpawnParameters P;P.ObjectFlags|=RF_Transient;
		AActor* A=Pawn->GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),P);
		UBoxComponent* B=NewObject<UBoxComponent>(A);A->SetRootComponent(B);A->AddInstanceComponent(B);
		B->SetBoxExtent(Extent);B->SetCollisionProfileName(TEXT("BlockAll"));B->RegisterComponent();A->SetActorLocation(Location);Fixtures.Add(A);return A;
	}
	void BuildSteps()
	{
		Add(TEXT("Initial full idle"),0.5,[this]{
			Check(100);TArray<UWAStaminaComponent*> C;Pawn->GetComponents(C);
			Test->TestEqual(TEXT("One native stamina authority"),C.Num(),1);
			Test->TestFalse(TEXT("Full idle does not tick"),S->IsComponentTickEnabled());
			Test->TestFalse(TEXT("No replication"),S->GetIsReplicated());
			Test->TestTrue(TEXT("Consume 25 succeeds"),S->TryConsumeStamina(25));Check(75);
			Test->TestFalse(TEXT("Overspend is refused"),S->TryConsumeStamina(76));Check(75);Sample();
		});
		Add(TEXT("Recovery delay blocks early regeneration"),0.85,[this]{
			Check(75);Test->TestFalse(TEXT("Waiting does not tick"),S->IsComponentTickEnabled());
			Test->TestTrue(TEXT("World timer still waiting"),S->GetRecoveryDelayRemaining()>0);});
		Add(TEXT("Recovery begins after delay"),0.5,[this]{
			Test->TestTrue(TEXT("Automatic recovery has started"),S->IsRecovering()&&S->IsComponentTickEnabled()&&S->GetCurrentStamina()>75);Sample();});
		Add(TEXT("Recovery rate 15 per second"),1.0,[this]{
			const double Actual=S->GetCurrentStamina()-SampleValue,Expected=15.0*(Now()-SampleTime);
			Test->AddInfo(FString::Printf(TEXT("STAMINA_REGEN_RATE cap=%d actual=%.4f expected=%.4f dt=%.4f"),Cap,Actual,Expected,Now()-SampleTime));
			Test->TestTrue(TEXT("Measured regeneration is 15 per second"),FMath::Abs(Actual-Expected)<0.8);
			S->RestoreStamina(1000);Check(100);Test->TestFalse(TEXT("Full refill stops Tick"),S->IsComponentTickEnabled());
			Key(EKeys::W,IE_Pressed);Key(EKeys::LeftShift,IE_Pressed);
		});
		Add(TEXT("Real Shift sprint starts"),0.6,[this]{
			Test->TestTrue(TEXT("Actual W plus Shift reaches 750"),Pawn->GetVelocity().Size2D()>700&&S->IsSprinting());
			Test->TestTrue(TEXT("Sprint has drained real stamina"),S->GetCurrentStamina()<92);Sample();});
		Add(TEXT("Sprint drain rate 20 per second"),1.0,[this]{
			const double Actual=SampleValue-S->GetCurrentStamina(),Expected=20.0*(Now()-SampleTime);
			Test->AddInfo(FString::Printf(TEXT("STAMINA_DRAIN_RATE cap=%d actual=%.4f expected=%.4f dt=%.4f"),Cap,Actual,Expected,Now()-SampleTime));
			Test->TestTrue(TEXT("Measured sprint drain is 20 per second"),FMath::Abs(Actual-Expected)<0.8);
			Test->TestFalse(TEXT("No recovery during sprint"),S->IsRecovering());Key(EKeys::LeftShift,IE_Released);Key(EKeys::W,IE_Released);
		});
		Add(TEXT("Release stops continuous drain"),0.15,[this]{
			Test->TestFalse(TEXT("Released Shift stops resource sprint"),S->IsSprinting());Test->TestEqual(TEXT("Released Shift restores 500"),Movement()->MaxWalkSpeed,500.0f);Sample();});
		Add(TEXT("Stopped sprint delay"),0.65,[this]{Check(SampleValue);Test->TestFalse(TEXT("No Tick during delay"),S->IsComponentTickEnabled());});
		Add(TEXT("Stopped sprint recovers"),0.6,[this]{Test->TestTrue(TEXT("Sprint recovery after one second"),S->GetCurrentStamina()>SampleValue&&S->IsRecovering());ResetFixture();});
		Add(TEXT("Exhaustion starts with 100"),0.5,[this]{Key(EKeys::W,IE_Pressed);Key(EKeys::LeftShift,IE_Pressed);});
		Add(TEXT("Exhausted held Shift stops sprint"),5.25,[this]{
			Check(0);Test->TestFalse(TEXT("Depleted component no longer drains"),S->IsSprinting());
			Test->TestEqual(TEXT("Exhaustion immediately restores walk speed"),Movement()->MaxWalkSpeed,500.0f);
			Test->TestTrue(TEXT("Exhaustion requires release"),Pawn->IsSprintReleaseRequired());
			Test->TestTrue(TEXT("Walking still works after exhaustion"),Pawn->GetVelocity().Size2D()>450);});
		Add(TEXT("Held Shift never auto-restarts on recovery"),1.8,[this]{
			Test->TestTrue(TEXT("Resource recovers while old Shift remains held"),S->GetCurrentStamina()>8);
			Pawn->StartSprint();Test->TestFalse(TEXT("Repeated start cannot bypass release latch"),S->IsSprinting());
			Test->TestEqual(TEXT("Recovered stamina alone cannot restart 750"),Movement()->MaxWalkSpeed,500.0f);Key(EKeys::LeftShift,IE_Released);});
		Add(TEXT("Release allows a fresh affordable press"),0.15,[this]{Test->TestFalse(TEXT("Release clears latch"),Pawn->IsSprintReleaseRequired());Key(EKeys::LeftShift,IE_Pressed);});
		Add(TEXT("Fresh press sprints again"),0.35,[this]{Test->TestTrue(TEXT("Fresh Shift with enough stamina resumes sprint"),S->IsSprinting()&&Pawn->GetVelocity().Size2D()>700);ResetFixture();});
		Add(TEXT("First real Left Ctrl"),1.2,[this]{Key(EKeys::LeftControl,IE_Pressed);});
		Add(TEXT("First dodge costs exactly 25"),0.08,[this]{
			Test->TestTrue(TEXT("First dodge is active"),Pawn->IsDodging());Check(75);
			Test->TestFalse(TEXT("Active repeat is rejected"),Pawn->TryDodge());Check(75);Key(EKeys::LeftControl,IE_Released);});
		Add(TEXT("Dodge ends without regeneration"),0.3,[this]{Test->TestFalse(TEXT("Original duration ends dodge"),Pawn->IsDodging());Check(75);});
		Add(TEXT("Cooldown rejection does not charge"),0.2,[this]{
			Test->TestFalse(TEXT("Cooling down cannot begin dodge"),Pawn->TryDodge());Check(75);
			Test->TestTrue(TEXT("Cooldown remains real"),Pawn->GetDodgeCooldownRemaining()>0);});
		Add(TEXT("Second affordable dodge"),0.55,[this]{Test->TestEqual(TEXT("Original one-second cooldown expired"),Pawn->GetDodgeCooldownRemaining(),0.0f);Check(75);Key(EKeys::LeftControl,IE_Pressed);});
		Add(TEXT("Second dodge costs exactly 25"),0.08,[this]{Test->TestTrue(TEXT("Second dodge active"),Pawn->IsDodging());Check(50);Key(EKeys::LeftControl,IE_Released);});
		Add(TEXT("Second dodge finishes"),0.3,[this]{Check(50);Test->TestFalse(TEXT("Second dodge ends"),Pawn->IsDodging());});
		Add(TEXT("Dodge end recovery delay"),0.6,[this]{Check(50);});
		Add(TEXT("Dodge automatic recovery"),0.55,[this]{
			Test->TestTrue(TEXT("Dodge recovery occurs after action plus delay"),S->GetCurrentStamina()>50&&S->IsRecovering());
			S->ResetStamina();S->TryConsumeStamina(76);Check(24);const float Delay=S->GetRecoveryDelayRemaining();
			Test->TestFalse(TEXT("24 cannot pay 25"),Pawn->TryDodge());Check(24);
			Test->TestEqual(TEXT("Insufficient request spends no cooldown"),Pawn->GetDodgeCooldownRemaining(),0.0f);
			Test->TestNull(TEXT("Insufficient request starts no montage"),Pawn->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage());
			Test->TestTrue(TEXT("Rejected request does not postpone regeneration"),FMath::Abs(S->GetRecoveryDelayRemaining()-Delay)<0.02);
			S->ResetStamina();Key(EKeys::SpaceBar,IE_Pressed);
		});
		Add(TEXT("Airborne denial is free"),0.15,[this]{
			Test->TestTrue(TEXT("Actually airborne"),Movement()->IsFalling());Test->TestFalse(TEXT("Airborne dodge refused"),Pawn->TryDodge());Check(100);
			Test->TestEqual(TEXT("Airborne denial spends no cooldown"),Pawn->GetDodgeCooldownRemaining(),0.0f);Key(EKeys::SpaceBar,IE_Released);});
		Add(TEXT("Point-blank wall setup"),1.3,[this]{
			Test->TestTrue(TEXT("Landed before wall test"),Movement()->IsMovingOnGround());Pawn->SetActorRotation(FRotator::ZeroRotator);
			const double X=Pawn->GetActorLocation().X+Pawn->GetCapsuleComponent()->GetScaledCapsuleRadius()+1.0+25.0;
			Wall=Box(FVector(X,Pawn->GetActorLocation().Y,150.0),FVector(25.0,500.0,400.0));});
		Add(TEXT("Point-blank obstruction is free"),0.1,[this]{
			Test->TestFalse(TEXT("Immediate wall prevents starting dodge"),Pawn->TryDodge());Check(100);
			Test->TestEqual(TEXT("Immediate wall spends no cooldown"),Pawn->GetDodgeCooldownRemaining(),0.0f);
			Test->TestNull(TEXT("Immediate wall starts no montage"),Pawn->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage());
			Wall->Destroy();const double X=Pawn->GetActorLocation().X+250.0;
			Wall=Box(FVector(X,Pawn->GetActorLocation().Y,150.0),FVector(25.0,500.0,400.0));Origin=Pawn->GetActorLocation();Key(EKeys::LeftControl,IE_Pressed);});
		Add(TEXT("Accepted collision-shortened dodge pays once"),0.4,[this]{
			Check(75);const double Travel=Pawn->GetActorLocation().X-Origin.X;
			Test->TestTrue(TEXT("Accepted dodge is shortened safely by wall"),Travel>100&&Travel<200);
			Test->TestFalse(TEXT("Wall dodge finishes"),Pawn->IsDodging());Key(EKeys::LeftControl,IE_Released);Wall->Destroy();});
		Add(TEXT("Sprint dodge coexistence setup"),0.8,[this]{ResetFixture();});
		Add(TEXT("Sprint dodge coexistence input"),0.5,[this]{Key(EKeys::W,IE_Pressed);Key(EKeys::LeftShift,IE_Pressed);});
		Add(TEXT("Sprint dodge begins"),0.4,[this]{Check(92,2);Sample();Key(EKeys::LeftControl,IE_Pressed);});
		Add(TEXT("Sprint drain pauses inside dodge"),0.08,[this]{
			Test->TestTrue(TEXT("Combined input accepts dodge"),Pawn->IsDodging());Check(SampleValue-25,1.5f);
			Test->TestEqual(TEXT("Explicit restoration cannot run during dodge"),S->RestoreStamina(10),0.0f);Sample();Key(EKeys::LeftControl,IE_Released);});
		Add(TEXT("No double drain inside dodge"),0.1,[this]{Check(SampleValue);Test->TestFalse(TEXT("Recovery also blocked in dodge"),S->IsRecovering());});
		Add(TEXT("Held sprint resumes after dodge"),0.6,[this]{
			Test->TestTrue(TEXT("Sprint resumes with remaining stamina"),S->IsSprinting()&&Pawn->GetVelocity().Size2D()>700);
			Test->TestTrue(TEXT("Resumed sprint actually drains"),S->GetCurrentStamina()<SampleValue);
			PC->FlushPressedKeys();Test->TestFalse(TEXT("Input flush stops sprint"),S->IsSprinting());Test->TestEqual(TEXT("Flush restores walk speed"),Movement()->MaxWalkSpeed,500.0f);ResetFixture();});
		Add(TEXT("Rapid Shift setup"),0.5,[this]{Key(EKeys::W,IE_Pressed);});
		for(int32 I=0;I<12;++I){
			Add(TEXT("Rapid Shift press"),0.04,[this]{Test->TestEqual(TEXT("Rapid release leaves normal max"),Movement()->MaxWalkSpeed,500.0f);Key(EKeys::LeftShift,IE_Pressed);});
			Add(TEXT("Rapid Shift release"),0.04,[this]{Test->TestEqual(TEXT("Rapid press selects sprint max once"),Movement()->MaxWalkSpeed,750.0f);Key(EKeys::LeftShift,IE_Released);});
		}
		Add(TEXT("Rapid Shift clean recovery"),0.12,[this]{
			Test->TestFalse(TEXT("Rapid Shift cannot leave sprint stuck"),S->IsSprinting());Test->TestTrue(TEXT("Rapid Shift bounded resource use"),S->GetCurrentStamina()>50&&S->GetCurrentStamina()<100);ResetFixture();});
		Add(TEXT("Rapid Ctrl accounting setup"),1.2,[this]{bTrackRapid=true;bWasDodging=false;SpentBefore=O->Spent;});
		for(int32 I=0;I<16;++I){
			Add(TEXT("Rapid Ctrl press"),0.04,[this]{Key(EKeys::LeftControl,IE_Pressed);});
			Add(TEXT("Rapid Ctrl release"),0.04,[this]{Key(EKeys::LeftControl,IE_Released);});
		}
		Add(TEXT("Rapid Ctrl exact fee accounting"),0.15,[this]{
			bTrackRapid=false;Test->TestTrue(TEXT("Rapid input produced accepted actions"),Starts.Num()>=2);
			for(int32 I=1;I<Starts.Num();++I){Test->TestTrue(TEXT("Rapid costs cannot bypass cooldown"),Starts[I]-Starts[I-1]>=0.99);}
			Test->TestTrue(TEXT("Exactly one 25 cost per observed accepted dodge"),FMath::Abs((O->Spent-SpentBefore)-25.0*Starts.Num())<0.02);
			Test->AddInfo(FString::Printf(TEXT("STAMINA_RAPID_CTRL cap=%d accepted=%d charged=%.3f"),Cap,Starts.Num(),O->Spent-SpentBefore));});
		Add(TEXT("Final automatic recovery to full"),8.0,[this]{
			Check(100);Test->TestFalse(TEXT("Full recovery turns Tick off"),S->IsComponentTickEnabled());Test->TestFalse(TEXT("Full recovery mode ends"),S->IsRecovering());
			Test->TestTrue(TEXT("Dynamic events observed consistent state"),O->bConsistent);
			Test->TestTrue(TEXT("Exhaustion and recovery threshold events happened"),O->Depletions>=1&&O->Recoveries>=1);
			Test->AddInfo(FString::Printf(TEXT("STAMINA_PIE_FINISHED cap=%d frames=%d simulation_fps=%.3f changes=%d depleted=%d recovered=%d actual_inputs_and_physics=true no_assets_saved=true"),Cap,Frames,Frames/Simulated,O->Changes,O->Depletions,O->Recoveries));});
	}
	void Cleanup()
	{
		if(bCleaned){return;}bCleaned=true;
		if(PC.IsValid()){PC->FlushPressedKeys();}
		if(S.IsValid()&&O.IsValid()){
			S->OnStaminaChanged.RemoveDynamic(O.Get(),&UWAStaminaTestObserver::Changed);
			S->OnStaminaDepleted.RemoveDynamic(O.Get(),&UWAStaminaTestObserver::Depleted);
			S->OnStaminaRecovered.RemoveDynamic(O.Get(),&UWAStaminaTestObserver::Recovered);}
		for(auto& F:Fixtures){if(F.IsValid()){F->Destroy();}}
		if(Fps){Fps->Set(PreviousCap,ECVF_SetByConsole);}
	}
	FAutomationTestBase* Test;
	TWeakObjectPtr<AWastelandAdventureCharacter> Pawn;
	TWeakObjectPtr<APlayerController> PC;
	TWeakObjectPtr<UWAStaminaComponent> S;
	TStrongObjectPtr<UWAStaminaTestObserver> O;
	TWeakObjectPtr<AActor> Wall;
	TArray<TWeakObjectPtr<AActor>> Fixtures;
	TArray<FStep> Steps;TArray<double> Starts;
	IConsoleVariable* Fps=nullptr;float PreviousCap=0,SampleValue=0,SpentBefore=0;
	FVector Origin;double CreatedAt=FPlatformTime::Seconds(),StepStarted=0,SampleTime=0,Simulated=0;
	int32 Cap,Step=0,Frames=0;bool bInitialized=false,bCleaned=false,bTrackRapid=false,bWasDodging=false;
};
bool RunStaminaPIE(FAutomationTestBase* Test,int32 Cap)
{
	if(GEditor->PlayWorld){Test->AddError(TEXT("Stop existing PIE before stamina tests"));return false;}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FStaminaPIECommand(Test,Cap));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWAStamina30Test,"WastelandAdventure.Stamina.Integration30FPS",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FWAStamina30Test::RunTest(const FString& P){return WastelandAdventure::Tests::RunStaminaPIE(this,30);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWAStamina60Test,"WastelandAdventure.Stamina.Integration60FPS",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FWAStamina60Test::RunTest(const FString& P){return WastelandAdventure::Tests::RunStaminaPIE(this,60);}
#endif
