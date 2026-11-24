// Copyright 2026 Ivan Iaitckikh. All Rights Reserved.

// CoopKit NetTest in the sample project (TASK-154): the plugin is enabled, and the sample's Net Play default (project
// defaults of the per-user editor settings, Config/DefaultEditorPerProjectUserSettings.ini) starts a listen server and one
// client under a preset that equals the network of every CoopKit.Sample network test (FNetSessionSettings: lag and loss on
// the packets every net driver sends). The test does not link against the plugin: it reads the plugin's settings object
// and its presets through reflection.

#include "CKSampleNetTestSession.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CKSample::Tests
{
	namespace NetTestPresetTest
	{
		const TCHAR* const SettingsClassPath = TEXT("/Script/CoopKitNetTest.CKNetTestSettings");
		const TCHAR* const EditorSettingsSection = TEXT("/Script/CoopKitNetTestEditor.CKNetTestEditorSettings");

		/** One side of a preset as the test reads it. */
		struct FPacketValues
		{
			int32 MinLatencyMs = INDEX_NONE;
			int32 MaxLatencyMs = INDEX_NONE;
			int32 PacketLossPercent = INDEX_NONE;
		};

		/** A preset as the test reads it. */
		struct FPresetValues
		{
			FName Name;
			FString Target;
			FPacketValues Outgoing;
			FPacketValues Incoming;
		};

		int32 ReadInt(const UStruct* Struct, const void* Container, const TCHAR* PropertyName)
		{
			const FIntProperty* Property = FindFProperty<FIntProperty>(Struct, PropertyName);
			return Property != nullptr ? Property->GetPropertyValue_InContainer(Container) : INDEX_NONE;
		}

		FPacketValues ReadPacket(const UStruct* PresetStruct, const void* Preset, const TCHAR* PropertyName)
		{
			FPacketValues Values;
			const FStructProperty* Property = FindFProperty<FStructProperty>(PresetStruct, PropertyName);
			if (Property != nullptr)
			{
				const void* Packet = Property->ContainerPtrToValuePtr<void>(Preset);
				Values.MinLatencyMs = ReadInt(Property->Struct, Packet, TEXT("MinLatencyMs"));
				Values.MaxLatencyMs = ReadInt(Property->Struct, Packet, TEXT("MaxLatencyMs"));
				Values.PacketLossPercent = ReadInt(Property->Struct, Packet, TEXT("PacketLossPercent"));
			}
			return Values;
		}

		/** Every preset of the plugin's settings object (Project Settings > Plugins > CoopKit NetTest), in order. */
		TArray<FPresetValues> ReadPresets(FAutomationTestBase& Test)
		{
			TArray<FPresetValues> Presets;
			const UClass* SettingsClass = FindObject<UClass>(nullptr, SettingsClassPath);
			const FArrayProperty* ArrayProperty = SettingsClass != nullptr ? FindFProperty<FArrayProperty>(SettingsClass, TEXT("Presets")) : nullptr;
			const FStructProperty* Inner = ArrayProperty != nullptr ? CastField<FStructProperty>(ArrayProperty->Inner) : nullptr;
			if (Inner == nullptr)
			{
				Test.AddError(FString::Printf(TEXT("%s with the array Presets is not loaded: CoopKit NetTest is not enabled in the sample."), SettingsClassPath));
				return Presets;
			}
			const FNameProperty* NameProperty = FindFProperty<FNameProperty>(Inner->Struct, TEXT("Name"));
			const FEnumProperty* TargetProperty = FindFProperty<FEnumProperty>(Inner->Struct, TEXT("Target"));
			FScriptArrayHelper Array(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(SettingsClass->GetDefaultObject()));
			for (int32 Index = 0; Index < Array.Num(); ++Index)
			{
				const void* Preset = Array.GetRawPtr(Index);
				FPresetValues& Values = Presets.AddDefaulted_GetRef();
				Values.Name = NameProperty != nullptr ? NameProperty->GetPropertyValue_InContainer(Preset) : NAME_None;
				if (TargetProperty != nullptr)
				{
					const int64 Target = TargetProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(TargetProperty->ContainerPtrToValuePtr<void>(Preset));
					Values.Target = TargetProperty->GetEnum()->GetNameStringByValue(Target);
				}
				Values.Outgoing = ReadPacket(Inner->Struct, Preset, TEXT("Outgoing"));
				Values.Incoming = ReadPacket(Inner->Struct, Preset, TEXT("Incoming"));
			}
			return Presets;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCKSampleNetTestPresetTest, "CoopKit.Sample.NetTest.Preset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCKSampleNetTestPresetTest::RunTest(const FString& Parameters)
{
	using namespace CKSample::Tests;
	using namespace CKSample::Tests::NetTestPresetTest;

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CoopKitNetTest"));
	if (!TestTrue(TEXT("CoopKit NetTest is enabled in the sample (CoopPlugins.uproject)"), Plugin.IsValid() && Plugin->IsEnabled()))
	{
		return false;
	}

	// The project's defaults, not this machine's Saved/Config: a developer's own Net menu choice must not fail the test.
	const FString DefaultsPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEditorPerProjectUserSettings.ini"));
	FConfigFile Defaults;
	Defaults.Read(DefaultsPath);
	FString PresetName;
	FString NumClients;
	FString HostMode;
	Defaults.GetString(EditorSettingsSection, TEXT("SelectedPreset"), PresetName);
	Defaults.GetString(EditorSettingsSection, TEXT("NumClients"), NumClients);
	Defaults.GetString(EditorSettingsSection, TEXT("HostMode"), HostMode);
	TestFalse(FString::Printf(TEXT("%s names the Net Play preset (SelectedPreset)"), *DefaultsPath), PresetName.IsEmpty());
	TestEqual(TEXT("Net Play starts one client by default (NumClients)"), NumClients, FString(TEXT("1")));
	TestEqual(TEXT("Net Play starts a listen server by default (HostMode)"), HostMode, FString(TEXT("ListenServer")));

	const TArray<FPresetValues> Presets = ReadPresets(*this);
	const FPresetValues* Preset = Presets.FindByPredicate([&PresetName](const FPresetValues& Values)
	{
		return Values.Name == FName(*PresetName);
	});
	if (!TestNotNull(FString::Printf(TEXT("CoopKit NetTest has the preset \"%s\" (of %d presets)"), *PresetName, Presets.Num()), Preset))
	{
		return false;
	}

	// The sample's network tests add the lag and the loss to the packets every net driver sends: the server's and each
	// client's. In a preset that is Target Everyone with Outgoing lag and loss and nothing on Incoming.
	const FNetSessionSettings SampleNetwork;
	AddInfo(FString::Printf(TEXT("The sample's test network: %d ms, %d%% loss on every side; preset \"%s\": %s, out %d..%d ms %d%%, in %d..%d ms %d%%."),
		SampleNetwork.PacketLagMs, SampleNetwork.PacketLossPercent, *PresetName, *Preset->Target, Preset->Outgoing.MinLatencyMs,
		Preset->Outgoing.MaxLatencyMs, Preset->Outgoing.PacketLossPercent, Preset->Incoming.MinLatencyMs, Preset->Incoming.MaxLatencyMs,
		Preset->Incoming.PacketLossPercent));
	TestEqual(TEXT("The preset emulates every side (Target)"), Preset->Target, FString(TEXT("Everyone")));
	TestEqual(TEXT("Outgoing minimum latency equals the tests' lag"), Preset->Outgoing.MinLatencyMs, SampleNetwork.PacketLagMs);
	TestEqual(TEXT("Outgoing maximum latency equals the tests' lag (no jitter)"), Preset->Outgoing.MaxLatencyMs, SampleNetwork.PacketLagMs);
	TestEqual(TEXT("Outgoing loss equals the tests' loss"), Preset->Outgoing.PacketLossPercent, SampleNetwork.PacketLossPercent);
	TestEqual(TEXT("No incoming latency"), Preset->Incoming.MinLatencyMs + Preset->Incoming.MaxLatencyMs, 0);
	TestEqual(TEXT("No incoming loss"), Preset->Incoming.PacketLossPercent, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
