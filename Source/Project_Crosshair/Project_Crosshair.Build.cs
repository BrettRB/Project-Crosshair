// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_Crosshair : ModuleRules
{
	public Project_Crosshair(ReadOnlyTargetRules Target) : base(Target)
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

		PublicIncludePaths.AddRange(new string[] {
			"Project_Crosshair",
			"Project_Crosshair/Variant_Horror",
			"Project_Crosshair/Variant_Horror/UI",
			"Project_Crosshair/Variant_Shooter",
			"Project_Crosshair/Variant_Shooter/AI",
			"Project_Crosshair/Variant_Shooter/UI",
			"Project_Crosshair/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
