# CoopKit Ping

Copyright 2026 Ivan Iaitckikh.

CoopKit Ping lets co-op players point things out to each other without voice chat:

- **Ping what you look at.** A short press on the ping key places a ping where the crosshair hits; on an enemy or a vehicle the ping follows the actor.
- **Command wheel.** Hold the key, point with the mouse or the right stick, release: "Go here", "Danger", "Loot", "Enemy" or your own commands. Built on Enhanced Input; release in the center to cancel.
- **Markers on the HUD and in the world.** Every visible ping gets a marker that tracks it on screen, shows the distance and sticks to the screen edge with an arrow when the ping is off screen. A ping type can also spawn your own world marker actor (a beam, a decal) for the players who see it. The widgets are CommonUI widgets built in C++; restyle them in Blueprint.
- **Only your team receives your pings.** The filter works at the replication level: the server puts a ping only into the replicated lists of the players allowed to see it, so the other team never receives it - with the default replication, the Replication Graph and Iris alike.
- **Server-side rate limit.** A player can place only so many pings per time window; refusals reach the player as an event for your "denied" feedback.
- **Data assets with their own editor.** Ping types and rules are data assets with a Summary and a Validation tab. The plugin ships ready-made rules with four ping types. Ping rules also live in the CoopKit Hub, with a card that shows the ping types they use and a wizard that can make your own copies of those types for you.
- **An AI agent skill.** `coopkit-ping` lets Claude Code, Antigravity, Codex and other agents change the ping behavior from your own C++ module and build the project.

Two components are enough to start: one on the game state adds the ping component to every player controller, and one on your player controller turns the ping key into pings. The plugin changes none of your base classes.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

1. Install the plugin from Fab, then enable **CoopKit Ping** in *Edit > Plugins* and restart the editor. The plugin enables the engine plugins **Enhanced Input** and **CommonUI** (CommonUI is marked Beta in UE 5.6 and 5.7 and stable in UE 5.8).
2. **Set the game viewport client** (required by CommonUI): *Project Settings > Engine > General Settings > Default Classes > Game Viewport Client Class* = `CommonGameViewportClient`, or in `Config/DefaultEngine.ini`:

   ```ini
   [/Script/Engine.Engine]
   GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient
   ```

   Without it, CommonUI logs the error *"Using CommonUI without a CommonGameViewportClient derived game viewport client"* for every local player, and automation tests that treat errors as failures fail. If your project deliberately uses another viewport client, CoopKit Ping still works (its widgets never take focus or input): silence the check with `CommonUI.Debug.CheckGameViewportClientValid=0` under `[SystemSettings]` in `DefaultEngine.ini`.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel also lists the pings you see, with their type, object, author and distance in meters.

