# CoopKit Traversal

Copyright 2026 Ivan Iaitckikh.

CoopKit Traversal adds parkour for co-op games: vaults, mantles and climbs that work over the network, and two-player moves where one player helps another up.

- **Moves alone.** A vault over a crate or a fence, a low mantle onto a table, a high mantle onto a wall top, a climb over a high thin wall. The character asks once; the plugin finds the obstacle, measures its height and depth and picks the move.
- **Moves together.** A friend braces at a wall and boosts you higher than you can mantle alone. A friend kneels at the edge of a ledge and pulls you up. A friend cups their hands on open ground and launches you forward. The helper offers, you ask, and the server gives the offer to the first friend whose move arrives.
- **Predicted on the player's machine.** The move starts at once, inside the character's movement, the same way walking is predicted. The server checks the start and runs the same path from the same moment, so server corrections are not expected. In the plugin's network test at 150 ms lag and 2 % packet loss, 400 solo moves (50 of each of the four kinds, on a dedicated and on a listen server) needed no server correction in a run on UE 5.8. The test passes with no more corrections than plain walking gets, so another run can differ.
- **Everyone sees the same move.** Other players replay the move along the same path from its replicated start, not from rare position updates.
- **The server decides and says why.** Every refusal is an enum value and a Gameplay Tag: `CK.Traversal.Reject.TooHigh`, `.TooLow`, `.NoRoom`, `.Blocked`, `.OfferTaken`, `.NotAnAlly` and others.
- **One profile, one card.** A traversal profile is one data asset with the heights, timings and exit speeds of all seven moves, the animation, the icon and two input actions. The **New Traversal Profile** wizard creates it from a preset (Standard, Agile, Heavy). The card draws the path of every move for an obstacle height you pick with a slider.
- **Other plugins can stop moves.** Add a tag such as a downed state to *Blocked Tags*: a character with that tag makes no move and offers no help. Mark an actor with `CK.Traversal.NoTraverse` and nobody traverses it.

The plugin is a character movement component. Derive your character from **CKTraversal Character**, or give your C++ character the plugin's movement component, and call *Request Traverse* from your input.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Traversal** in *Edit > Plugins* and restart the editor. The plugin uses the engine plugin **Enhanced Input**; the editor enables it with the plugin.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows what your character does, the last answer to your request and the server corrections, and tells you what to do next.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitTraversal/Maps/L_TraversalDemo`, then click **Play** in the toolbar. A course of seven lanes stands in front of you, one obstacle per lane, with its name and height above it.
2. Click **Vault (90 cm)**. Your character runs to a crate and vaults it without stopping. The state line shows *Moving: Vault* with its progress.
3. Click **Mantle low (110 cm)**, **Mantle high (180 cm)** and **Climb over (160 cm)**. Each button puts your character in front of its obstacle and runs it. The plugin picks the move from the obstacle: up onto a deep block, over a thin wall.
4. Click **Too high alone (260 cm)**. Your character runs at a 260 cm wall and stops. The panel shows the answer: *Too High (CK.Traversal.Reject.TooHigh)*.
5. Click **Boost a friend (260 cm)**. A helper comes to the same wall and braces at it. Your character runs at the wall and is boosted up to its top.
6. Click **Pull up a friend (340 cm)**. A helper kneels at the edge of a 340 cm ledge, and your character is pulled up.
7. Click **Assisted jump**. A helper cups their hands on open floor, and your character is launched 4.5 m forward as it runs past.
8. To watch the moves from another player's window, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were. Run a station in one window and watch it in the other.
9. With two windows, click **Lag 150 ms** or **Lag 300 ms** and run the stations again. The *Server corrections* line counts the corrections your character received and, in brackets, those around moves. The plugin's test saw none around moves at 150 ms lag with 2 % packet loss. The tests do not cover 300 ms.

How the demo is built:

