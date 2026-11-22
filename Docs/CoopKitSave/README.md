# CoopKit Save

Copyright 2026 Ivan Iaitckikh.

CoopKit Save saves a co-op game on the host and gives every player their progress back, also when they join later:

- **The world and every player.** The game state, actors placed on the map (moved, changed or destroyed), actors spawned during play (spawned again on load), and every player's player state and pawn with the place it stood. You mark what matters: a **CoopKit Saveable** component (or the `ICKSaveable` interface) on an actor, and the **SaveGame** flag on its properties.
- **Players who are not there today.** A friend who missed this evening keeps their progress: their record moves from save to save. A player who joins a running game, or reconnects, gets their own record back at once.
- **The host saves, everyone sees it.** Saving and loading run on the server only (listen host, dedicated server or standalone game); clients get the restored state through normal replication.
- **No frame hitch on disk.** The world is captured on the game thread, the file is written and read in the background through the engine's platform save system (`SaveGameToSlot` storage: PC, consoles).
- **Slots, versions, migration.** Named slots with a slot list for menus, a limit per profile, autosave by timer and when a player leaves. Saves carry the plugin's format version and your game's data version: old saves keep loading, and your game converts old values in one event.
- **A data asset with its own editor.** The save profile has a Summary (what is saved, when, how players are matched) and a Validation tab. Save profiles also live in the CoopKit Hub, with a card and a wizard that makes a new profile.
- **An AI agent skill.** `coopkit-save` lets Claude Code, Antigravity, Codex and other agents save a new value of your game, keep old saves working after a change and build the project from your own C++ module.

