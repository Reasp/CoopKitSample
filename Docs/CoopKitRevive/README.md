# CoopKit Revive

Copyright 2026 Ivan Iaitckikh.

CoopKit Revive gives a co-op game the life cycle of its players, from the lethal hit to the next life:

- **Down, not dead.** A lethal hit puts the body down: it crawls and bleeds out, and a teammate can revive it by standing next to it. Your game decides when a hit is lethal; the plugin owns no health.
- **Death, spectating, respawn.** A dead player watches the body for a moment, then spectates a living teammate and respawns by timer, at the next checkpoint, or never, as the rules say. Respawns per player can be limited.
- **Checkpoints and team wipe.** Placeable checkpoints decide where players come back. When nobody is left standing, the plugin tells your game, or brings everyone back at the checkpoint.
- **Works with or without GAS.** State comes as Gameplay Tags on the plugin's components. When a pawn has an Ability System Component, the same tags appear there, the plugin sends Gameplay Events, and a Gameplay Effect can block going down, reviving or respawning.
- **Data assets with their own editor.** Body data and match rules are data assets with a Summary (the timeline a player goes through) and a Validation tab. Revive rules also live in the CoopKit Hub, with a card that shows the body data they use and a wizard that can make that body data for you.
- **An AI agent skill.** `coopkit-revive` lets Claude Code, Antigravity, Codex and other agents change the down, revive and respawn behavior from your own C++ module and build the project.