1. If you have not done it yet, set the game viewport client (Installation, step 2).
2. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitPing/Maps/L_PingDemo`, then click **Play** in the toolbar.
3. Under **1. Ping type:**, click a type, for example **Danger**. Under **2. Ping an object:**, click **Crate 1**. A red marker with its distance appears on the crate, and the panel lists your ping.
4. Click **What you look at**. The selected type marks the point in the middle of your view.
5. To send a ping to a second player, click **Play with 2 windows** on the panel. Play restarts as a listen server with two windows. Your Play settings stay as they were. Click **Crate 2** in one window. The marker appears in both windows.
6. Click **Clear my pings** to remove your own pings. The button shows only while you have pings.

How the demo is built:

- The game mode `BP_CKPingDemoGameMode` uses the game state `BP_CKPingDemoGameState` (the match component), the player controller `BP_CKPingDemoPlayerController` (the input component) and the HUD class **CKPing Demo HUD**, which draws the panel.
- The panel is C++, without Widget Blueprints. It calls only the Blueprint API of the player's **CKPing Controller Component**: Request Ping on the object, Ping At Crosshair, Request Remove Ping. The objects are the actors of the map with the tag `CKPingDemoTarget`; their second tag is the button's name.
- The middle mouse button still pings at the crosshair (tap) and opens the wheel (hold), as in your game.

In your project:

1. **Add the match component.** Add **CKPing Match Component** to your game state Blueprint. With no rules set it uses the plugin's default rules (*Project Settings > Plugins > CoopKit Ping > Default Rules*). With *Add Components* on (the default), it adds the ping controller component to every player controller on the server; that component adds the HUD layer for local players (*Create Hud* in the project settings).
2. **Add the input component** (**CKPing Input Component**) to your player controller Blueprint. It uses the plugin's input actions and contexts (`/CoopKitPing/Input`): the ping key on the middle mouse button and D-pad up, the wheel on the mouse and the right stick. To use your own keys, make a mapping context for `IA_CKPing` and set it as the component's **Ping Context**. Your own input code can drive the same functions instead: *Begin Ping Input*, *End Ping Input*, *Add Wheel Pointer Input*, *Set Wheel Stick*, *Open Wheel*, *Close Wheel*.
3. **Teams** (optional): implement `IGenericTeamAgentInterface` on your player controller, pawn or player state. Players see pings of their own team; a player without a team is everyone's ally. For other rules, override **Can Player See Ping** in a subclass of the match component.
4. **Feedback**: bind **On Ping Rejected** of the ping controller component (**Find Local Controller Component**) to play a "denied" sound or show a hint.

## Console commands for developers

The demo needs no console. For development and automation the plugin also has console commands: `ck.ping.Ping [sector]` pings at the crosshair with the quick type or a wheel sector, `ck.ping.Clear` removes every ping on the server, and `ck.ping.Debug 1` in the server window draws every live ping with its author and how many players see it (see *Settings, console and API* below).

## Overview map

`/CoopKitPing/Maps/L_PingOverview` shows every feature on its own stand. Open it and click **Play** (alone or as a listen server); each stand plays its scenario by itself, in a loop, and its sign says what happens:

1. **Ping what you look at**: a bot pings crates with the quick ping type; you get the markers with distances.
2. **A wheel of ping types**: a bot pings with each type of the wheel in turn.
3. **Pings follow targets**: a bot pings a moving crate with a type that follows targets.
4. **Server-side rate limit**: a bot sends a burst of pings; the sign shows how many the server accepted.
5. **Pings reach only your team**: bots of a red and a blue team ping in turn; the sign shows who may see each ping.

The bots are local players without a view of their own (`ACKPingShowcaseScenario::CreateBot`), so the plugin treats them as real players. The scenarios (`ACKPingShowcasePinger`, `ACKPingShowcaseTeams`) place pings through **CKPing Match Component** the way a player's request does: read them as examples of server-side pings. The stands (`ACKPingShowcaseStand`) are built from engine content only: move them, change their texts and color in the Details panel. On a dedicated server there are no bots; the scenarios then do nothing.

## Data assets

Create them in the Content Browser: *Add > CoopKit*, or make ping rules with **New Ping Rules** in the CoopKit Hub (below). A double click opens the CoopKit Ping data editor: *Properties*, *Summary* and *Validation* (problems, refreshed on every change).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKPingTypeData` (*Ping Type*) | Display name; icon (soft reference; empty: the marker shows its color only); color; lifetime in seconds; sound for every player who sees the ping; whether a ping on an actor follows it; whether the marker shows the distance and stays at the screen edge; an optional world marker actor class spawned locally for every player who sees the ping. Summary: how the ping looks, sounds and lives. | Display name not empty; lifetime above 0. Warning: no icon. |
| `UCKPingRulesData` (*Ping Rules*) | Wheel types (2 to 8, clockwise from the top) and the quick ping type; hold time before the wheel opens; wheel dead zone; trace distance, server distance tolerance and trace channel; rate limit (pings per window, window seconds); live pings per player (a new one replaces the oldest); visibility (*Allies* or *Everyone*); how often the server re-checks visibility; adding components. Summary: the wheel sector by sector, the rate limit and who sees pings. | 2 to 8 wheel sectors, none empty; a quick ping type; hold time, distance, rate limit, window and live pings above 0; dead zone from 0 to below 1; tolerance and refresh not negative. Warning: a type twice on the wheel. |