The plugin changes none of your base classes: no game mode, game instance or save game class to inherit from.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Save** in *Edit > Plugins* and restart the editor. The plugin needs no other plugin.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows where you stand and lists the saved slots.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitSave/Maps/L_SaveDemo`, then click **Play** in the toolbar. Three crates stand on the map.
2. Click **Save the game**. The game is saved to the slot *Quick*, and a button such as **Load Quick save (saved 12 s ago, 2 players)** appears under **Load a slot:**.
3. Click **Move every player and crate**. You and the crates move to new spots; the panel shows your new position.
4. Click that **Load Quick save** button. You and every crate are back where you were at the save.
5. To see a second player come back too, click **Play with 2 windows** on the panel. Play restarts as a listen server with two windows. Your Play settings stay as they were. The host saves and loads, so the buttons are in window 1. Window 2 shows that player's position ("You stand at x .. m, y .. m."). The load puts that player back as well.

How the demo is built:

- The map has three replicated crates `BP_CKSaveDemoCrate`. Each crate has a CoopKit Saveable component with a *Save Id* (`Crate_A`, `Crate_B`, `Crate_C`).
- The game mode `BP_CKSaveDemoGameMode` uses the HUD class **CKSave Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. It calls only the Blueprint API of the save subsystem: Save To Slot, Get Slots, Load From Slot, On Save Completed and On Load Completed. **Move every player and crate** stands in for gameplay: it moves the pawns and crates on the server.
- The demo uses the plugin's built-in profile (prefix `CoopKit_`). `DA_SaveDemoProfile` is an example profile with autosave every 5 minutes and when a player leaves; set it as *Default Profile* in the project settings to try it.

In your project:

1. **Mark what is saved.** Tick **SaveGame** (Blueprint: *Variable > Advanced > SaveGame*; C++: `UPROPERTY(SaveGame)`) on the properties of your game state, player state, pawn and their components that make up progress. Add a **CoopKit Saveable** component to every other actor whose state matters (chests, doors, pickups, buildings), or implement `ICKSaveable` on it. Actors placed on a map and actors spawned during play both work.
2. **Pick a profile.** Create a *Save Profile* (*Add > CoopKit*), set its *Slot Prefix* (your game's name) and set it as *Default Profile* in *Project Settings > Plugins > CoopKit Save*, or call **Set Profile** on the subsystem.
3. **Save and load from your menu**, on the host: **Save Game To Slot (CoopKit)** and **Load Game From Slot (CoopKit)** (latent nodes with a *Completed* pin), or **Get CoopKit Save Subsystem** > **Save To Slot** / **Load From Slot** and the events *On Save Completed* / *On Load Completed*. **Get Slots** lists the slots with map, time and player names.
4. **React to a load** where a value needs more than a replicated property: implement `ICKSaveable::OnAfterLoad` (rebuild a mesh from a saved level, refill a cache). RepNotify functions run on clients as usual, not on the server.
5. **Identify players.** Players are matched to their records by unique net id (Steam, EOS). In the editor the Null online service gives every PIE window a new random id each launch, so a save made in one Play session does not find its players in the next: implement `ICKSavePlayerIdentity` on your player state to return your own key (an account id of your backend, a character slot) when you need that.

A client asks the host to save through your own Server RPC (for example on the player controller) that calls **Save To Slot** on the server; the plugin does not save on clients.

**A save in memory, without a slot.** **Capture To Bytes** writes the world and the players into an array of bytes. It uses no slot and no disk, and it does not change the session save. **Load From Bytes** applies such an array the way Load From Slot does, with the same checks. Both nodes run on the server only, and a client gets *NotAuthority*. The bytes use the slot file format. You can load a slot file's bytes with Load From Bytes, and you can write the bytes of a capture to disk as a slot file. Capture To Bytes returns *Blocked* while `CK.Save.Block.Save` is set. Load From Bytes returns the final result at once and also fires *On Load Completed*. If it has to travel to the saved map first, it returns *Pending*, and the result arrives in *On Load Completed*. If the load did not start, you get *Busy*, *Blocked* or *NotAuthority*.

## Console commands for developers

The demo needs no console. For development and automation the plugin also has console commands on the server: `ck.save.Save <slot>`, `ck.save.Load <slot>`, `ck.save.Delete <slot>`, `ck.save.List` and `ck.save.Dump [slot]` (slot *Quick* by default), and `ck.save.Debug 1` shows the save state as an overlay (see *Settings, console and API* below).

## Overview map

The demo map `/CoopKitSave/Maps/L_SaveOverview` has four stands. Open it and click **Play**: each stand plays by itself with bot players, and the sign at each stand says what happens. A save and a load cover the whole map, so the stands take turns.

1. **Crates.** Three crates, two placed on the map and one spawned during play, are saved. Then two crates are moved, the spawned one is smashed and a new one is added. The load restores the world as it was saved.
2. **Player coins.** Two bots have coins, a SaveGame property on the player state. After the save both lose their coins. The load gives each bot its own coins back.
3. **Slots.** One bot saves three named slots: Camp, Cave and Boss. The stand reads **Get Slots**, newest first, and loads the oldest one.
4. **Rejoin.** A bot collects coins and leaves, and the subsystem records it. When the bot joins again, its coins are applied.

The scenarios call the same subsystem functions your game would call. A bot gets its name only after it logs in, so for a bot the scenario calls **Restore Player**. On a dedicated server the map has no bots, so the player stands do nothing there.

The map uses the profile `DA_SaveOverviewProfile`. Its slot prefix is `CKShowcase_`, players are matched by name, and pawn positions are not saved. The stands delete their slots when they finish, so nothing stays on your disk.

## Data assets

Create them in the Content Browser: *Add > CoopKit*, or make a save profile with **New Save Profile** in the CoopKit Hub (below). A double click opens the CoopKit Save data editor: *Properties*, *Summary* (what is saved, when, how players are matched) and *Validation* (problems, refreshed on every change).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKSaveProfileData` (*Save Profile*) | Slot prefix, maximum slots (0: no limit), platform user index; autosave slot, autosave interval (0: off) and autosave when a player leaves; what is saved (game state, players, player pawns, pawn position); restoring players who join, remembering players who leave, how players are matched (*Unique Net Id* or *Player Name*); travelling to the saved map on load; Game Data Version. Summary: what is saved, where and when. | Slot prefix not empty, letters, digits, '-' and '_' only; maximum slots and autosave interval not negative; autosave slot a valid name when autosave is on; Game Data Version at least 1; restoring or remembering players needs *Save Players*. Warning: autosave more often than every 10 s. |

### Save profile card and the New Save Profile wizard

Save profiles are an entity of the CoopKit Hub (*Tools > CoopKit Hub*, category **Save Profile**). The hub lists them by asset name, because a save profile has no display name field.