- The game mode **CKTraversal Demo Game Mode** builds the course from code on the server. It gives every player a **CKTraversal Demo Character** (the plugin's character with a body, a camera and an autopilot) and uses the HUD class **CKTraversal Demo HUD**, which draws the panel.
- A station button asks the server to put your character at the station's start and, for a two-player station, to bring a helper bot that opens its offer. Your character's autopilot then gives the same movement input a player gives and calls *Request Traverse* at the obstacle, so the move goes through your own prediction and the server's check.
- The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: *Request Traverse* and *Open Offer* (through the autopilot), *Get Traversal Kind*, *Get Traversal Progress*, *Get Net Stats* and the event *On Traversal Result*.

In your project:

1. **Give your character the movement.** In C++, derive your character from **ACKTraversalCharacter**, or replace the movement component of your own character in its constructor: `Super(ObjectInitializer.SetDefaultSubobjectClass<UCKTraversalMovementComponent>(ACharacter::CharacterMovementComponentName))`. In a Blueprint-only project, create a Blueprint class with **CKTraversal Character** as its parent and use it as your pawn. The component keeps every setting of the engine's Character Movement.
2. **Make a profile.** **New Traversal Profile** in the CoopKit Hub (or *Add > CoopKit > Traversal Profile*) asks for a preset, a name, an animation and an icon, and creates the profile with its own input actions. Set it as the component's *Profile*. Or take a ready profile (Standard, Agile or Heavy) from `/CoopKitTraversal/Data`; see [Data assets](#data-assets). Without a profile the component uses its *Default Settings*. Every player must use the same profile.
3. **Ask for moves.** Bind your traverse input to **Request Traverse** on the character's movement component. Or call **Bind Profile Inputs** with your Enhanced Input component: the profile's *Input* asks for a move, and its *Assist Input* opens or closes an offer (**Toggle Offer**). Add both actions to your input mapping context. The jump input is a good place for *Request Traverse*: when it answers anything but *Started*, jump.
4. **Help a friend.** Call **Open Offer** with *Brace* (facing a wall), *Kneel* (at the edge of a ledge, facing the drop) or *Cup* (on open floor). Or call **Toggle Offer**, which picks the kind the spot allows. The helper stands still while the offer is open; moving away closes it. A friend's *Request Traverse* within 2 m of the offer gets the two-player move.
5. **Show the answers.** Bind **On Traversal Result** for refusals, **On Traversal Started** and **On Traversal Finished** for sounds and the camera, and **On Offer Changed** and **On Offer Taken** for the helper's prompt.
6. **Animate.** Set the profile's *Animation* (an Anim Montage). The plugin plays it at the start of every move, stretched to the move's duration, on every machine. Without it the capsule still moves, and your Animation Blueprint can read **Is Traversing** and **Get Traversal Kind**.
7. **Block moves** (optional). Put tags into the profile's *Blocked Tags*, or call **Set Blocked Tags** on the component on every machine. The component reads the tags of the character and of its player state, so another plugin's state (for example CoopKit Revive's `CK.Revive.State.Downed`) stops the moves with no code. Mark actors nobody may climb with `CK.Traversal.NoTraverse` (through `IGameplayTagAssetInterface`).
8. **Teams** (optional). Turn on *Require Same Team* in the profile: only a helper of the receiver's team (`IGenericTeamAgentInterface` on the pawn or its controller) may help.

## Overview map

`/CoopKitTraversal/Maps/L_TraversalOverview` shows the plugin on three stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel. The stands play by themselves, in a loop, and the last line of each sign says what the server answered:

1. **Over it alone**: Ada runs at a 90 cm crate, a 110 cm and a 180 cm ledge and a thin 160 cm wall: a vault, a low mantle, a high mantle, a climb over.
2. **Up together**: Ben braces at a 230 cm wall, kneels on a 230 cm ledge and cups his hands; Cleo is boosted, pulled up and launched.
3. **Refusals with a reason**: Dan is refused with `TooHigh` (a 230 cm wall alone), `Blocked` (a crate marked `CK.Traversal.NoTraverse`) and `TooLow` (a 40 cm step, which he walks up).

The bots are characters with the plugin's movement and an autopilot, driven by the server (`ACKTraversalShowcaseTrial`, one class with a mode per stand). Their moves take the same request, prediction and server check as a player's, so the stands also play on a dedicated server.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Traversal Profile*) or with **New Traversal Profile** in the CoopKit Hub. A double click opens the card:

- *Properties*;
- *Summary*: the moves with their heights and durations in plain words, the offers, the server's tolerances and the blocking tags;
- *Validation*: the problems of the data, refreshed on every change;
- *Preview*: the path of each move over an obstacle of the height you pick, alone and with each kind of help;
- *Parts*: icon, animation, input actions.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKTraversalProfileData` | For each of the seven moves: on or off, lowest and highest obstacle, duration, exit speed. Also: thin and deep obstacles, reach, clearance, landing, the server's tolerances and request rate, the offer radius and grace time, the launch's distance and height, *Require Same Team*, *Blocked Tags*, animation, icon, *Input* and *Assist Input*. | A display name; for every move, a duration above 0 and the lowest height not above the highest; vault and climb over, and the two mantles, without overlapping heights; the boost and the pull-up reaching higher than the high mantle; *Path Tolerance* not above *Start Tolerance*; reach, thin depth and offer radius above 0. |

The presets of the wizard:

| Preset | Moves |
|---|---|
| Standard | Vault 50-110 cm in 0.6 s, low mantle 50-130 cm in 0.7 s, high mantle 130-200 cm in 1.0 s, climb over 110-200 cm in 1.0 s, boost up to 320 cm, pull-up up to 360 cm, launch 4.5 m. |
| Agile | Heights 10 % higher, durations 20 % shorter, launch distance 10 % longer than Standard. |
| Heavy | Heights 15 % lower, durations 25 % longer, launch distance 15 % shorter than Standard. |

The three presets are also ready profiles in `/CoopKitTraversal/Data`: `DA_TraversalProfile_Standard`, `DA_TraversalProfile_Agile` and `DA_TraversalProfile_Heavy`. Each uses the input actions `IA_CKTraversal_Traverse` (*Input*) and `IA_CKTraversal_Assist` (*Assist Input*) in `/CoopKitTraversal/Input`, and the input mapping context `IMC_CKTraversal` there binds them to Space and E. Set a profile on your character's movement component, call **Bind Profile Inputs** and add `IMC_CKTraversal` to the player's Enhanced Input subsystem, and your character traverses with Space and helps with E. Duplicate a profile to change it.

## Integration

How CoopKit Traversal talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Traversal.State.Traversing` | State | The movement component | The character is in a move. |
| `CK.Traversal.State.Assisting` | State | The movement component, from the replicated offer | The character offers help. |
| `CK.Traversal.Offer.Brace`, `.Kneel`, `.Cup` | State | The movement component | The kind of the open offer. |
| `CK.Traversal.NoTraverse` | Marker | Your actors (`IGameplayTagAssetInterface`) | Nobody traverses this actor (refusal `Blocked`). |
| `CK.Traversal.Reject.NoObstacle`, `.TooLow`, `.TooHigh`, `.NoRoom`, `.Blocked`, `.OnCooldown`, `.TooFar`, `.RateLimited`, `.InvalidRequest`, `.NoOffer`, `.OfferTaken`, `.NotAnAlly` | Refusal | The plugin, for each `ECKTraversalResult` | Why a move or an offer was refused. |

The component publishes its state tags through `IGameplayTagAssetInterface`. Blocked tags are read from the character, its components and its player state.

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKTraversalMovementComponent` | Your character, as its movement component | Yes, with the character's movement | Replaces the engine's Character Movement and keeps all its settings. `ACKTraversalCharacter` has it already. |
| `UCKTraversalAutopilotComponent` | A character with the movement above (optional) | No: it runs on the machine that predicts the character | Walks to a point and asks for moves on the way, or walks to a spot and offers help. For AI, scripted scenes and tests; the demo and the overview use it. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IGameplayTagAssetInterface` (engine) | `GetOwnedGameplayTags` with `CK.Traversal.NoTraverse`, on an obstacle | The actor can be traversed. |
| `IGameplayTagAssetInterface` (engine) | `GetOwnedGameplayTags` with a tag of *Blocked Tags*, on the character, its components or its player state | Moves are allowed. |
| `IGenericTeamAgentInterface` (engine) | `GetGenericTeamId` on the pawn or its controller | With *Require Same Team*: no team, so nobody helps. Without it: anyone helps. |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `OnTraversalStarted` | All: the player's machine, the server, and the others as they replay the move | The kind of move |
| `OnTraversalFinished` | All | The kind of move |
| `OnTraversalResult` | The machine that asked; the server's refusal reaches the owning client | The refusal (`ECKTraversalResult`) |
| `OnOfferChanged` | All | The offer: kind, anchor, open or closed |
| `OnOfferTaken` | All | The friend who took the offer |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| The start of a move and its parameters | In the client's movement updates, repeated until the server acknowledges it | Repeated until acknowledged |
| The traversal state in a correction | In the server's movement response | As the character's movement |
| `Offer` | All | Property |
| `ProxyState` (the move others replay) | Simulated proxies only | Property |
| `ClientTraversalRefused` | Owning client | Yes |
| `ServerOpenOffer`, `ServerCloseOffer` | Server | Yes |

### Blueprint API

| Node | Where | What it does |
|---|---|---|
| **Request Traverse** | The player's machine | Finds the obstacle ahead, or a friend's offer nearby, and starts the move. Answers *Started* or why not. |
| **Open Offer**, **Close Offer**, **Toggle Offer** | The helper's machine | Opens or closes an offer of help. |
| **Bind Profile Inputs** | The player's machine | Binds the profile's *Input* and *Assist Input* actions. |
| **Is Traversing**, **Get Traversal Kind**, **Get Traversal Progress** | Any | The move under way. |
| **Is Offering**, **Get Offer** | Any | The character's offer. |
| **Get Profile**, **Set Profile**, **Set Blocked Tags**, **Get Blocked Tags** | Any, before play | Settings of the component. |
| **Get Net Stats**, **Reset Net Stats** | Any | Moves, refusals and server corrections counted on this machine. |

### CoopKit Hub

The traversal profile appears in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click **Traversal Profile** to list the profiles, and click **< Back** to return.

From the hub you can:

- open a profile's card;
- create a profile with the **New Traversal Profile** wizard;
- check the project: `IsDataValid` of every entity and duplicate ids.

The *Parts* tab lists the assets a profile refers to. An own part, such as an input action the wizard made, lies in the profile's folder. It is not referenced from outside the kit and follows the profile on rename, duplicate and delete. A shared part, such as an animation or an icon, is never touched.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Enhanced Input | Required | Stable |

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-traversal`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to the plugin's data, for example "add a slide under low obstacles". In Claude Code you can also type `/coopkit-traversal`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or let Live Coding in the editor compile the change.

The skill can also edit traversal profiles in the open editor, without clicks. The editor module has two function libraries for Python and Editor Utility Blueprints. Both answer in JSON:

- `UCKTraversalAgentLibrary` (`unreal.CKTraversalAgentLibrary`) lists, creates, describes, validates, exports and imports the profiles and runs the project check.
- `UCKTraversalAgentOps` (`unreal.CKTraversalAgentOps`) creates a profile from a preset, applies a preset, changes one move's settings and samples the path of a move. It also tells which moves an obstacle height gets alone and with help, and reads an actor of the open level as an obstacle.

You can ask your agent, for example: "make an Agile profile, let the vault go up to 120 cm and check which moves a 150 cm wall gets". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_traversal` to `<YourProject>/.claude/skills/coopkit-traversal` and to `<YourProject>/.agents/skills/coopkit-traversal`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Changelog

See `CHANGELOG.md` next to `CoopKitTraversal.uplugin` and the Changelog section of the Fab listing.
