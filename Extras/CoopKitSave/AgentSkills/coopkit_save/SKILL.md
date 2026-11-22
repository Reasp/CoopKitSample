---
name: coopkit-save
description: Add or change what a co-op game saves, how it loads, or a setting of the CoopKit Save Unreal Engine plugin, then build the project. Use when the user asks to save something new ("the chest remembers its coins", "save the door state", "save what the player carries"), to keep old saves working after a change ("I renamed a variable", "a value now means something else"), to match players to their saves another way ("by our account id"), to block or react to saving ("no autosave during the boss fight", "show a Saved message"), to configure UCKSaveProfileData, or to build and fix compile errors in a project that uses CoopKitSave. Extends the plugin from the project's own C++ module without editing the plugin, follows the CoopKit code rules, adds an automation test and builds the Editor target, explaining in plain words what to install if the build cannot run.
compatibility: Unreal Engine 5.6, 5.7 and 5.8, a C++ project. CoopKitSave itself supports Windows (Win64) only, built with Visual Studio 2022. Reads and writes files in the project; runs the engine's build tool.
---

# CoopKit Save: extend what is saved and loaded, and build

You are helping a game maker (often an artist, not a programmer) add something to the save and load of a co-op game. They describe the result; you write the C++, test it and build it. Explain results in their language, without jargon.

## Recipe

1. **Learn the project.** Find the `.uproject`, its game module (`Source/<Name>/`, its `Build.cs`) and whether CoopKitSave sits in the project's or the engine's `Plugins` folder. Read one existing project source file to take over the copyright line; if there is none, ask the user whose name goes in the copyright line. Never use another company's notice.
2. **Choose the extension point** with the table below and read `reference/extension_map.md` for the chosen row.
3. **Extend without editing the plugin.** The plugin is a purchased product that gets updates; code you add to it is lost on update. Its classes are `final`: use `SaveGame` properties and the `ICKSaveable` / `ICKSavePlayerIdentity` interfaces on the project's own classes, the events of `UCKSaveSubsystem`, Gameplay Tags or a Blueprint in the project. Edit the plugin only when no extension point can do the job, and then follow "Editing the plugin" in `reference/rules.md`.
4. **Write the code by the rules** in `reference/rules.md` (English code and comments, copyright, Unreal naming, engine-version differences in a compat header). Copy the shape from `reference/examples.md`.
5. **Add an automation test** for the new logic (see the examples file). A parameter with validation gets a test that bad data is reported.
6. **Build** with the commands in "Build the project" below, never with other commands. The result decides:
   - **BUILD OK** (exit code 0) - go on.
   - **BUILD FAILED** (errors in the code) - fix the errors the log lists and build again until it passes.
   - **CANNOT BUILD** (the computer or the project is not ready) - the user must act. Tell them what is missing and how to fix it in plain words, then stop.
7. **Run the tests** with the command in `reference/build_and_diagnose.md` (section "Run the tests"). Fix until they pass.
8. **Report** in a few lines: what you added and where, how the user sees it in the editor (which asset, which tab of its card, which field, which Project Settings page), and that the build and the test passed.

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

<!-- coopkit-agent:begin - the same text in every CoopKit skill of a plugin with a hub (fab-plugin new template; check agent-surface compares it) -->
## Drive the editor

CoopKitSave has an editor surface for agents: `UCKSaveAgentLibrary`, a function library of the plugin's editor module. Each call does one of the things the CoopKit Hub, the New wizard and the entity card do by mouse: list the entity types and the entities, create an entity, describe it, validate it, export and import its field values, check the project. Use it when the user asks to create or change the plugin's data ("make three new ...", "set ... to ...", "what is wrong with my ..."). Never edit `.uasset` files as text, and do not ask the user to click through windows for what one call does.

**Calling it.** The functions run inside the Unreal Editor of the project, from Python (or an Editor Utility Blueprint, category `CoopKit|Save|Agent`).

- The editor is open: the user enables **Python Editor Script Plugin** once (Edit > Plugins, restart), then runs your script with File > Execute Python Script, or pastes lines into the Output Log with the command box switched from `Cmd` to `Python`.
- No editor window (the project must be closed): run the script with the editor's command-line build and end the script with `unreal.SystemLibrary.quit_editor()`:

  ```powershell
  & "<Engine>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\<Project>.uproject" -ExecutePythonScript="$PWD\Saved\CoopKitAgent\task.py" -EnablePlugins=PythonScriptPlugin -unattended -nullrhi -nosplash -stdout 2>&1 | Out-File -Encoding utf8 Saved\CoopKitAgent\python.log
  ```

  `<Engine>` is found as in "Build the project". Print the answers with `unreal.log(...)` and read them from the log.

**The calls.** Python names are the C++ names in snake_case, on the class `unreal.CKSaveAgentLibrary`:

| Call | Does |
|---|---|
| `describe_surface()` | the plugin, the schema version and the list of calls |
| `list_entity_types()` | entity types: `type`, `class`, `folder`, `factory`, `canCreate`, `idField` |
| `list_entities(type_name)` | entities of a type: `path`, `asset`, `name`, `displayName` |
| `create_entity(type_name, name, save=False)` | a new entity, as New in the hub makes it (asset `DA_<Type>_<Name>` in the type's folder) |
| `describe_entity(asset_path)` | editable fields (`name`, `type`, `category`, `tooltip`, `value`, `readOnly`) and parts |
| `validate_entity(asset_path)` | what the card's Validation tab shows: `valid`, `errors` and `warnings` with the `field` they name |
| `check_project()` | the hub's project check for this plugin's entities |
| `export_entity(asset_path)` | `{"fields": {"<Field>": "<value>"}}` |
| `import_entity(asset_path, json, save=False)` | sets fields from such JSON; all or nothing; one undo step |

A type is its name as the hub shows it or its class name. An asset path is `/Game/Folder/DA_Name.DA_Name` (the package path without `.DA_Name` works too). Values are text as the Details panel copies them: `12.5`, `True`, `(X=1.0,Y=2.0)`, an asset path; JSON numbers and booleans work too.

**Reading the answer.** Every call returns a JSON string: `{"schema": "coopkit.agent/1", "plugin": "CoopKitSave", "operation": "...", "ok": true, "result": {...}, "error": null}`. When `ok` is false, `result` is null and `error` has a `code` (`unknown_type`, `invalid_name`, `already_exists`, `no_factory`, `create_failed`, `asset_not_found`, `not_an_entity`, `invalid_json`, `unknown_field`, `read_only_field`, `invalid_value`, `save_failed`) and a `message`. Fix the call by the code; report the message to the user when you cannot.

**Example: create, set, check, save.**

```python
import json, unreal

lib = unreal.CKSaveAgentLibrary

def call(answer):
    reply = json.loads(answer)
    if not reply["ok"]:
        raise RuntimeError(reply["error"]["code"] + ": " + reply["error"]["message"])
    return reply["result"]

types = call(lib.list_entity_types())["types"]
type_name = types[0]["type"]                       # pick the type the user means
path = call(lib.create_entity(type_name, "Alpha"))["path"]
for field in call(lib.describe_entity(path))["fields"]:
    unreal.log(field["name"] + " (" + field["type"] + ") = " + field["value"])

result = call(lib.import_entity(path, json.dumps({"fields": {"<Field>": "<value>"}}), save=True))
unreal.log("changed: " + ", ".join(result["changed"]))
for issue in result["validation"]["errors"]:
    unreal.log_warning(issue["field"] + ": " + issue["message"])
```

To copy an entity's values to another one, pass the answer of `export_entity` to `import_entity` of the other entity unchanged. After changes, run `check_project()` and tell the user what is still invalid, in plain words. The new data shows in the editor at once: the user finds it in the CoopKit Hub (Tools > CoopKit Hub) and opens its card there.
<!-- coopkit-agent:end -->

## Where does a new parameter go?

| The user wants... | Put it in | Why |
|---|---|---|
| a new value of an actor saved (a chest's coins, a door's state, a lamp's switch) | a `SaveGame` property on the project's actor or its component, plus a **CoopKit Saveable** component on the actor (no C++ for Blueprints: *Variable > Advanced > SaveGame*) | the plugin saves only what is marked; a placed actor needs a unique *Save Id* when it may be renamed |
| a value of a player saved (inventory, level, skills) | a `SaveGame` property on the project's player state or pawn | the plugin saves player states and pawns without marks, under the player's key |
| to rebuild something after a load (UI, an effect, a cached value) or convert an old value | the `ICKSaveable` interface on the actor or component: `OnBeforeSave`, `OnAfterLoad(SavedGameDataVersion)` (examples 1-2) | `OnAfterLoad` runs on the server right after the values were applied and tells the version the save was written with |
| old saves to keep working after a change | add properties freely; **rename** with a property redirect; when the *meaning* changes raise *Game Data Version* of the profile and convert in `OnAfterLoad` or `On Migrate Save` (`reference/extension_map.md`, "Old saves") | saves are tagged by property name; a higher version does not load in a lower game |
| another way to match a joining player to their record (an account id, a character slot) | `ICKSavePlayerIdentity::GetSavePlayerKey` on the player state, or `ResolvePlayerKey` of `UCKSaveSubsystem` in C++ (example 3) | the plugin asks the player state first, then the profile's *Player Key Source* |
| other slots, autosave, what is saved, version | **New Save Profile** in the CoopKit Hub (no C++), set with `SetProfile` or in *Project Settings > Plugins > CoopKit Save* | the profile is the plugin's data; `UCKSaveProfileData` is `final` |
| a Save or Load button, a "Saved" message, slot list UI | a widget that calls `SaveToSlot`, `LoadFromSlot`, `GetSlots` of `UCKSaveSubsystem` (or the Blueprint nodes *Save Game To Slot (CoopKit)* and *Load Game From Slot (CoopKit)*) and listens to `OnSaveCompleted` / `OnLoadCompleted` (example 4) | the subsystem is the one door; results are never silent |
| no save or load for a while (a boss fight, a cutscene) | the loose tags `CK.Save.Block.Save`, `CK.Save.Block.Load` on the game state, or a Gameplay Effect granting them | the plugin reads the blocks from the game state |
| a change inside the plugin's file format, server rules or editor windows | the plugin source, only if no extension point exists | see "Editing the plugin" |

When unsure, prefer marking properties `SaveGame` and the `ICKSaveable` hooks before anything else.

## Hard rules

- Do not edit anything under the plugin folder unless the recipe above sends you there; never edit the engine.
- Code and comments are English; chat with the user in their language.
- Every Blueprint-visible and editable property has a tooltip comment, `Category` and, for numbers, `ClampMin` where a range makes sense.
- Saving, loading and restoring run on the server only (`HasAuthority()`); a client never calls the subsystem to save. Cosmetics run on every machine.
- Do not claim the build or the test passed unless the exit code of the build and the test run said so.
- Do not install Visual Studio or anything else yourself, and do not edit the engine: the user installs what `reference/build_and_diagnose.md` names.