- **Save profile card** (double-click a save profile, or *Open* in the hub): Properties, **Preview**, **Parts**, Summary and Validation. A save profile refers to no other asset, so the Parts tab is empty and says so.
- **New Save Profile wizard** (the hub button **New Save Profile**, or *Add > CoopKit > Save Profile* in the Content Browser): **Start From** (a save profile to copy values from; leave it empty for the plugin's defaults) and **Slot Prefix** (leave it empty to keep the prefix of the *Start From* profile, or the plugin's default). Click **Create** or **Cancel**.
- **Folders.** New save profiles go to `/Game/CoopKit/SaveProfiles/<Name>/`. To change that, set the key `EntityPathTemplate.Save Profile` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Profiles you already have stay where they are, and the hub lists them there.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-save`. It works with Claude Code, Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to what your game saves, for example "the chest remembers how many coins it holds" or "I renamed a variable, keep the old saves working". In Claude Code you can also type `/coopkit-save`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitSave data in the open editor, without clicks. The editor module has `UCKSaveAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKSaveAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_save` to `<YourProject>/.claude/skills/coopkit-save` and to `<YourProject>/.agents/skills/coopkit-save`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## How a save works

- **What is in a save:** the game state's SaveGame properties (and its components'), one record per saveable actor (key, transform, SaveGame properties of the actor and of its components that have any), the keys of placed saveable actors destroyed before the save, and one record per player (player state and pawn, with the pawn's position and the map).
- **Keys of placed actors:** the Saveable component's *Save Id*, otherwise the actor's name on its map (with the sub-level name for actors of a streaming sub-level). Give an actor a Save Id before you rename it, so older saves still find it.
- **Actors spawned during play** get a key of their own and are spawned again on load from their class; their SaveGame properties are set before `BeginPlay`, those of their Blueprint components right after. On load, saveable actors spawned since the save are destroyed.
- **Destroyed placed actors** (a picked-up item, a broken door) are destroyed again on load.
- **Streaming sub-levels:** actors of a sub-level that is not loaded at save time keep the state of the previous save of the map; a sub-level that streams in after a load gets its part of the save.
- **Players:** a load restores the players in the game at once, players who join later when they join (the pawn when it spawns). A player who leaves is recorded at once, so a reconnect restores them and the next save keeps their progress. The pawn's position is restored only on the map it was saved on.
- **Another map:** loading a save of another map travels the server and its players there first (*Travel To Saved Map*), then applies the save before any actor's `BeginPlay`.
- **Versions:** the save holds the plugin's format version (older formats are upgraded on load) and your profile's *Game Data Version*. A save with a lower Game Data Version runs **On Migrate Save** (rename or remove records) and every saveable gets the record's version in `OnAfterLoad`. Added and removed SaveGame properties need no migration; a renamed property needs a property redirect (`[CoreRedirects]`). A save of a newer plugin or game version does not load (*Newer Format*, *Newer Game Data*).

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Save`):

- `CoopKit.Save.Replication.DedicatedServer` / `.ListenServer` - two Play sessions, 150 ms packet lag and 2% loss. First session: the world and every player change and the host saves. Second session, one client fewer: the host loads, and the world and the players match on every client; the missing player joins the running game and gets their record; a player who is kicked and joins again gets what they had when they left.
- `CoopKit.Save.World.SaveLoad` - a load undoes changes made after the save, spawned actors come back, newer ones go; request checks (slot names, block tag, busy, slot limit, delete).
- `CoopKit.Save.Migration.Format` - a save of format version 1 kept in the test still loads; newer formats are refused; the upgrade steps run in order.
- `CoopKit.Save.Migration.GameData` - a save of Game Data Version 1 loaded by version 2: On Migrate Save renames a record, OnAfterLoad converts old values; the reverse load is refused.
- `CoopKit.Save.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay` - the demo map loads with no warning in Map Check and Play; on a listen server with a client the host clicks Save the game, Move every player and crate, then Load Quick save: both players and the crates are back at their saved spots, in the client's window too; Play with 2 windows restarts Play with two windows.
- `CoopKit.Save.Data.ProfileEditor` - the profile's factory, editor window and validation.
- `CoopKit.Save.Hub.*`, `CoopKit.Save.Workspace.*` - the CoopKit Hub (which plugin raises the hub window, entity types, New and Open), the New Save Profile wizard, the card, the workspace functions that rename, duplicate and delete save profiles, and the demo profile in its older folder.
- `CoopKit.Save.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.
- `CoopKit.Save.Overview` - a listen server and a client on the overview map with 150 ms packet lag and 2% loss. The stands, scenarios and placed crates are in place. The bots are players and mannequins on both machines, and the client sees the coins. The signs show that a world load puts the crates back, that each player's own coins come back, that the three slots are listed and the oldest one is loaded, and that a bot which left and joined again gets its coins. The stands leave no slot on disk.

## Limitations and known issues

- References between actors spawned during play (a chest pointing to the item inside) are not restored; save a key or an id instead. References to assets and to placed actors are restored.
- Members of a struct property are saved only when they are marked SaveGame too.
- SaveGame properties that do not replicate change on the server only; clients see what replication sends them.
- Seamless travel does not apply player records on the new map: travel with a save (Load From Slot of a save of that map) or carry values in `APlayerState::CopyProperties`.
- Capturing the world walks every actor on the game thread: autosave a large world every few minutes, not every few seconds.
- Load From Bytes only checks the start of the data. It returns *Corrupt* for bytes that do not start like a save, but a save cut off after its header is not detected, and in UE 5.8 the engine hits a fatal assertion on it. Pass bytes exactly as Capture To Bytes or a slot file gave them.
- Dedicated servers: tested in the editor on a dedicated server world. The plugin also compiles into the sample project's Server target, built on Unreal Engine 5.8 from source, and two clients join that packaged server in a smoke test. On UE 5.6 and 5.7 the Server target is not compiled, and any Server target needs an engine built from source.

## Integration

How CoopKit Save talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Save.Block.Save` | Block | Your game (game state tags) | No save or autosave starts while the tag is present (a boss fight, a cutscene). Save To Slot returns *Blocked*. |
| `CK.Save.Block.Load` | Block | Your game (game state tags) | No load starts while the tag is present. |

