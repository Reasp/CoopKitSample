# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`), and the copyright line with the project's. Examples 1 and 3 belong in an **Editor** module of the project (its `Build.cs` lists `"CoopKitNetTest"`, `"CoopKitNetTestEditor"`, `"UnrealEd"`); example 2 works in a runtime module that lists `"CoopKitNetTest"`.

The most common request needs no code: a new preset is a line in the project's `Config/DefaultCoopKitNetTest.ini` (see `extension_map.md`, "A preset in the ini file").

## 1. Start a test session from the project's own code ("Test 3 players on Mobile")

`Source/SleepwalkerEditor/Private/SleepwalkerNetTestCommand.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKNetTestPlaySession.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"

static void LaunchMobileSession()
{
	UCKNetTestEditorSubsystem* NetTest = GEditor != nullptr ? GEditor->GetEditorSubsystem<UCKNetTestEditorSubsystem>() : nullptr;
	if (NetTest == nullptr)
	{
		return;
	}

	// Start from the user's saved choices, change only what this command is about.
	FCKNetTestLaunchOptions Options = NetTest->MakeLaunchOptions();
	Options.Preset = TEXT("Mobile");
	Options.NumClients = 3;
	Options.HostMode = ECKNetTestHostMode::DedicatedServer;

	FText Error;
	if (!NetTest->LaunchSession(Options, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("Sleepwalker net test: %s"), *Error.ToString());
	}
}

static FAutoConsoleCommand SleepwalkerLaunchMobile(TEXT("sleepwalker.TestMobile"),
	TEXT("Plays the current level with a server and 3 clients under the Mobile network preset."),
	FConsoleCommandDelegate::CreateStatic(&LaunchMobileSession));
```

`LaunchSession` returns false with a reason when a session already runs or the preset name does not exist (names are not case sensitive). It works on a private copy of the Play settings, so the user's own Play settings stay unchanged. For a button, add it to a menu of the project with `UToolMenus`; the same calls exist in Blueprint and Python (`unreal.CKNetTestEditorSubsystem`).

## 2. Show the ping in the project's own HUD

`Source/Sleepwalker/Private/SleepwalkerPingText.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKNetTestStatsLibrary.h"
#include "Engine/World.h"

/** One line per connection, e.g. "Server: ping 312 ms | loss in 2.0% out 1.8% | ...". Empty in a standalone game. */
FString MakeSleepwalkerPingText(const UWorld* World)
{
	FString Text;
	for (const FCKNetTestConnectionStats& Stats : UCKNetTestStatsLibrary::GetConnectionStats(World))
	{
		Text += UCKNetTestStatsLibrary::FormatConnectionStats(Stats) + LINE_TERMINATOR;
	}
	return Text;
}
```

The values cover the last second and refresh about once a second. On a client the list has one entry (the server); on a server it has one entry per client.

## 3. The automation test of a preset the project added

`Source/SleepwalkerEditor/Private/SleepwalkerNetTestPresetTest.cpp`

This test needs no session. A test that plays a whole multiplayer session starts it with `LaunchSession`, waits (latent command, with a time limit) until the server world sees every player, checks the project's own rule on the server and on every client, and ends the session with `GEditor->RequestEndPlayMap()` also when a check failed. The plugin's own `Source/CoopKitNetTestTests/Private/CKNetTestPingTest.cpp` shows the shape; its helper classes are not part of the shipped plugin, so copy the idea, not the include.

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKNetTestPlaySession.h"
#include "CKNetTestSettings.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerNetTestPresetTest, "Sleepwalker.NetTest.MobilePreset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerNetTestPresetTest::RunTest(const FString& Parameters)
{
	// The good case: the project's preset exists and adds what the team agreed on.
	const FCKNetTestPreset* Preset = UCKNetTestSettings::Get().FindPreset(TEXT("Mobile"));
	if (!TestNotNull(TEXT("The Mobile preset is in DefaultCoopKitNetTest.ini"), Preset))
	{
		return false;
	}
	TestTrue(TEXT("The preset adds delay or loss"), Preset->HasEffect());
	TestTrue(TEXT("The preset adds at least 200 ms to the round trip"), Preset->GetMinAddedRoundTripMs() >= 200);

	// The bad case: a mistyped name is refused with a reason and changes nothing.
	UCKNetTestEditorSubsystem* NetTest = GEditor != nullptr ? GEditor->GetEditorSubsystem<UCKNetTestEditorSubsystem>() : nullptr;
	if (TestNotNull(TEXT("The editor subsystem exists"), NetTest))
	{
		const FName SelectedBefore = NetTest->GetSelectedPreset();
		FText Error;
		TestFalse(TEXT("A preset that does not exist is refused"), NetTest->SelectPreset(TEXT("Mobil"), Error));
		TestFalse(TEXT("The refusal says why"), Error.IsEmpty());
		TestEqual(TEXT("The selected preset did not change"), NetTest->GetSelectedPreset(), SelectedBefore);
	}
	return true;
}

#endif
```

### Testing a validation

The plugin's presets live in the project's settings and have no `IsDataValid`: a test reads the preset and checks its values (see example 3). A data asset of your own (a `UPrimaryDataAsset` subclass with `IsDataValid`) is tested like this, inside `#if WITH_EDITOR`, because `IsDataValid` exists only in the editor: `USleepwalkerSquadData` and its `SquadSize` stand for your own class and property.

```cpp
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

// Bad data: the asset must report it.
USleepwalkerSquadData* Data = NewObject<USleepwalkerSquadData>(GetTransientPackage());
FIntProperty* Size = FindFProperty<FIntProperty>(Data->GetClass(), TEXT("SquadSize"));
if (!TestNotNull(TEXT("The property exists"), Size))
{
	return false;
}
Size->SetPropertyValue_InContainer(Data, 0);
FDataValidationContext Bad;
TestEqual(TEXT("A squad of 0 is invalid"), Data->IsDataValid(Bad), EDataValidationResult::Invalid);
TestTrue(TEXT("The error is reported"), Bad.GetNumErrors() > 0);

// Good data: no errors.
Size->SetPropertyValue_InContainer(Data, 4);
FDataValidationContext Good;
Data->IsDataValid(Good);
TestTrue(TEXT("A squad of 4 has no errors"), Good.GetNumErrors() == 0);
```

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
