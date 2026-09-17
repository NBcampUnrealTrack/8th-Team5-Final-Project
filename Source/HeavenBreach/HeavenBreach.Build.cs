// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HeavenBreach : ModuleRules
{
	public HeavenBreach(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			// Stash account resolution calls FUniqueNetIdWrapper APIs implemented by CoreOnline.
			"CoreOnline",
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
			"CommonUI",
			"CommonInput",
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

		// DLSS is a client rendering feature; dedicated servers do not link the plugin.
		bool bWithDLSS = Target.Platform == UnrealTargetPlatform.Win64 && Target.Type != TargetType.Server;
		PublicDefinitions.Add("HB_WITH_DLSS=" + (bWithDLSS ? "1" : "0"));
		if (bWithDLSS)
		{
			PrivateDependencyModuleNames.Add("DLSSBlueprint");
		}

		PublicIncludePaths.AddRange(new string[] {
			"HeavenBreach",
			"HeavenBreach/Interaction",
			"HeavenBreach/Loot",
			"HeavenBreach/SealedDoor",
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
            "HeavenBreach/Core/DataManagement",
            "HeavenBreach/Shop",
            "HeavenBreach/UI",
            "HeavenBreach/Craft"
        });
		
		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
