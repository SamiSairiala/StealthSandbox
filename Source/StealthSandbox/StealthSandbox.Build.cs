// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StealthSandbox : ModuleRules
{
	public StealthSandbox(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"HeadMountedDisplay",

			// New input system
			"EnhancedInput",

			// AI / perception / navigation
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",

			// UI later
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}