// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavenBreach : ModuleRules
{
	public HeavenBreach(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "NavigationSystem",
			"HTTP",
			"Json",
            "JsonUtilities",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"HeavenBreach",
			"HeavenBreach/Interaction",
			"HeavenBreach/Loot",
			"HeavenBreach/Escape",
			"HeavenBreach/Variant_Horror",
			"HeavenBreach/Variant_Horror/UI",
			"HeavenBreach/Variant_Shooter",
			"HeavenBreach/Variant_Shooter/AI",
			"HeavenBreach/Variant_Shooter/UI",
			"HeavenBreach/Variant_Shooter/Weapons",
			"HeavenBreach/Inventory",
            "HeavenBreach/Equipment",
            "HeavenBreach/Item",
            "HeavenBreach/Core/DataManagement"
        });
		
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