One component on the game state is enough to start: it adds everything else to your players. The plugin changes none of your base classes: no game mode, character or player controller to inherit from.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Revive** in *Edit > Plugins* and restart the editor. The plugin enables the engine plugin **Gameplay Abilities**; your pawns do not need an Ability System Component.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel lists the players (*Player 1*, *Player 2*, ...), the state of each body, and the buttons for that player.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitRevive/Maps/L_ReviveDemo`, then click **Play** in the toolbar.
2. Click **Down** on your row (*Player 1 (you)*). You go down, and the line counts down the 20 seconds until you bleed out. Click **Give up**. You die, the camera stays on your body for 2 s, and you respawn after 8 s.
3. A revive needs a teammate. Click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
4. In window 1, click **Down** on the *Player 2* row. Window 2 says *You are down*.
5. In window 1, click **Go to**, then **Revive** on the *Player 2* row. After 3 seconds Player 2 stands again. A revive from too far away is refused, so click **Go to** first.
6. Now the other way round. In window 1, click **Down** on your own row. In window 2, click **Go to**, then **Revive** on the *Player 1* row.
7. In window 1, click **Kill** on the *Player 2* row. Player 2 respawns after 8 s, or at once when you click **Respawn**.

How the demo is built:

- The game mode `BP_CKReviveDemoGameMode` uses the game state `BP_CKReviveDemoGameState`. The game state carries the match component with the rules `DA_ReviveDemoRules` (respawn after 8 s, 2 s death camera) and the body data `DA_ReviveDemoBody` (20 s bleed-out, 3 s revive). A start checkpoint and a second checkpoint at the far end of the floor show where players respawn.
- The game mode uses the HUD class **CKRevive Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API. **Down** and **Kill** call the body component, and **Respawn** calls **Respawn Player** on the match component. These run on the server, so they are on the host's panel. **Revive**, **Stop reviving**, **Give up** and **Watch the next teammate** call the controller component.
- **Go to** stands in for walking. The demo player controller **CKRevive Demo Player Controller** moves your pawn next to the teammate on the server. A client asks the server through an RPC.
- The demo uses the engine's `DefaultPawn`, which has no Character Movement, so crawling is visible only on a `Character`.

In your project:

1. **Add the match component.** Add **CKRevive Match Component** to your game state Blueprint and set its **Rules** (or set *Default Rules* in the project settings). With *Add Components* on (the default), it adds the player component to every player state, the controller component to every player controller and the body component to every pawn, on the server.
2. **Down the body when health runs out.** On the server, call **Down** on the pawn's body component (**Find Body Component**) when your damage code would kill it; it returns *Downed*, or *Dead* when the body may not go down. Restore health in **On Revived** (the event carries *Revived Health Fraction*). Call **Kill** for damage that must kill at once.
3. **Bind the player's keys** to the controller component (**Find Local Controller Component**): *Request Revive* with *Find Revive Target* on press, *Stop Revive* on release, *Give Up*, *Spectate Next* and *Spectate Previous*. The component sends them to the server, which checks distance, states and blocks.
4. **Place checkpoints** (`ACKReviveCheckpoint`) if players should come back near the action: a player walking into the volume activates it. Without checkpoints, players respawn at the game mode's player starts.
5. **Show the state** in your HUD from the body component (*Get State*, *Get Bleed Out Seconds Left*, *Get Revive Progress*) and the player component (*Get Life State*, *Get State Seconds Left*, *Get Respawns Left*).

## Console commands for developers

The demo needs no console. For development and automation the plugin has console commands that run on the server: `ck.revive.Down`, `ck.revive.Revive`, `ck.revive.Kill` and `ck.revive.Respawn`. Each takes a player index in the game state's player list (0 by default). `ck.revive.Debug 1` shows an overlay with your life state, every player's body and the active checkpoint.

## Overview map

`/CoopKitRevive/Maps/L_ReviveOverview` shows the life cycle on five stands and a welcome sign. Open it and press Play (alone or as a listen server); each stand plays its scenario by itself, in a loop, and its sign says what happens:

1. **Down, not dead**: a bot goes down, bleeds out on the server clock, dies and respawns by the rules' timer.
2. **Hold to revive a teammate**: a bot revives a downed teammate through its controller component, walks past the break distance once (the server stops the revive), comes back, starts again and finishes. The downed bot stands up.
3. **Death camera and spectating**: a downed bot gives up, the camera stays on the body, then the bot spectates living teammates in turn until it respawns.
4. **Checkpoints set the respawn point**: a bot walks between two checkpoints; the pad of the active checkpoint lights up on every machine, and the bot respawns there after it is killed.
5. **Body and rules are data assets**: two bots take the same hit; one body data asset lets the body go down, the other has *Can Be Downed* off, so that bot dies at once.

The bots are local players without a view of their own (`ACKReviveShowcaseScenario::CreateBot`), so the plugin treats them as real players. The scenario (`ACKReviveShowcaseLife`, one class with a mode per stand) makes the calls your game makes: **Down** and **Kill** on the body component, *Request Revive*, *Give Up* and *Spectate Next* on the controller component. The map's game mode `BP_CKReviveOverviewGameMode` (a child of the demo game mode, same game state and rules) uses the plugin's showcase pawn, which shows its body state on every machine: color, a squashed or flat shape, and a label with the name, the bleed-out countdown or the revive progress. The engine's default pawn shows none of this, which is why the map has its own. The body data of the stands is in `/CoopKitRevive/Showcase`. The stands (`ACKReviveShowcaseStand`) are built from engine content only: move them, change their texts and color in the Details panel.

The match has one set of rules and one active checkpoint: every respawn uses the demo rules' timer (8 s), and bots of other stands that respawn at the stand 4 checkpoint are moved back to their own stand on the server in the same frame. The team never wipes on this map: you stay standing. On a dedicated server there are no bots; the scenarios then do nothing.

## Data assets

Create them in the Content Browser: *Add > CoopKit*, or make revive rules with **New Revive Rules** in the CoopKit Hub (below). A double click opens the CoopKit Revive data editor: *Properties*, *Summary* (what a player goes through with these values) and *Validation* (problems, refreshed on every change).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKReviveBodyData` (*Revive Body Data*) | Whether the body may go down; bleed-out seconds (0: never) and whether the timer pauses during a revive; downs before death (0: no limit); crawl speed multiplier; whether the player may give up; revive seconds, range and break distance; health fraction to restore after a revive. Summary: the timeline down -> revive -> death. | Revive seconds and range above 0; break distance at least the range; bleed-out and downs not negative; crawl multiplier between 0 and 1; health fraction above 0 and at most 1. Warning: bleed-out not longer than the revive while the timer does not pause. |
| `UCKReviveRulesData` (*Revive Rules*) | Respawn mode (*Never*, *Timer*, *Next Checkpoint*), respawn delay, respawns per player (-1: unlimited), whether the old body is destroyed, death camera seconds, spectating teammates, team wipe action (*Event Only*, *Respawn All At Checkpoint*) and its delay, adding components, body data for added bodies, remembering deaths across a reconnect. Summary: the path dead -> spectating -> respawn. | Delays not negative; respawns at least -1. Warnings: a respawn delay shorter than the death camera; rules with no way back (*Never* and *Event Only*). |

### Revive rules card and the New Revive Rules wizard

Revive rules are an entity of the CoopKit Hub (*Tools > CoopKit Hub*, category **Revive Rules**). The hub lists them by asset name, because revive rules have no display name field.

