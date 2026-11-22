// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

using UnrealBuildTool;

public class CoopPluginsEditorTarget : TargetRules
{
	public CoopPluginsEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// Latest, not a fixed version: in UE 5.4 these are V5 and Unreal5_4; in a newer installed engine the editor target
		// must use the engine's own settings, otherwise the build stops with "modifies the values of properties".
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "CoopPlugins", "CoopKitSampleBridges" });
	}
}