Block tags are read from the game state's `IGameplayTagAssetInterface` and from its components that implement it. The plugin publishes no state tags: its events are delegates of the subsystem.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKSaveableComponent` (*CoopKit Saveable*) | Any actor whose state is saved | No | Marks the actor as saveable. *Save Id* (placed actors: key in saves), *Save Transform*, *Save Components*. `Find Saveable Component`, `Is Actor Saveable`. |
| `UCKSaveSubsystem` | Game instance subsystem (no setup) | No | Save, load, delete, slots, players, events. `Get CoopKit Save Subsystem`. |
| `UCKSaveWorldSubsystem` | World subsystem (no setup) | No | Placed actor registry, world capture and apply. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKSaveable` (actor or component) | `OnBeforeSave()` | Nothing. Copy runtime state into SaveGame properties here. An actor that implements it is saveable without a component. |
| `ICKSaveable` (actor or component) | `OnAfterLoad(SavedGameDataVersion)` | Nothing. Rebuild runtime state from SaveGame properties; convert values of an older Game Data Version. |
| `ICKSavePlayerIdentity` (player state) | `GetSavePlayerKey()` | The profile's *Player Key Source*: unique net id, or the player name. Return an empty string to fall back to it. |

C++ projects may also bind `UCKSaveSubsystem::ResolvePlayerKey`, which comes before the interface.

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKSaveSubsystem::OnSaveCompleted` | Server | Slot name, `ECKSaveResult` |
| `UCKSaveSubsystem::OnLoadCompleted` | Server | Slot name, `ECKSaveResult` |
| `UCKSaveSubsystem::OnPlayerRestored` | Server | Player state (its pawn is restored when it spawns) |
| `UCKSaveSubsystem::OnMigrateSave` | Server | Save game, from and to Game Data Version |
| `ICKSaveable::OnBeforeSave`, `OnAfterLoad` | Server | Saved Game Data Version |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| (none) | The plugin replicates nothing of its own: restored SaveGame properties, transforms, spawned and destroyed actors reach clients through your actors' own replication. | - |
| `AController::ClientSetLocation` (engine RPC) | Remote owning client of a restored pawn: replicated movement does not reach the owner. Not sent to the listen server host, whose pawn the server places directly | Reliable (engine) |

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKSaveSettings` | Project settings: *Default Profile*; section `[/Script/CoopKitSave.CKSaveSettings]` in `DefaultCoopKitSave.ini` | Runtime module `CoopKitSave` |
| `UCKSaveSubsystem` | Blueprint and C++ (server): `SaveToSlot`, `LoadFromSlot`, `CaptureToBytes`, `LoadFromBytes`, `DeleteSlot`, `RememberPlayer`, `RestorePlayer`, `SetProfile`; any machine: `GetSlots`, `DoesSlotExist`, `IsBusy`, `GetSessionSave`, `GetPlayerKey` | Runtime module |
| *Save Game To Slot (CoopKit)*, *Load Game From Slot (CoopKit)* | Blueprint latent nodes (server) | Runtime module |
| `UCKSaveGame` | The save itself; migration edits: `RemoveActorRecord`, `RenameActorKey`, `RemovePlayerRecord`, `RenamePlayerKey` | Runtime module |
| `ACKSaveDemoHUD` | HUD class of the demo game mode: the panel of buttons and the mouse cursor for the local player; set it as the HUD Class of your own test map's game mode to get the same panel | Runtime module |
| `ck.save.Save <slot> [name]`, `ck.save.Load <slot>`, `ck.save.Delete <slot>`, `ck.save.List`, `ck.save.Dump [slot]` | Console commands, server (slot *Quick* by default) | Any build |
| `ck.save.Debug` | Console variable, 0/1: overlay | Non-Shipping builds |
| `LogCKSave` | Log category | |

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Save Profile** | `UCKSaveProfileData` with the class metadata `CoopKitEntity="Save Profile"`, `CoopKitEntityFolder="SaveProfiles"` | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKSaveHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitSaveEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Save Profile`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| (none) | The plugin uses engine modules only (Engine, GameplayTags, DeveloperSettings, CoreOnline). | - |

## Changelog

See `CHANGELOG.md` next to `CoopKitSave.uplugin` and the Changelog section of the Fab listing.