- **Revive rules card** (double-click revive rules, or *Open* in the hub): Properties, **Preview**, **Parts**, Summary and Validation. The Parts tab shows the **Body Data** the rules give to the bodies the match adds. The row has a thumbnail, an own or shared label, and the buttons **Open**, **Replace** and **Create**. **Create** makes a new body data `DA_ReviveBody_<Name>` in the rules' folder and gives it to the rules. **Replace** with *None* makes the rules use the *Default Body Data* in *Project Settings > CoopKit Revive*.
- **New Revive Rules wizard** (the hub button **New Revive Rules**, or *Add > CoopKit > Revive Rules* in the Content Browser): **Start From** (rules to copy values from; leave it empty for the plugin's defaults), **Create Body Data** (on by default) and **Body Data**. With *Create Body Data* on, the wizard makes the rules' own body data `DA_ReviveBody_<Name>` next to them. It copies the values of the body data you pick. If you pick none, it copies the body data of the *Start From* rules, and if there are none, the plugin's defaults. With *Create Body Data* off, the rules use the body data you pick where it is. If you pick none, they use the body data of the *Start From* rules, or the project's default. Click **Create** or **Cancel**.
- **Own and shared parts.** An own part lies in the rules' folder, and nothing outside the rules uses it. Body data elsewhere is shared and is never moved or deleted. The plugin has functions that rename, duplicate and delete revive rules together with their own body data. The hub has no buttons for them yet.
- **Folders.** New revive rules go to `/Game/CoopKit/ReviveRules/<Name>/`. To change that, set the key `EntityPathTemplate.Revive Rules` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Rules you already have stay where they are, and the hub lists them there. A folder that is not named after the rules (such as the demo's `/CoopKitRevive/Data`) is never treated as their own folder. Only the body data they use can be their own.

Body data is not an entity of the hub. Create it with *Add > CoopKit > Revive Body Data*. It keeps its own data editor (Properties, Summary, Validation). A pawn that needs other numbers than the rules give (a heavy character, an AI teammate) gets its own body data in the *Body Data* field of its **CKRevive Body Component**.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-revive`. It works with Claude Code, Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to the revive loop, for example "a revived player runs faster for three seconds". In Claude Code you can also type `/coopkit-revive`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds a validation and an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitRevive data in the open editor, without clicks. The editor module has `UCKReviveAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKReviveAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_revive` to `<YourProject>/.claude/skills/coopkit-revive` and to `<YourProject>/.agents/skills/coopkit-revive`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Life cycle

- **Body** (`ECKReviveBodyState`, on the pawn): *Alive* -> *Downed* -> *Alive* (revived) or *Dead* (bled out, killed, gave up, or one down too many). The bleed-out timer and the revive run on the server clock; clients compute progress locally, without per-frame replication.
- **Player** (`ECKRevivePlayerState`, on the player state): *Playing* -> *Dead* (death camera) -> *Spectating* or *Waiting To Respawn* -> *Playing* on a new pawn. Respawns use the game mode: `RestartPlayerAtTransform` at a spawn point of the active checkpoint, or `RestartPlayer` without one, so your game mode's pawn class and spawn rules apply.
- **Reviving** stops when the reviver walks farther than the break distance, goes down, dies, releases the key or gets a block tag.
- **Team wipe:** when no player has a standing body, the game state gets `CK.Revive.State.TeamWiped` and **On Team Wiped Changed** fires; with *Respawn All At Checkpoint* every player respawns after the delay.
- **Reconnect:** a player who died, left and joins the same match again comes back dead, with the same respawns left (matched by unique net id).
- **Seamless travel** starts every player alive on the new map: state does not carry over.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Revive`):

- `CoopKit.Revive.Replication.DedicatedServer` / `.ListenServer` - a server and two clients in one editor process with 150 ms packet lag and 2% loss: a player goes down, a client revives it through its controller component, the player goes down again and bleeds out, spectates a teammate and respawns on a new pawn; a client joining the running match sees a downed player.
- `CoopKit.Revive.Gas.ReplicatedTags` - a pawn with an Ability System Component gets the state tags on the server and the clients and the Downed Gameplay Event; a block tag on the ASC stops a revive.
- `CoopKit.Revive.Overview` - a listen server and a client on the overview map with 150 ms packet lag and 2% loss: the stands, scenarios and checkpoints are in place and the bots are players; the client sees a bot down with its countdown, one bot reviving another, a bot spectating and a checkpoint other than the start one active; the signs show the revive stopped by distance, the data asset comparison ("Same hit: Ivy is down, Jay is dead.") and a timed respawn.
- `CoopKit.Revive.Rules.*`, `CoopKit.Revive.Data.*Editor` - timers and rules, data editors.
- `CoopKit.Revive.Hub.*`, `CoopKit.Revive.Workspace.*` - the CoopKit Hub (which plugin raises the hub window, entity types, New and Open), the New Revive Rules wizard with its body data, the card's Body Data part, the workspace functions that rename, duplicate and delete revive rules with their own body data, and the demo rules in their older folder.
- `CoopKit.Revive.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.
- `CoopKit.Revive.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay` - the demo map loads with no warning in Map Check and Play. In one window, the player goes down, gives up and respawns by the timer, all with the panel's buttons. On a listen server with a client, the host puts the client down, goes to it and revives it. The client goes to the downed host and revives it. The host kills the client and respawns it. Every step is a click on a panel. Play with 2 windows restarts Play with two windows.

## Limitations and known issues

- The plugin owns no health or damage: your game calls **Down** and **Kill** and restores health in **On Revived**.
- Crawling changes the walk speed of Character Movement only; other movement components keep their speed. Downed animation is your animation Blueprint's job (read *Get State* or the tag).
- Spectating sets the player controller's view target on the server; a custom camera manager that ignores view targets needs its own spectator camera.
- Players on the same team (`IGenericTeamAgentInterface`) or without teams can revive each other; implement `ICKRevivable` for other rules.
- Dedicated servers: tested in the editor on a dedicated server world. The plugin also compiles into the sample project's Server target, built on Unreal Engine 5.8 from source, and two clients join that packaged server in a smoke test. On UE 5.6 and 5.7 the Server target is not compiled, and any Server target needs an engine built from source.

## Integration

How CoopKit Revive talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Revive.State.Downed` | State | Body component (pawn), server | The body is down: crawling and bleeding out. |
| `CK.Revive.State.BeingRevived` | State | Body component, server | A teammate is reviving the downed body. |
| `CK.Revive.State.Reviving` | State | Body component of the reviver, server | This pawn is reviving a teammate. |
| `CK.Revive.State.Dead` | State | Body component, server | The body is dead; only a respawn brings the player back. |
| `CK.Revive.State.Spectating` | State | Player component (player state), server | The player watches a teammate, also while waiting to respawn. |
| `CK.Revive.State.WaitingToRespawn` | State | Player component, server | A respawn is scheduled. |
| `CK.Revive.State.TeamWiped` | State | Match component (game state), server | No player has a standing body left. |
| `CK.Revive.Event.Downed`, `.Revived`, `.Died`, `.ReviveStarted`, `.ReviveStopped` | Gameplay Event | Body component, server, to the pawn's ASC | Instigator: who downed or killed the body, or the reviver. `.Revived` magnitude: Revived Health Fraction. |
| `CK.Revive.Event.Respawned` | Gameplay Event | Match component, server, to the new pawn's ASC | The player respawned on this pawn. |
| `CK.Revive.Block.Down` | Block | Your game (pawn tags or ASC) | The pawn does not go down: a lethal hit kills it. |
| `CK.Revive.Block.Revive` | Block | Your game (downed pawn or reviver) | No revive may start; a running one stops. |
| `CK.Revive.Block.Respawn` | Block | Your game (player state tags or ASC) | The player does not respawn while the tag is present. |

The components publish the state tags through `IGameplayTagAssetInterface`; the values come from replicated properties, so they are the same on the server and on clients. When the pawn has an Ability System Component (`IAbilitySystemInterface`), the body component also adds the body tags to it as replicated loose tags. Block tags are read from the actor's `IGameplayTagAssetInterface`, its components and its Ability System Component, so a Gameplay Effect can grant them.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKReviveMatchComponent` | Game state | Yes | Rules, checkpoints, respawns, team wipe; adds the other components (*Add Components*). `Find Match Component` finds it. |
| `UCKReviveBodyComponent` | Pawn (players and AI) | Yes | Body state, bleed-out, revive, crawl speed, ASC tags and events. `Find Body Component` finds it. |
| `UCKRevivePlayerComponent` | Player state | Yes | Life state between pawns, respawns left, spectated pawn. `Find Local Player Component` finds the local player's one. |
| `UCKReviveControllerComponent` | Player controller | Yes (RPCs only) | Relays the player's revive, give up and spectate actions to the server. `Find Local Controller Component` finds the local one. |
| `ACKReviveCheckpoint` | Actor in the level | Yes | Activation volume and spawn points; *Order* decides which checkpoint is ahead, *Start Active* marks the first one. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKRevivable` (pawn) | `CanBeDowned(DamageInstigator)` | `true`: a lethal hit puts the body down. Return `false` for falls into pits or finishing moves. |
| `ICKRevivable` (pawn) | `CanBeRevivedBy(Reviver)` | `true` when both pawns are on the same team (`IGenericTeamAgentInterface` on the pawn or its controller) or when either has no team. |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKReviveBodyComponent::OnStateChanged` | All | Body, old and new `ECKReviveBodyState` |
| `UCKReviveBodyComponent::OnDowned`, `OnRevived`, `OnDied`, `OnReviveStarted`, `OnReviveStopped` | All | Body, other actor (who downed or killed it, or the reviver) |
| `UCKRevivePlayerComponent::OnLifeStateChanged` | All | Player, old and new `ECKRevivePlayerState` |
| `UCKRevivePlayerComponent::OnRespawned` | All | Player |
| `UCKRevivePlayerComponent::OnSpectatedPawnChanged` | All | Player, spectated pawn (null when spectating ends) |
| `UCKReviveMatchComponent::OnCheckpointActivated` | All | Checkpoint |
| `UCKReviveMatchComponent::OnTeamWipedChanged` | All | Wiped |
| `CK.Revive.Event.*` Gameplay Events | Server, pawns with an ASC | See *Gameplay Tags* |

Clients fire the events from replicated state, so a state shorter than one network update may be skipped there.

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKReviveBodyComponent::Status` (state, bleed-out end, revive window, reviver, downs) | Every client (`COND_None`), `OnRep` fires the body events | Property |
| `UCKReviveBodyComponent::Tuning` (crawl multiplier, revive range) | Every client (`COND_None`): clients need no data asset | Property |
| `UCKReviveBodyComponent::RevivingTarget` | Every client (`COND_None`) | Property |
| `UCKRevivePlayerComponent::Status` (life state, state end time, respawns left, spectated pawn) | Every client (`COND_None`), `OnRep` fires the player events | Property |
| `UCKReviveMatchComponent::ActiveCheckpoint`, `bTeamWiped` | Every client (`COND_None`) | Property |
| `UCKReviveControllerComponent::ServerRequestRevive`, `ServerStopRevive`, `ServerGiveUp`, `ServerSpectate` | Owning client to server | Reliable: rare explicit player actions |
| Ability System Component loose tags | The engine's replicated loose tags | Property |

Times are server world times (`GetServerWorldTimeSeconds`), so every machine shows the same countdown.

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKReviveSettings` | Project settings: *Default Body Data*, *Default Rules*; section `[/Script/CoopKitRevive.CKReviveSettings]` in `DefaultCoopKitRevive.ini` | Runtime module `CoopKitRevive` |
| Body component | Blueprint and C++ (server): `Down`, `Kill`, `BeginRevive`, `StopRevive`, `ReviveNow`, `ResetToAlive`, `RestoreDowned` (a saved or migrated downed state), `SetBodyData`; any machine: `GetState`, `GetBleedOutSecondsLeft`, `GetReviveProgress`, `CanBeRevivedBy` | Runtime module |
| Match component | Blueprint and C++ (server): `ActivateCheckpoint`, `RespawnPlayer`, `RespawnAllPlayers`, `SpectateNext`, `SetRules` | Runtime module |
| Controller component | Blueprint and C++ (owning client or server): `FindReviveTarget`, `RequestRevive`, `StopRevive`, `GiveUp`, `SpectateNext`, `SpectatePrevious` | Runtime module |
| `ACKReviveDemoHUD`, `ACKReviveDemoPlayerController` | HUD class and player controller of the demo game mode: the panel of buttons and the mouse cursor for the local player, and the demo's Go to (a server move of the pawn) | Runtime module |
| `ck.revive.Down`, `Kill`, `Revive`, `Respawn` `<player index>` | Console commands, server | Any build |
| `ck.revive.Debug` | Console variable, 0/1: overlay | Non-Shipping builds |
| `LogCKRevive` | Log category | |

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Revive Rules** | `UCKReviveRulesData` with the class metadata `CoopKitEntity="Revive Rules"`, `CoopKitEntityFolder="ReviveRules"` | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKReviveHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitReviveEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Revive Rules`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Gameplay Abilities | Required (enabled by the plugin); an Ability System Component on pawns is optional | Stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitRevive.uplugin` and the Changelog section of the Fab listing.
