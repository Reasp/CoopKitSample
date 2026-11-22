# CoopKitHeroes

Copyright 2026 Ivan Iaitckikh.

Heroes for co-op and team games. A hero is one data asset: name and role, look and skins, Gameplay Ability System content (abilities, effects, tags) and modules for systems of your game. A component on the player state lets a player choose a hero; the server checks the choice and applies the hero to every pawn the player gets, and every client sees the hero's look, players who join a running game included.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKitHeroes** in *Edit > Plugins* and restart the editor. The plugin enables the engine's Gameplay Abilities plugin.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel lists the heroes of the roster with **Play** and a skin button, and the players (*Player 1*, *Player 2*, ...) with the hero each one plays.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitHeroes/Maps/L_HeroesDemo`, then click **Play** in the toolbar. You play the roster's default hero, the Knight.
2. Click **Play** on the *Ranger* row. The server checks the choice against the roster and dresses your pawn as the Ranger. Click **Frost skin** on the same row: the Ranger changes its colors.
3. The roster gives each hero to one player per match. To see that, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
4. In window 2, click **Play** on the row of the hero window 1 plays. The server refuses, and the panel says *Taken*. Click **Play** on a free hero instead. Both windows list the new hero.
5. Click **Look at** on the other player's row. The camera turns to that player's pawn, so you see the hero's look. Change the skin in the other window and watch the colors change.

How the demo is built:

- The game mode `BP_CKHeroesDemoGameMode` uses the player state `BP_CKHeroesDemoPlayerState`, which carries the hero component with the roster `DA_HeroRoster_Demo` (four heroes, each once per match, Knight by default), and the demo pawn `ACKHeroesDemoCharacter`.
- The game mode uses the HUD class **CKHeroes Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. **Play** and the skin buttons call *Request Hero* on your hero component, as a hero select screen would. The refusals come from *On Selection Rejected*. **Look at** only turns your camera.
- Automation tests `CoopKit.Heroes.Demo.MapsLoad`, `.ListenServer` and `.RestartPlay` load the map with no warning, then click the same buttons in one window and on a listen server with a client under 150 ms lag: a hero and its skin, a refused hero, a free hero on both machines, Look at, and Play with 2 windows.

In your project:

1. Create heroes: *Content Browser > Add > CoopKit > Hero*. Fill in Hero Id, Display Name, Mesh and, if you use the Gameplay Ability System, Abilities, Effects and Granted Tags.
2. Create a roster: *Add > CoopKit > Hero Roster*. Add the heroes players may choose, pick the uniqueness rule (a hero once per team or per match) and a default hero.
3. Add **CK Hero Component** to your player state Blueprint or class and set its Roster.
4. Give your pawn a skeletal mesh and, for abilities, an Ability System Component (on the pawn or on the player state).
5. From your hero select screen call **Request Hero** on the local player's hero component (*Find Local Hero Component*). Bind **On Selection Rejected** to show why a choice failed and **On Hero Changed** to update the screen.

## Overview map

`/CoopKitHeroes/Maps/L_HeroesOverview` shows the plugin at work on three stands, with bots and the four demo heroes (Knight, Ranger, Mage, Cleric):

- **Lineup**: two bots play the Knight and the Ranger and switch between the hero's own look and a skin; every machine shows the same colors.
- **Rules**: a bot plays a free hero. The server turns down its requests for a hero another player already plays and for a skin the hero does not have, and shows the reason. A skin the hero has goes through.
- **Respawn**: a bot gets a new pawn again and again and stays the same hero.

Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves. To choose heroes yourself, use the demo map from Quick start.

## Hero workspace (editor)

Everything about a hero is in one place; you never need to look for its parts in folders.

- **Hero card** (double-click a hero): Properties, a 3D **Preview** with a skin picker, **Parts** (mesh, animation, materials, abilities, effects, skins, icon, portrait, pawn class) with a thumbnail, an own / shared label and **Open / Replace / Create**, plus Summary and Validation.
- **New Hero wizard** (Content Browser > Add > CoopKit > Hero, the Hero Roster or the CoopKit Hub): creates `DA_Hero_<Name>` with its own ability (`GA_<Name>_Primary`), stats effect (`GE_<Name>_Stats`) and skin material (`MI_<Name>_Alt`) in the hero's folder and links them; the mesh and the Animation Blueprint stay shared.
- **Tools > Hero Roster**: every hero with search and a role filter; New, Open, Duplicate, Rename, Delete (with all own parts; shared parts are never touched), Save All, Compare, Export CSV and Import CSV.
- **Tools > Hero Comparison**: heroes side by side with the attributes their effects set, for balancing.
- **Tools > CoopKit Hub**: the entities of every installed CoopKit plugin in one window. The hub opens on categories, one per entity type. Click **Hero** to list the heroes, and click **< Back** to return. Each row shows the name players see next to the asset name.

The folder of a new hero comes from `Config/DefaultEditor.ini` of your project:

```ini
[CoopKit.Workspace]
EntityPathTemplate=/Game/CoopKit/{Folder}/{Name}
EntityPathTemplate.Hero=/Game/Characters/Heroes/{Name}
SharedPathTemplate=/Game/CoopKit/Shared/{Folder}
```

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-heroes`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for something like "add a regeneration rate parameter to heroes". In Claude Code you can also type `/coopkit-heroes`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitHeroes data in the open editor, without clicks. The editor module has `UCKHeroesAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKHeroesAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_heroes` to `<YourProject>/.claude/skills/coopkit-heroes` and to `<YourProject>/.agents/skills/coopkit-heroes`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKitHeroes talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Heroes.State.Selected` | State | Hero component (player state), via `IGameplayTagAssetInterface` | The player has a hero. |
| `CK.Heroes.Reject.UnknownHero` | Reject reason | Server | The hero is empty or not in the player's roster. |
| `CK.Heroes.Reject.UnknownSkin` | Reject reason | Server | The hero has no skin with this id. |
| `CK.Heroes.Reject.Taken` | Reject reason | Server | Another player of the team or the match plays the hero. |
| `CK.Heroes.Reject.Locked` | Reject reason | Server | The server locked the choice, or the roster forbids switching while the pawn lives. |
| `CK.Heroes.Reject.Denied` | Reject reason | Your selection gate | The game refused the choice. |
| `CK.Heroes.Event.HeroApplied` | Gameplay Event | Server, to the Ability System Component that got the hero (the pawn's, else the player state's) | The hero was applied to the pawn; the hero data is the Optional Object. |
| `CK.Heroes.Role.Tank`, `.Damage`, `.Support` | Ready values | Hero data (Role) | Use them or any tag of your own. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKHeroComponent` | Player state | Yes | Choice, roster and lock. Applies the hero to each pawn of the player. *Mesh Component Tag* picks the skeletal mesh that shows the hero (default: the character's mesh or the first skeletal mesh). |
| `ACKHeroesDemoHUD` | HUD class of the demo game mode | No | The demo's panel of buttons and mouse cursor for the local player. Demo only. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKHeroSelectionGate` (game mode or game state) | `CheckHeroSelection(Player, Hero, Skin)`: return a tag to refuse | Every choice the roster rules allow goes through. |
| `IGenericTeamAgentInterface` (engine; player state or controller) | `GetGenericTeamId` | Players without a team form one team for the Team uniqueness rule. |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `OnHeroChanged` | All | Component, hero (null when cleared), skin id |
| `OnSelectionRejected` | Owning Client (and a listen host for its own player) | Component, requested hero, skin id, reason tag |
| `OnAppearanceApplied` | All | Component, pawn, hero |
| `CK.Heroes.Event.HeroApplied` | Server | `FGameplayEventData`: Instigator = player state, Target = pawn, Optional Object = hero |
| `UCKHeroModule::ApplyToPawn` / `RemoveFromPawn` | Server | Pawn, player state |
| `UCKHeroModule::ApplyAppearance` | All | Pawn, skin id |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `Selection` (hero asset and skin id) | All (`COND_None`), `OnRep` applies the look | - |
| `Roster`, `bSelectionLocked` | All (`COND_None`) | - |
| `ServerRequestHero` | Owning client to server | Yes: an explicit player action |
| `ClientSelectionRejected` | Server to owning client | Yes: the player must learn why |
| Abilities, effects, tags | Through the Ability System Component's own replication | - |

Seamless travel: call `CopyTo` on the old player state's hero component from your player state's `CopyProperties`.

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKitHeroes data editor (Properties, Summary and Validation tabs).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKHeroData` (Hero) | Id, name, role, pawn class, mesh, animation, materials (the hero's own look, by slot), icon, portrait, skins, abilities, effects, tags, modules | Id and name required; skin ids set and unique; no empty ability, effect or module entries; module errors. A missing mesh is a warning. |
| `UCKHeroRosterData` (Hero Roster) | Heroes on offer, uniqueness (none, team, match), default hero, switching while alive | At least one hero; no empty entries; no hero or Hero Id twice; the default hero is on the list. |

Hero modules (`UCKHeroModule`, Blueprint or C++) carry the parts of a hero the plugin does not know, such as a starting inventory or revive settings. A module does not keep state: one module object serves every player of the hero.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Gameplay Abilities | Required (enabled by the plugin) | Stable |

## Console commands for developers

The demo needs no console. For development and automation the plugin has a console command:

| Command | Where | What it does |
|---|---|---|
| `ck.heroes.Select <HeroId> [SkinId]` | Any player | Asks the server for the hero (and skin) for the local player. Without arguments, writes the current hero and skin to the log. |

## Changelog

See `CHANGELOG.md` next to `CoopKitHeroes.uplugin` and the Changelog section of the Fab listing.
