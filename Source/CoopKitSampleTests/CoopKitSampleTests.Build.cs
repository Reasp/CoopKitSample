// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

using UnrealBuildTool;

// Editor-only automation tests of the CoopKit sample project (filter "CoopKit.Sample").
// Kept out of the game module so that test code never ships in a Game or Server build.
public class CoopKitSampleTests : ModuleRules
{
	public CoopKitSampleTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// GameplayAbilities is an engine plugin (not a CoopKit plugin): the inventory test reads the abilities the bridge's
		// Ability System Component got. OnlineSubsystem and OnlineSubsystemUtils are engine plugins: the matchmaking test reads
		// the match id from each machine's game session. HTTP (engine module): the Nakama matchmaking test checks that a local
		// Nakama server answers before it starts. AssetRegistry (engine module): the heroes test finds every hero asset of the
		// project. CoreOnline (engine module): the host migration test keys players by unique net id. HTTPServer and Json (engine modules): the party test answers the PlayFab provider from a small local PlayFab mock (CKSamplePlayFabMock). CoopKit plugins and bridges stay unlinked; tests reach them by name and reflection.

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"Core",
			"CoreOnline",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"HTTP",
			"HTTPServer",
			"InputCore",
			"Json",
			"NetCore",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"UMG",
			"UnrealEd",
			"CoopPlugins"
		});
	}
}
