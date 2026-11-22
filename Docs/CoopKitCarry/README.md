# CoopKit Carry

Copyright 2026 Ivan Iaitckikh.

CoopKit Carry lets players pick up, carry, drop and throw items in a co-op game, carry a heavy chest together, and tow a downed teammate. The server decides who carries what; the player sees the result at once.

- **No lag in the hands.** While an item is in the hands it is not physics: every machine puts it at the hands of the carrier as that machine sees the carrier, so it moves exactly like the player who holds it. In the plugin's network tests at 150 ms lag and 2% packet loss, the item stays 0.0 cm from the hands on every machine.
- **Thrown with the server's physics, seen at once.** A throw flies with the server's physics. The player who threw it sees the flight at once, and the server's flight corrects it smoothly (in our test runs, 0.1 cm or less off the server at the 95th percentile, with no hard correction; the test fails above 15 cm). Every machine sees the item come to rest where the server's does, within 2 cm.
- **Two carry, one drags.** A heavy item has two grips. One player alone drags it along the floor, slowly; a second player takes the free grip and both carry it, the item between their hands. Each player limits only their own walk (a leash to the partner), so the engine's movement prediction is never corrected. Walk too far apart for too long and the server lets go with *Stretched*.
- **Tow a downed teammate.** A pawn with a body item is towed behind the tower, at the tow point, on every machine. The plugin knows no "downed": the body's data requires your game's tag (in the demo `CK.Carry.Demo.Downed`, with CoopKit Revive `CK.Revive.State.Downed`).
- **The server decides and says why.** Every request is checked on the server: reach with room for lag, line of sight, who holds the item, free hands, a free grip, tags, the throw. A refusal is an enum and a Gameplay Tag (`CK.Carry.Reject.AlreadyHeld`, `.HandsFull`, `.NotTowable`, `.NoFreeGrip`, `.ThrowNotAllowed`, `.OutOfReach` and others), and the player's prediction goes back smoothly.
- **One item, one card.** Each kind of item is one data asset: kind, mass, speed while carried, grips, throw, leash, network settings, required and blocked tags, mesh, montages and sound. The **New Carry Item** wizard creates the item and its own Blueprint actor; the card shows every part, a summary and the problems of the data.
- **Your project's setup stays.** The plugin needs no project physics settings, no Physics Prediction and no Async Physics. A project that set up Chaos physics prediction itself keeps its replication mode.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKitCarry** in *Edit > Plugins* and restart the editor. The plugin needs no other plugin.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows what you carry and what is in front of you, tells you what to click next and shows the server's last answer.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitCarry/Maps/L_CarryDemo`, then click **Play** in the toolbar. Three crates, a lantern, a chest and an orange training dummy stand in front of you; a low wall stands beyond the crates.
2. Under *Go to*, click **Go to Crate 1**. Your character stands in front of the crate and faces it.
3. Click **Pick up**. The status line says *Server: you lift Crate 1*, and the crate rides in front of your character. The live line under it shows how far the visible crate is from your hands on this machine: *0.0 cm*.
4. Click **Walk forward** or **Turn left**: the crate moves with you. Click **Throw**: the crate flies and lands.
5. Click **Go to Lantern**, **Pick up**, then **Throw**: the status line says *Server refused Throw: ThrowNotAllowed - this item cannot be thrown*. Click **Drop** to put it down.
6. Click **Go to Training dummy**, then **Tow**: refused, *NotTowable - the body is not downed*. Click **Down the dummy**: it lies down. Click **Tow**, then **Walk forward**: the dummy follows right behind you. Click **Drop** to let it go, **Restore** to get it up.
7. Click **Go to Chest** and **Pick up**: the chest is too heavy for one, so you drag it, slowly.
8. Carrying it together needs two players: click **Play with 2 windows**. Play restarts as a listen server with two windows; your Play settings stay as they were. In window 1, click **Go to Chest** and **Pick up**. In window 2, click **Go to Chest** (you stand at its free grip) and **Join**. Both windows say the chest is carried by Player 1 and Player 2. Click **Walk forward** in one window and **Walk back** in the other: you carry it together. **Drop** lets go of your grip; the other player drags it on.
9. In window 2, click **Down me**. In window 1, click **Go to Player 2**, then **Tow**: one player tows the other. **Restore** gets everyone up and ends the tow.

The demo items in `/CoopKitCarry/CarryItems`:

| Item | Kind | Mass | Speed while carried | Notes |
|---|---|---|---|---|
| `DA_CarryItem_Crate` | Hold | 15 kg | 85% | Thrown at 8 m/s, 35 degrees up. Actor `BP_Carry_Crate`, a 50 cm cube. |
| `DA_CarryItem_Lantern` | Hold | 3 kg | 85% | Cannot be thrown. Actor `BP_Carry_Lantern`. |
| `DA_CarryItem_Chest` | Heavy | 60 kg | 60% together, 35% dragged alone | Grips at (60, 0, 0) and (-60, 0, 0); let go at 3 m apart for 1 s. Actor `BP_Carry_Chest`. |
| `DA_CarryItem_Body` | Body | 80 kg | 50% while towing | Requires `CK.Carry.Demo.Downed`; tow point 130 cm behind the tower. No actor: the dummy and the players are the bodies. |

How the demo is built:

- The game mode `BP_CKCarryDemoGameMode` (parent **CKCarry Demo Game Mode**) uses the demo character **CKCarry Demo Character** (a CK Carrier component, a CK Carryable body with *Player Body Data*, a body of engine shapes and a camera from behind), the player controller **CKCarry Demo Player Controller** (*Go to*, *Walk*, *Turn*, *Down*, *Restore*) and the HUD class **CKCarry Demo HUD**, which draws the panel.
- The dummy is the plugin's **CKCarry Training Dummy**: a character with a body carryable that publishes `CK.Carry.Demo.Downed` while it lies.
- The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: *Find Best Item* of the carry subsystem for the item in front of you, *Request Pick Up*, *Request Join*, *Request Tow*, *Request Drop* and *Request Throw* on your carrier component; the answers, refusals included, come from *On Carry Result*.

In your project:

1. **Make items.** *New Carry Item* in the CoopKit Hub (or *Add > CoopKit > Carry Item*) asks for a name, the kind and a mesh, and creates the item data and its Blueprint `BP_Carry_<Name>`: place the Blueprint in your levels. To make a prop of your own carryable instead, add a **CK Carryable** component to it and set its *Item Data*; the actor must replicate and its root must be the physics body.
2. **Let players carry.** Add a **CK Carrier** component to your player's pawn (or an AI pawn). Items sit at the item's *Hold Offset* from the pawn's root, the capsule's center of a Character. If your pawn's root sits lower (a Default Pawn's sphere), raise the component's *Hold Origin* by the difference.
3. **Ask from your input or UI.** On the local player's carrier component call **Request Pick Up** with the item (the carry subsystem's **Find Best Item** gives the one in front), **Request Drop** and **Request Throw**. *Request Pick Up* lifts a Hold item, drags a Heavy item alone, takes the free grip of a Heavy item another player drags and tows a Body; **Request Join** and **Request Tow** ask for exactly that. The walk slows by itself while the pawn carries.
4. **Show the answers.** Bind **On Carry Result** for the reason of a refusal; bind the item's **On Phase Changed** for sounds and animations of a pickup, a throw, a landing.
5. **Tow downed players** (optional). Add a **CK Carryable** component to your player's pawn with an item of the kind *Body*, and put your game's downed tag into its *Required Tags*. Publish that tag with `IGameplayTagAssetInterface` on the pawn or on one of its components. When your game gets the player up, call **Server Drop All** on the body's carryable component.
6. **Stop carrying in a state** (optional). Put your game's tags into the item's *Blocked Tags*: the server refuses while the item or the carrier has one of them.

## Overview map

`/CoopKitCarry/Maps/L_CarryOverview` shows the plugin on four stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves, in a loop, and the last line of each sign says what the server answered:

1. **Lift, carry, throw**: Ada lifts the crate, carries it in her hands (the sign shows the gap to the hands: 0.0 cm) and throws it; the server's physics flies it.
2. **Too heavy for one**: Ben drags the chest alone; Cid takes the other grip and they carry it together; then Ben lets go and Cid drags it on.
3. **Tow a downed body**: Dan is refused while the dummy stands (*Not Towable*); the dummy is downed and Dan tows it; the sign shows the gap to the tow point: 0.0 cm.
4. **Refusals with a reason**: Eve and Fin are refused with *Throw Not Allowed*, *Hands Full*, *Already Held*, *No Free Grip*, *Not Towable* and *Out Of Reach*.

The bots are local players without a view of their own, so the plugin treats them as real players. The scenario (`ACKCarryShowcaseTrial`, one class with a mode per stand) spawns the stand's items from their data, calls the requests of each bot's carrier component, the same server path a player's click takes, and walks the bots on the server; the items in their hands follow on every machine. On a dedicated server there are no bots, and the scenarios do nothing.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Carry Item*) or with **New Carry Item** in the CoopKit Hub. A double click opens the card: *Properties*, *Summary* (the whole item in plain words), *Validation* (problems, refreshed on every change), *Parts* (mesh, montages, sound, the item's Blueprint) and *Preview*.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKCarryItemData` (*Carry Item*) | *Display Name*; *Kind* (Hold, Heavy, Body), *Mass Kg*, *Can Drag Alone*; speeds *Hold*, *Co Carry*, *Drag*, *Tow Speed Multiplier*; grips *Hold Socket*, *Hold Offset* (for a Body, the tow point), *Hold Rotation*, *First Grip*, *Second Grip*; throw *Can Throw*, *Throw Speed*, *Throw Angle Degrees*, *Drop Speed*; *Pick Up Range*; together *Leash Slack Cm*, *Break Distance Cm*, *Break Seconds*; network *Predict Throw*, *Visual Blend Seconds*, *Hard Snap Distance Cm*, *Flight Replication*; *Blocked Tags*, *Required Tags*; parts *Mesh*, *Pick Up Montage*, *Throw Montage*, *Pick Up Sound*, *Actor Class* (the item's Blueprint). | Name required; mass above 0; every speed multiplier above 0 and at most 1; a Heavy item's grips at least 10 cm apart; a Body is never thrown and has no actor; a throwable item has a speed; the break distance is farther than the grips plus the leash slack; no negative range, slack, time or speed. |

## How a carry plays

- **Ask.** On the player's machine a request shows at once: the item jumps to the hands (with a short blend), a thrown item flies. A reliable RPC carries the request, the item and, for a throw, the release velocity to the server.
- **Check.** The server checks, in order: requests per second, the item and its data, who holds the item (free, or a free grip), reach (*Pick Up Range* plus 15% for lag), line of sight from the eyes, *Blocked Tags*, the kind (only a Body is towed), *Required Tags* of a body, free hands, a partner for a Heavy item that cannot be dragged alone, and for a throw the data and the speed.
- **Carry.** The server replicates only who carries the item and how (`FCKCarryState`: phase, carriers, a sequence number). Every machine puts the item at the hands itself; physics and movement replication of the item are off while it is in the hands. Two carriers stop blocking each other's movement; each limits only its own walk.
- **Throw.** The server releases the item with its physics and the requested velocity, limited by the data. The thrower's machine flies its own prediction and compares it with the server's flight at the same moment of the flight; other machines see the server's flight (*Predictive Interpolation* by default). When the item comes to rest, the server takes it back.
- **Tow.** A towed body is attached at the tow point behind the tower on every machine; its own movement stops, and the server does not correct it. When the tow ends, the server trusts the body's position for a moment, so no correction follows.
- **Answer.** **On Carry Result** fires on the asking machine with the result and its reason tag; a refusal takes the prediction back. **On Phase Changed** fires on every machine.

## Automation tests

Editor automation tests (Session Frontend, filter `CoopKit.Carry`):

- `CoopKit.Carry.Spike.*`: a server and two clients in one editor process at 150 ms lag and 2% packet loss, listen and dedicated, measured every frame on every machine. Hold and walk a 6 m circle; throw at 8 m/s and 35 degrees to rest, also with a 0.15 s stall on every 7th frame during the throw; two carry a chest 10 m with a 90 degree turn; tow a body 8 m; two players grab the same crate at once (*Already Held*); two carriers walk apart (*Stretched*). The numbers: the item 0.0 cm from the hands on every machine; no more jumps of the item than of its carrier; no more movement corrections than the same walk without a load; the thrower's flight at most 15 cm off the server at the 95th percentile (0.03 to 0.10 cm in our runs), no hard correction; other machines see the server's flight a network delay behind it (5.6 to 10.6 cm off at the 95th percentile in our runs); at rest within 2 cm on every machine.
- `CoopKit.Carry.Demo.Panel`, `.Together`, `.RestartPlay`: every step is a click on a panel button. One window picks up a crate and throws it, is refused a tow of the standing dummy, downs it and tows it; a listen server and a client with 150 ms lag carry the chest together and let go; the Play with 2 windows button restarts Play with two windows.
- `CoopKit.Carry.Overview`: a listen server and a client on the overview map; all four stands play and every refusal on the signs comes up.
- `CoopKit.Carry.Rules.*`, `CoopKit.Carry.Metrics.*`: the server's checks as pure functions in the order they run, the hands, the leash and the smoothness metrics on synthetic tracks.
- `CoopKit.Carry.Data.Validation`, `.ItemEditor`, `CoopKit.Carry.Agent.*`, `CoopKit.Carry.Hub.*`, `.Workspace.*`, `.Skills.*`, `.Tags.*`: every data problem names its field; the wizard, the card and its tabs; the agent operations; the CoopKit Hub; the AI agent skill.

## Limitations and known issues

- No telekinesis: an item in the hands is not rotated or brought closer. No carrying a body on the shoulder (a body is towed), no three or more carriers, no storing items in an inventory (an inventory of your game can take the item from your code).
- No animation set ships with the plugin. *Pick Up Montage* and *Throw Montage* of the item are yours to play from *On Phase Changed*.
- Other players see the item in someone's hands where they see that player: a network delay behind the server, like the player.
- Other players see a thrown item fly along the server's flight, a network delay behind it. In our test runs their view was 5.6 to 10.6 cm off the server's at the 95th percentile; the thrower's own view, 0.1 cm or less.
- The thrower's prediction knows only what its machine simulates: a hit on something that moves only on the server is corrected after the hit.
- A towed body is placed by every machine behind the tower as it sees the tower; while the tower's machine freezes, the body stands still.
- Items are referenced over the network. Use saved assets, not objects made at runtime: a transient item data does not replicate.
- Dedicated servers are tested in the editor on a dedicated server world.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-carry`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change, for example "let a downed player be towed" or "make a barrel two players carry". In Claude Code you can also type `/coopkit-carry`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or let Live Coding in the editor compile the change.

The skill can also edit CoopKitCarry data in the open editor, without clicks. The editor module has `UCKCarryAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKCarryAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. `UCKCarryAgentOps` (`unreal.CKCarryAgentOps`) adds what only a carry item has: create an item with its Blueprint, set its grips, describe it, and make an actor of the open level carryable. You can ask your agent, for example: "make a coffin two players carry and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_carry` to `<YourProject>/.claude/skills/coopkit-carry` and to `<YourProject>/.agents/skills/coopkit-carry`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Carry talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Carry.Reject.InvalidItem` | Reason | Server | No item, no item data, or the item is being destroyed. |
| `CK.Carry.Reject.OutOfReach` | Reason | Server | The item is farther than *Pick Up Range* plus 15%. |
| `CK.Carry.Reject.NoLineOfSight` | Reason | Server | A wall stands between the carrier's eyes and the item. |
| `CK.Carry.Reject.Blocked` | Reason | Server | The item or the carrier has one of the item's *Blocked Tags*. |
| `CK.Carry.Reject.NotTowable` | Reason | Server | Tow on an item that is not a Body, or a body without its *Required Tags*; also when a towed body gets up. |
| `CK.Carry.Reject.AlreadyHeld` | Reason | Server | Somebody else holds the item. |
| `CK.Carry.Reject.HandsFull` | Reason | Server | The carrier already holds another item. |
| `CK.Carry.Reject.NeedsPartner` | Reason | Server | A Heavy item that cannot be dragged alone. |
| `CK.Carry.Reject.NoFreeGrip` | Reason | Server | Join on an item without a free grip. |
| `CK.Carry.Reject.ThrowNotAllowed` | Reason | Server | The item cannot be thrown, or is not held by one carrier. |
| `CK.Carry.Reject.NotHolding` | Reason | Server | Drop or throw without an item in the hands. |
| `CK.Carry.Reject.Stretched` | Reason | Server | The carriers (or the tower and the body) got too far apart for too long; the server let go. |
| `CK.Carry.Reject.RateLimited` | Reason | Server | More requests per second than the carrier component allows. |
| `CK.Carry.Reject.NotAuthority` | Reason | Server API | A server function was called on a client. |
| `CK.Carry.State.Held`, `.Towed`, `.Thrown` | State | Carryable component (all machines) | The item is in the hands, towed, or flying. |
| `CK.Carry.State.Carrying` | State | Carrier component (all machines) | The pawn carries, drags or tows something. |
| `CK.Carry.Demo.Downed` | State | Demo character and training dummy | Demo only: the body may be towed. |

Tags read from other plugins (no dependency): any tag in an item's *Blocked Tags* or *Required Tags*, for example `CK.Revive.State.Downed` of CoopKit Revive, is read from the actor and its components (`IGameplayTagAssetInterface`).

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKCarryableComponent` (*CK Carryable*) | Any replicated actor whose root is the physics body; a pawn for a Body | Yes | *Item Data*, the carry state, *On Phase Changed*; *Server Drop All*, *Server Let Go*. **Find Carryable** finds it. |
| `UCKCarrierComponent` (*CK Carrier*) | The player's pawn, or an AI pawn | RPC only | *Request Pick Up*, *Request Join*, *Request Tow*, *Request Drop*, *Request Throw*, *Get Held Item*, *On Carry Result*; *Execute On Server* for server code; *Hold Origin*, where the items' Hold Offset starts. **Find Carrier** finds it. |
| `ACKCarryActor` | A Blueprint per item (the wizard makes it) | Yes, movement to 0.01 cm | A hidden physics body, the visible mesh, the carryable component. |
| `UCKCarrySubsystem` | World subsystem | No | Every item of the world; **Find Best Item** is the item in front of a pawn. |
| `ACKCarryTrainingDummy`, `ACKCarryDemoCharacter`, `ACKCarryDemoPlayerController`, `ACKCarryDemoHUD`, `ACKCarryDemoGameMode` | The demo map | Dummy and character: the downed state; controller: RPC only | Demo only. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IGameplayTagAssetInterface` (engine) on the item, the carrier, a body or their components | `GetOwnedGameplayTags` | No tags: *Blocked Tags* never block, a body with *Required Tags* is never towed. |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKCarrierComponent::OnCarryResult` | The asking machine: the owning client, or the server for its own player and AI | Result, action, item, reason tag, request id, state sequence |
| `UCKCarryableComponent::OnPhaseChanged` | All | Item, old phase, new phase |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKCarryableComponent::State`, `ItemData` | Every client (`COND_None`) | Property |
| `UCKCarryableComponent::FlightSample` | Owner only (`COND_OwnerOnly`): the thrower, during the flight | Property |
| `UCKCarrierComponent::ServerRequest` | Owning client to server | Reliable: an explicit player action |
| `UCKCarrierComponent::ClientCarryResult` | Server to owning client | Reliable: the prediction is confirmed or taken back |
| `ACKCarryDemoPlayerController::ServerDemoGoTo`, `ServerDownDummy`, `ServerDownSelf`, `ServerRestore` | Owning client to server | Reliable: demo clicks |

### CoopKit Hub

Data types of CoopKitCarry with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click one to list its entities, and click **< Back** to return.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

The Parts tab lists the assets an entity refers to through properties marked with `CoopKitPart` metadata. An own part lies in the entity's folder (the item's Blueprint `BP_Carry_<Name>`). It is not referenced from outside the kit and follows the entity on rename, duplicate and delete; a duplicated item gets its own copy of the Blueprint, which carries the copy. A shared part (the mesh, montages, sounds) is never touched.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

None: the plugin uses only engine modules (Engine, Physics Core, Gameplay Tags, UMG for the demo panel).

## Changelog

See `CHANGELOG.md` next to `CoopKitCarry.uplugin` and the Changelog section of the Fab listing.
