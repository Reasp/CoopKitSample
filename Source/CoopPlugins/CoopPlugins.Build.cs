// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

using UnrealBuildTool;

// Game module of the CoopKit sample project: the gameplay framework skeleton (game mode, game state,
// player controller, player state, character) shared by every CoopKit plugin demo.
// It must not depend on any CoopKit plugin; plugin integration lives in the CoopKitSampleBridges module.
public class CoopPlugins : ModuleRules
{
	public CoopPlugins(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
