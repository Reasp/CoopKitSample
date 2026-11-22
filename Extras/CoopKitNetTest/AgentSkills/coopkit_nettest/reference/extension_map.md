# CoopKitNetTest: extension map

Plugin module names: `CoopKitNetTest` (runtime: presets, stats, demo level) and `CoopKitNetTestEditor` (editor only: Play toolbar, session launch, console commands). Add `"CoopKitNetTest"` to the dependency lists of your project module's `Build.cs`, and `"CoopKitNetTestEditor"` only to an **Editor** module of the project. Headers are included by plain name: `#include "CKNetTestPlaySession.h"`.

## Entity types (what the CoopKit Hub and the cards know)

CoopKitNetTest has no entity types: it is an editor tool, not a mechanic with game data. Its presets are project settings (an ini file, like every other setting of a tool), so the plugin adds nothing to the CoopKit Hub and a new preset is never an asset. Do not create Data Assets for presets.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| none | | | | | | | |
<!-- coopkit:entities:end -->

## Classes you will meet

All classes are `final`: call them, do not subclass them.

| Class | Module | Role |
|---|---|---|
| `UCKNetTestSettings` (`CKNetTestSettings.h`, *Project Settings > Plugins > CoopKit NetTest*) | runtime | the presets of the project: `GetPresets`, `FindPreset`, `GetPresetNames`; saved to the project's `Config/DefaultCoopKitNetTest.ini` |
| `FCKNetTestPreset`, `FCKNetTestPacketSettings`, `ECKNetTestEmulationTarget` (`CKNetTestTypes.h`) | runtime | one preset: `Name`, `Description`, `Target`, `Outgoing`, `Incoming` (min latency, max latency, loss percent) |
| `UCKNetTestStatsLibrary` (`CKNetTestStatsLibrary.h`) | runtime | `GetConnectionStats`, `FormatConnectionStats`, `GetOverlayLines`, `SetOverlayVisible`, `IsOverlayVisible` (also Blueprint) |
| `UCKNetTestEditorSettings` (`CKNetTestEditorSettings.h`, *Editor Preferences > Plugins > CoopKit NetTest*) | editor | the user's choices: selected preset, clients (1-8), host mode, process mode |
| `UCKNetTestEditorSubsystem` (`CKNetTestPlaySession.h`) | editor | `SelectPreset`, `GetSelectedPreset`, `GetActiveEmulationName`, `MakeLaunchOptions`, `CanLaunchSession`, `LaunchSession` (also Blueprint and Python) |
| `FCKNetTestLaunchOptions` (`CKNetTestPlaySession.h`) | editor | the layout of a launch: `Preset`, `NumClients`, `HostMode` (`DedicatedServer`, `ListenServer`), `ProcessMode` (`OneProcess`, `SeparateProcesses`) |
| `CKNetTest::TryParseLaunchArguments`, `ApplyPresetToEmulation`, `ConfigurePlaySettings` | editor | the pure pieces behind the console command and the Play settings |

## Console commands (for the developer, not required for the user)

`ck.nettest.Preset [Name]`, `ck.nettest.Launch [Clients=<1..8>] [Dedicated|Listen] [Preset=<Name>] [OneProcess|SeparateProcesses]`, `ck.nettest.Overlay 0|1`.

## A preset in the ini file

In the project's `Config/DefaultCoopKitNetTest.ini` (the plugin's defaults are in `Plugins/CoopKitNetTest/Config/`; the project's file is merged on top, so only add your lines):

```ini
[/Script/CoopKitNetTest.CKNetTestSettings]
+Presets=(Name="Mobile",Description="Mobile data: +300 ms ping, 3% loss.",Target=Everyone,Outgoing=(MinLatencyMs=150,MaxLatencyMs=150,PacketLossPercent=3),Incoming=(MinLatencyMs=0,MaxLatencyMs=0,PacketLossPercent=0))
```

- `Name` is unique and not empty; it is what the menu and `ck.nettest.Preset` show.
- Latency is added on every emulated side: with `Target=Everyone` the ping grows by twice the preset's latency. A preset with no latency and no loss means "emulation off".
- `MaxLatencyMs` above `MinLatencyMs` gives jitter (a random delay between the two).
- Latency is 0..5000 ms, loss is 0..100 percent.

Prefer the settings page when the user is at the editor: it shows the same fields with tooltips and writes the same file.

## Where a new test goes (checklist)

1. **Preset or code?** A new network condition is a preset (above). Code is only for starting sessions or reading stats from the project's own tool or test.
2. **Editor or runtime?** Starting a session and selecting a preset need `CoopKitNetTestEditor`: an Editor module of the project (`"Type": "Editor"` in the `.uproject`). Stats can be read at runtime from `CoopKitNetTest`.
3. **Server and clients.** Run your game rule on the server (`HasAuthority()`) and check on every client what replicated. Under a preset, allow time: at 150 ms per side a round trip takes about 300 ms and a lost packet is resent later.
4. **Test.** An automation test with the patterns in `examples.md` (one idle editor, wait for the players, always end the session). Keep waits in frames or with a timeout, never a fixed sleep.
5. **Report.** Tell the user which preset and layout the test used and what it measured.

## What not to touch

- Anything under `Plugins/CoopKitNetTest/` (see "Editing the plugin" in `rules.md`).
- The plugin's `ck.nettest.*` console names and the Play toolbar section `CoopKitNetTest`: documentation and demo refer to them.
- The user's Editor Preferences > Play > Multiplayer settings: the subsystem's `LaunchSession` works on a private copy so they stay as the user left them; `SelectPreset` is the one call that writes them, on purpose.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitNetTestEditor/Private/`: `CoopKitNetTestEditorModule.cpp` (toolbar, console commands, demo restart), `CKNetTestPlaySession.cpp` (presets to Play settings, launch), `CKNetTestSkills.cpp` (installer of these skills). The plugin's own tests in `Source/CoopKitNetTestTests/Private/` show a full session in an automation test.
