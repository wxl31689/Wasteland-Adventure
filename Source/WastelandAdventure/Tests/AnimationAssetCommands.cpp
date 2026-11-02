// One-time editor asset setup companion. Python does not expose BlendSpace::ResampleData.
#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "WastelandAdventure.h"

namespace WastelandAdventure::Editor
{
static void RebuildLocomotionBlendSpace()
{
	UBlendSpace* Space = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
	if (!Space || !FMath::IsNearlyEqual(Space->GetBlendParameter(1).Max, 750.0f))
	{
		UE_LOG(LogWastelandAdventure, Error, TEXT("Configure the inspected 750 cm/s BlendSpace axis before rebuilding it.")); return;
	}
	Space->Modify();
	for (int32 Index = 0; Index < Space->GetNumberOfBlendSamples(); ++Index)
	{
		const FBlendSample Sample = Space->GetBlendSample(Index);
		if (!Sample.Animation) { continue; }
		const FString Path = Sample.Animation->GetPathName();
		const bool bWalk = Path.Contains(TEXT("/Unarmed/Walk/"));
		const bool bJog = Path.Contains(TEXT("/Unarmed/Jog/"));
		if (!bWalk && !bJog) { continue; }
		const FVector Value(Sample.SampleValue.X, bWalk ? 500.0 : 750.0, Sample.SampleValue.Z);
		UE_LOG(LogWastelandAdventure, Display, TEXT("BlendSpace sample %s old=%.3f target=%.3f rate=%.6f"), *Sample.Animation->GetName(), Sample.SampleValue.Y, Value.Y, Sample.RateScale);
		if (!Sample.SampleValue.Equals(Value, 0.001) && !Space->EditSampleValue(Index, Value))
		{
			UE_LOG(LogWastelandAdventure, Error, TEXT("Could not edit sample %d."), Index); return;
		}
	}
	Space->ValidateSampleData();
	Space->ResampleData();
	Space->MarkPackageDirty();
	TArray<UPackage*> Packages { Space->GetPackage() };
	if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
	{
		UE_LOG(LogWastelandAdventure, Error, TEXT("Could not save rebuilt BlendSpace.")); return;
	}
	UE_LOG(LogWastelandAdventure, Display, TEXT("PHASE2_4_BLENDSPACE_REBUILT: native validation, sample editing, triangulation and package save complete."));
}
static FAutoConsoleCommand RebuildCommand(TEXT("WastelandAdventure.Animation.RebuildBlendSpace"),
	TEXT("Finish the one-time phase 2.4 BlendSpace setup using the native editor resampling API."),
	FConsoleCommandDelegate::CreateStatic(&RebuildLocomotionBlendSpace));
}
#endif