The plugin's own assets live in `/CoopKitPing/Data`: `DA_PingDefaultRules` and the types `DA_PingType_Go`, `DA_PingType_Danger`, `DA_PingType_Loot`, `DA_PingType_Enemy`. They have no icons: add your own textures to the types or make your own types.

### Ping rules card and the New Ping Rules wizard

Ping rules are an entity of the CoopKit Hub (*Tools > CoopKit Hub*, category **Ping Rules**). The hub lists them by asset name, because ping rules have no display name field.

- **Ping rules card** (double-click ping rules, or *Open* in the hub): Properties, **Preview**, **Parts**, Summary and Validation. The Parts tab shows the **Quick Ping** type and one row per wheel sector (**Wheel 1**, **Wheel 2**, ...), then an empty row while the wheel has room (a wheel has up to 8 sectors). Each row has a thumbnail, an own or shared label, and the buttons **Open**, **Replace** and **Create**. **Replace** with a type in the empty row adds a sector. **Replace** with *None* removes a sector (the sectors after it move up) or empties the quick ping. **Create** makes a new ping type `DA_PingType_<Name>_Quick` or `DA_PingType_<Name>_Wheel<N>` in the rules' folder, named after its row, and puts it there.
- **New Ping Rules wizard** (the hub button **New Ping Rules**, or *Add > CoopKit > Ping Rules* in the Content Browser): **Start From** (rules to copy from; leave it empty for the project's default rules from *Project Settings > CoopKit Ping*) and **Create Types** (on by default). With *Create Types* on, the wizard makes the rules' own copies of the ping types of the rules to start from, next to the rules. Each copy is named after its row, for example `DA_PingType_<Name>_Wheel1` or `DA_PingType_<Name>_Quick`. A type that stands both on the wheel and as the quick ping is copied once. With *Create Types* off, the new rules use the ping types of the rules to start from where they are. Click **Create** or **Cancel**.
- **Own and shared parts.** An own part lies in the rules' folder, and nothing outside the rules uses it. A ping type elsewhere is shared and is never moved or deleted. The plugin has functions that rename, duplicate and delete ping rules together with their own types. The hub has no buttons for them yet.
- **Folders.** New ping rules go to `/Game/CoopKit/PingRules/<Name>/`. To change that, set the key `EntityPathTemplate.Ping Rules` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Rules you already have stay where they are, and the hub lists them there. A folder that is not named after the rules (such as `/CoopKitPing/Data`) is never treated as their own folder. Only the ping types they use can be their own.

A ping type is not an entity of the hub. Create it with *Add > CoopKit > Ping Type*. It keeps its own data editor (Properties, Summary, Validation).

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-ping`. It works with Claude Code, Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to the pings, for example "only my squad sees my pings". In Claude Code you can also type `/coopkit-ping`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds a validation and an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitPing data in the open editor, without clicks. The editor module has `UCKPingAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKPingAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_ping` to `<YourProject>/.claude/skills/coopkit-ping` and to `<YourProject>/.agents/skills/coopkit-ping`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## How it works

- **Placing a ping.** The client traces from the camera along the view direction (rules: distance and channel) and sends the point and the actor it hit to the server through its controller component (reliable server RPC). The server checks the type (on the wheel or the quick type), the block tag `CK.Ping.Block.Ping` on the player's controller, pawn or player state, the distance from the player's view (trace distance plus tolerance; no line-of-sight check) and the rate limit, then creates the ping with a lifetime on the server clock.
- **Who receives it.** Each player controller carries the list of pings its player may see (a fast array, replicated to the owner only). The server writes a ping into the lists of the allowed players only. A player who joins a running game, a spectator, or a player whose team changes gets the right list when the controller component starts and on every visibility check (rules: *Visibility Refresh Seconds*; call **Refresh Visibility** after your game changes teams).
- **Markers.** The HUD layer creates a marker for every ping in its player's list, moves it every frame to the ping's place on screen (or to the screen edge, also for pings behind the camera) and hides it as soon as the server time passes the ping's end. The world marker actor of a type is spawned on the viewer's machine only and never replicates.
- **Wheel.** The input component times the hold of the ping key; once the wheel opens it adds the wheel mapping context with high priority, so the mouse and the right stick move the wheel pointer instead of the camera. The mouse moves the pointer by deltas, the stick sets its direction. Releasing the key pings with the selected sector's type; inside the dead zone it cancels.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Ping`):

