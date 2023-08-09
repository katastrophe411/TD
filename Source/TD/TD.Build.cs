// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TD : ModuleRules
{
	public TD(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "NavigationSystem", "AIModule",
			"Niagara", "EnhancedInput", "CommonUI", "CommonUIEditor", "Slate", "SlateCore", "UMG", "UMGEditor", "WidgetCarousel",
		});
	}
}