// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WastelandAdventure : ModuleRules
{
	public WastelandAdventure(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd"); // Actual PIE input/physics regression test only.
			PrivateDependencyModuleNames.Add("AnimGraphRuntime"); // Inspect real running animation nodes in editor tests.
		}

		PublicIncludePaths.AddRange(new string[] {
			"WastelandAdventure",
			"WastelandAdventure/Variant_Platforming",
			"WastelandAdventure/Variant_Platforming/Animation",
			"WastelandAdventure/Variant_Combat",
			"WastelandAdventure/Variant_Combat/AI",
			"WastelandAdventure/Variant_Combat/Animation",
			"WastelandAdventure/Variant_Combat/Gameplay",
			"WastelandAdventure/Variant_Combat/Interfaces",
			"WastelandAdventure/Variant_Combat/UI",
			"WastelandAdventure/Variant_SideScrolling",
			"WastelandAdventure/Variant_SideScrolling/AI",
			"WastelandAdventure/Variant_SideScrolling/Gameplay",
			"WastelandAdventure/Variant_SideScrolling/Interfaces",
			"WastelandAdventure/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