- `CoopKit.Ping.Replication.DedicatedServer` / `.ListenServer` - three players in two teams in one editor process with 150 ms packet lag and 2% loss: a team A ping reaches the author and the ally and never reaches team B (neither the client nor its list on the server), a team B ping never reaches team A, a team A player joining the running game sees the live team A ping and not the team B ping, the ping expires for everyone.
- `CoopKit.Ping.RateLimit` - a client sends more pings than the rules allow: the server accepts exactly the limit, the client hears the refusals, after the window the server accepts again.
- `CoopKit.Ping.Rules.*` - wheel sectors, the rate window, marker placement on and off screen.
- `CoopKit.Ping.Overview` - the overview map on a listen server with a client: every stand is in place, every bot pings on its own, the client gets the bots' pings, the rate limit and team stands show their results.
- `CoopKit.Ping.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay` - the demo map loads with no warning in Map Check and Play; a listen server and a client click the panel's buttons (Danger on Crate 1, What you look at, Clear my pings) and both windows see the pings; Play with 2 windows restarts Play with two windows.
- `CoopKit.Ping.Data.TypeEditor`, `.RulesEditor`, `CoopKit.Ping.Tags.GetActorTags` - data editors and tag reading.
- `CoopKit.Ping.Hub.*`, `CoopKit.Ping.Workspace.*` - the CoopKit Hub (which plugin raises the hub window, entity types, New and Open), the New Ping Rules wizard with its ping types, the card's Quick Ping and Wheel parts, the workspace functions that rename, duplicate and delete ping rules with their own types, and the default rules in their older folder.
- `CoopKit.Ping.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.

## Limitations and known issues

- No icons ship with the plugin: the default markers show the type's color.
- The server checks the distance of a ping, not the line of sight from the player.
- Pings do not carry over seamless travel: a new map starts without pings.
- The default wheel shows sector labels; restyle it (or the marker) in a Blueprint subclass and set it in the project settings.
- Visibility by team was tested with the default replication system. With the Replication Graph or Iris the same guarantee comes from the engine: a player controller replicates only to its owner.
- Dedicated servers: tested in the editor on a dedicated server world. The plugin also compiles into the sample project's Server target, built on Unreal Engine 5.8 from source, and two clients join that packaged server in a smoke test. On UE 5.6 and 5.7 the Server target is not compiled, and any Server target needs an engine built from source.

## Integration

How CoopKit Ping talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Ping.Block.Ping` | Block | Anyone, on the player's controller, pawn or player state (`IGameplayTagAssetInterface` of the actor or its components) | The server refuses the player's pings (*Blocked*) |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKPingMatchComponent` | Game state | Yes (the rules reference) | Server: live pings, checks, rate limit, visibility; adds the controller component (rules: *Add Components*). Blueprintable: override *Can Player See Ping* |
| `UCKPingControllerComponent` | Player controller | Yes, the ping list to the owner only | Client requests (server RPCs), the player's visible pings, sound, world markers, HUD layer |
| `UCKPingInputComponent` | Local player controller | No | Ping key and command wheel on Enhanced Input; optional |
| `UCKPingHudWidget`, `UCKPingMarkerWidget`, `UCKPingWheelWidget` | Widgets (`UCommonUserWidget`) | No | HUD layer, marker, wheel; replace them in the project settings |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IGenericTeamAgentInterface` (engine, AIModule) on the player controller, pawn or player state | `GetGenericTeamId` | No team: the player sees every ping and everyone sees theirs |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKPingMatchComponent::OnPingCreated` | Server | The new ping (`FCKPingEntry`) |
| `UCKPingMatchComponent::OnPingEnded` | Server | Ping id (expired, replaced, removed, cleared) |
| `UCKPingControllerComponent::OnPingAdded` | Owning Client (and the listen server host for its own player) | The ping the player now sees |
| `UCKPingControllerComponent::OnPingRemoved` | Owning Client (and the listen server host) | Ping id |
| `UCKPingControllerComponent::OnPingRejected` | Owning Client | `ECKPingRejectReason`: *Rate Limited*, *Too Far*, *Blocked*, *Unknown Type*, *No Match*, *Nothing Hit* |
| `UCKPingInputComponent::OnWheelOpenChanged`, `OnWheelSectorChanged` | Local player | Open flag, sector index |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKPingControllerComponent::VisiblePings` (fast array of `FCKPingEntry`) | Owner only (`COND_OwnerOnly` on a player controller) | Property |
| `UCKPingMatchComponent::Rules` | Every client (`COND_None`) | Property |
| `UCKPingControllerComponent::ServerRequestPing`, `ServerRemovePing` | Owning client to server | Reliable: rare explicit player actions |
| `UCKPingControllerComponent::ClientPingRejected` | Server to owning client | Unreliable: a lost refusal only loses feedback |

