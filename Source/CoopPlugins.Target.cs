// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

using UnrealBuildTool;

public class CoopPluginsTarget : TargetRules
{
	public CoopPluginsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Latest, not a fixed version: in UE 5.4 these are V5 and Unreal5_4; a newer engine builds with its own defaults
		// (the same settings as the editor target, see CoopPluginsEditor.Target.cs).
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "CoopPlugins", "CoopKitSampleBridges" });
	}
}
