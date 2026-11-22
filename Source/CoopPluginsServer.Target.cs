// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

using UnrealBuildTool;

// Dedicated server target. Builds only with an engine built from source: installed (launcher) engines ship without
// Server libraries, so on them this target is listed but never built.
public class CoopPluginsServerTarget : TargetRules
{
	public CoopPluginsServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		// Latest, not a fixed version: the same settings as the game and editor targets (see CoopPlugins.Target.cs).
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "CoopPlugins", "CoopKitSampleBridges" });
	}
}
