---
name: coopkit-nettest
description: Add or change a network preset, a session layout or a network test of the CoopKit NetTest Unreal Engine plugin, then build the project. Use when the user asks for a new network condition to test under ("add a Mobile preset: 200 ms and 3% loss", "a preset where only the server lags"), to start a multiplayer test session from their own button, menu or script, to read the ping and packet loss in their own HUD or test, to write an automated multiplayer test that runs under a preset, or to build and fix compile errors in a project that uses CoopKitNetTest. The plugin is an editor tool without game data assets: presets are project settings. Extends the plugin from the project without editing it, follows the CoopKit code rules, adds an automation test and builds the Editor target, explaining in plain words what to install if the build cannot run.
compatibility: Unreal Engine 5.6, 5.7 and 5.8, a C++ project. CoopKitNetTest itself supports Windows (Win64) only, built with Visual Studio 2022. Reads and writes files in the project; runs the engine's build tool.
---

# CoopKitNetTest: extend the plugin and build

You are helping a game maker (often an artist, not a programmer) test their multiplayer game under bad network conditions with CoopKitNetTest. They describe the result; you set the preset or write the C++, test it and build it. Explain results in their language, without jargon.

## Recipe

1. **Learn the project.** Find the `.uproject`, its game module (`Source/<Name>/`, its `Build.cs`) and whether CoopKitNetTest sits in the project's or the engine's `Plugins` folder. Read one existing project source file to take over the copyright line; if there is none, ask the user whose name goes in the copyright line. Never use another company's notice.
2. **Choose the extension point** with the table below and read `reference/extension_map.md` for the chosen row.
3. **Extend without editing the plugin.** The plugin is a purchased product that gets updates; code you add to it is lost on update. Most requests need no C++ at all: a preset is a line in the project's `Config/DefaultCoopKitNetTest.ini`. Everything else goes through the plugin's public classes from the project's own module (all of them are `final`: call them, do not subclass them). Edit the plugin only when no extension point can do the job, and then follow "Editing the plugin" in `reference/rules.md`.
4. **Write the code by the rules** in `reference/rules.md` (English code and comments, copyright, Unreal naming, engine-version differences in a compat header). Copy the shape from `reference/examples.md`.
5. **Add an automation test** for the new logic (see the examples file). A parameter with validation gets a test that bad data is reported.
6. **Build** with the commands in "Build the project" below, never with other commands. The result decides:
   - **BUILD OK** (exit code 0) - go on.
   - **BUILD FAILED** (errors in the code) - fix the errors the log lists and build again until it passes.
   - **CANNOT BUILD** (the computer or the project is not ready) - the user must act. Tell them what is missing and how to fix it in plain words, then stop.
7. **Run the tests** with the command in `reference/build_and_diagnose.md` (section "Run the tests"). Fix until they pass.
8. **Report** in a few lines: what you added and where, how the user sees it in the editor (which menu entry, which Project Settings page, which console command), and that the build and the test passed.

<!-- coopkit-build:begin - the same text in every CoopKit skill (fab-plugin new template; check agent-skill compares it) -->
## Build the project

Build the project's Editor target with the engine's own build tool, exactly as below. A build takes from one minute (a small change) to half an hour (the first build of a project): wait for it and never start a second one. CoopKit plugins run on Windows, so these are Windows commands.

1. **Find the engine.** Read `EngineAssociation` in the project's `.uproject`.
   - A version such as `5.7`: in `C:\ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat` (JSON) take `InstallLocation` of the entry whose `AppName` is `UE_5.7`. Not there: `reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.7" /v InstalledDirectory`.
   - A `{GUID}` (an engine built from source): `reg query "HKCU\Software\Epic Games\Unreal Engine\Builds" /v {GUID}`.

   `<Engine>` is that folder; it has `Engine\Build\BatchFiles\Build.bat`. No such folder: **CANNOT BUILD**. The user installs this engine version in the Epic Games Launcher, or verifies it there (Library, the engine, Verify).