Times are server world times (`GetServerWorldTimeSeconds`), so every machine hides an expired ping at the same moment.

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKPingSettings` | Project settings: *Default Rules*, *Create Hud*, *Hud Z Order*, *Hud/Marker/Wheel Widget Class*; section `[/Script/CoopKitPing.CKPingSettings]` in `DefaultCoopKitPing.ini` | Runtime module `CoopKitPing` |
| Match component | Blueprint and C++ (server): `SetRules`, `ClearPings`, `RefreshVisibility`, `CanPlayerSeePing`; C++: `CreatePing`, `RemovePing`, `GetLivePings` | Runtime module |
| Controller component | Blueprint and C++: `PingAtCrosshair`, `TraceFromView`, `RequestPing`, `RequestRemovePing`, `GetVisiblePings`, `SeesPing` | Runtime module |
| Input component | Blueprint and C++ (local): `BeginPingInput`, `EndPingInput`, `OpenWheel`, `CloseWheel`, `AddWheelPointerInput`, `SetWheelStick`, `IsWheelOpen`, `GetSelectedSector`, `GetWheelTypes` | Runtime module |
| `ACKPingDemoHUD` | HUD class of the demo game mode: the panel of buttons and the mouse cursor for the local player; set it as the HUD Class of your own test map's game mode to get the same panel | Runtime module |
| `ck.ping.Ping [sector]` | Console command, local player: ping at the crosshair (quick type or a wheel sector) | Any build |
| `ck.ping.Clear` | Console command, server: remove every ping | Any build |
| `ck.ping.Debug` | Console variable, 0/1: the server draws every live ping (debug draw is compiled out of Shipping) | Any build |
| `LogCKPing` | Log category | |

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Ping Rules** | `UCKPingRulesData` with the class metadata `CoopKitEntity="Ping Rules"`, `CoopKitEntityFolder="PingRules"` | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKPingHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitPingEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Ping Rules`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Enhanced Input | Required (enabled by the plugin) | Stable |
| CommonUI | Required (enabled by the plugin); needs `CommonGameViewportClient` (see Installation) | Beta in UE 5.6 and 5.7, stable in UE 5.8 |

## Changelog

See `CHANGELOG.md` next to `CoopKitPing.uplugin` and the Changelog section of the Fab listing.