2. **Find the target.** `<Target>` is the name of the file in `Source` that ends in `Editor.Target.cs`, without `.Target.cs`. No such file: **CANNOT BUILD**. It is a Blueprint-only project: the user adds a C++ class once in the editor (Tools > New C++ Class), or you create the module as in `reference/examples.md`, example 5, with the user's consent.
3. **Build** in PowerShell from the project folder. The full log goes to `Saved\CoopKitAgent\build.log`:

   ```powershell
   New-Item -ItemType Directory -Force Saved\CoopKitAgent | Out-Null
   & "<Engine>\Engine\Build\BatchFiles\Build.bat" <Target> Win64 Development "-Project=$PWD\<Project>.uproject" -WaitMutex 2>&1 | Out-File -Encoding utf8 Saved\CoopKitAgent\build.log
   "exit code $LASTEXITCODE"
   ```

   From another shell, run the same `Build.bat` with the same arguments and send all its output to the same file.
4. **Read the result.**
   - Exit code `0`: **BUILD OK**.
   - Otherwise search the log for `error`. Errors in the files of the project or of the plugin (`error C2065`, `error LNK2019`, `fatal error C1083`): **BUILD FAILED**. Fix them (`reference/build_and_diagnose.md` says what usually fixes each kind) and build again.
   - The log has a message from the table "The computer or the project is not ready" in `reference/build_and_diagnose.md`: **CANNOT BUILD**. Stop changing code and tell the user what that row says.
   - Neither: show the user the last 30 lines of the log, say that the build failed for a reason you cannot fix, and stop.
<!-- coopkit-build:end -->

## Where does a new parameter go?

| The user wants... | Put it in | Why |
|---|---|---|
| a new network condition (delay, jitter, loss) to test under | a preset: **Project Settings > Plugins > CoopKit NetTest > Presets**, or a `+Presets=(...)` line in the project's `Config/DefaultCoopKitNetTest.ini` (format in `reference/extension_map.md`) | presets are project data shared by the team; no C++, and the toolbar menu and `ck.nettest.Preset` list it at once |
| a preset that delays only the server or only the clients | the preset's `Target` (*Server*, *Clients*, *Everyone*) | latency is added on every emulated side, so *Everyone* doubles the added ping |
| their own button, menu entry or script that starts a test session | an editor module of the project that calls `UCKNetTestEditorSubsystem::LaunchSession` (or Blueprint / Python on the same subsystem) with `FCKNetTestLaunchOptions` (example 1) | the subsystem works on a private copy of the Play settings and leaves the user's Play settings alone |
| ping and packet loss in their own HUD or log | `UCKNetTestStatsLibrary::GetConnectionStats` and `FormatConnectionStats` (works in every build, also Shipping) (example 2) | the overlay of the plugin is only one user of this data |
| an automated multiplayer test | an automation test in the project that starts a session through the subsystem and waits for the players, then checks its own game rule (example 3) | the plugin's own test helpers are not shipped; the public API is enough |
| a preset applied from code (CI, a custom menu) | `UCKNetTestEditorSubsystem::SelectPreset` | it writes the preset into the Play settings, so the regular Play button uses it too |
| a change inside the plugin's emulation, launch rules or editor windows | the plugin source, only if no extension point exists | see "Editing the plugin" |

When unsure, start with a preset in the settings: it needs no code.

## Hard rules

- Do not edit anything under the plugin folder unless the recipe above sends you there; never edit the engine.
- Code and comments are English; chat with the user in their language.
- Every Blueprint-visible and editable property has a tooltip comment, `Category` and, for numbers, `ClampMin` where a range makes sense.
- `UCKNetTestEditorSubsystem` and everything that launches sessions are editor-only: code that calls them lives in an Editor module of the project and never ships in a Game or Server build. `UCKNetTestStatsLibrary` is runtime and also works in Shipping.
- Network emulation changes only how a test session behaves; never leave a preset switched on in a project's shipped settings or in `Config/DefaultEngine.ini`.
- Do not claim the build or the test passed unless the exit code of the build and the test run said so.
- Do not install Visual Studio or anything else yourself, and do not edit the engine: the user installs what `reference/build_and_diagnose.md` names.
