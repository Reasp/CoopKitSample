# CoopKit Loot

Copyright 2026 Ivan Iaitckikh.

CoopKit Loot shares loot between the players of a co-op game, and the server decides who gets what:

- **Personal loot.** Each player rolls the table alone and sees only their own drops. The server never sends another player's personal drop to your machine, so no client can see or take it.
- **Free for all, round robin, Need or Greed.** One roll for the group. In free for all, the first player to pick an item up gets it; if two players grab it at the same moment, one gets the item and the other is refused. In round robin, each item goes to the next player in turn. In Need or Greed, each item opens a vote with a time limit: Need beats Greed, and the server draws the numbers.
- **Loot tables as data assets.** Items with weights, rarity, counts and guaranteed items; more rolls for more players (a number per player or a curve); a guaranteed rarity in every drop. Each table has a card in the CoopKit Hub, a "New Loot Table" wizard and a Summary that shows the chances.
- **The server rolls.** Clients never roll: the chance comes from the server's random stream. You can set its seed for tests and replays.
- **Your inventory, your UI.** Implement one interface on your inventory and the plugin hands it every item a player picks up. The plugin ships no widget: you get the drops, the rolls and the server's answers as data and events.

The plugin changes none of your base classes. Call *Drop Loot* on the server, and the plugin adds what the players need.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Loot** in *Edit > Plugins* and restart the editor. No other plugin is required.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel lists the drops you see, whose they are, and what the server answered.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitLoot/Maps/L_LootDemo`, then click **Play** in the toolbar.
2. Click **Open the chest: Personal loot**. The panel lists your own drops. Click a row: the server gives you the item, and the panel says *The server gave you ...*.
3. Click **Open the chest: Need or Greed**. A roll appears with **Need**, **Greed** and **Pass** and the seconds left. Click **Need**. You are the only player, so you win, and the row says the item is yours. Click it to take it.
4. Click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
5. In window 2, click **Open the chest: Free for all**. Both windows list the same free items. Click a row in window 2: that player gets the item, and the row goes away in both windows. A player who clicks an item someone already took sees *The server refused: Taken*.
6. Click **Open the chest: Personal loot** in either window. Each window lists only its own drops: the other player's personal loot is not on your machine at all.
7. Click **Open the chest: Round robin**. The rows say *yours* or *for Player 2*. Click a row of the other player: the server refuses with *NotYours*.

The four demo tables are in `/CoopKitLoot/Data`:

| Asset | What it is |
|---|---|
| `DA_LootTable_DemoPersonal` | Personal loot, 2 rolls for each player, an Uncommon or rarer item in every drop. |
| `DA_LootTable_DemoFreeForAll` | Free for all, 2 rolls plus 1 per extra player. |
| `DA_LootTable_DemoRoundRobin` | Round robin, 2 rolls plus 1 per extra player. |
| `DA_LootTable_DemoNeedGreed` | Need or Greed, 1 roll, 20 s to choose, an item of Rare or rarer in every drop. |

How the demo is built:

- The game mode `BP_CKLootDemoGameMode` uses the player controller `BP_CKLootDemoPlayerController` and the engine's `DefaultPawn`. The demo tables have a pickup radius of 30 m, so the demo needs no walking.
- The game mode uses the HUD class **CKLoot Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: a drop row calls *Pick Up* on your loot player component, **Need**, **Greed** and **Pass** call *Roll*, and the answers come from *On Loot Received* and *On Rejected*.
- **Open the chest** stands in for your game's rule. The demo controller asks the server to open it, and the server calls *Drop Loot* at the chest.

In your project:

1. **Make a loot table.** In the CoopKit Hub, click **New Loot Table**, or in the Content Browser use *Add > CoopKit > Loot Table Data*. Pick the sharing mode, then add your items to *Entries*: an *Item Id*, a weight, the rarity and the count. Set *Item Asset* to your inventory's item asset if you have one.
2. **Drop loot on the server.** When a chest opens or an enemy dies, call **Drop Loot** (*Get World Subsystem > CKLoot Subsystem*) with the table and the location. The server rolls and lays the drops out around the location.
3. **Take the items into your inventory.** Add a component that implements **CK Loot Receiver** to your player state (or pawn, or player controller). *Receive Loot* gets the player and the item and returns how many units it took. Return 0 when the bag is full: the player gets *Full* and the item stays on the floor.
4. **Let players pick up.** On a click or a key, call **Pick Up** on the player's loot component (**Find Local Loot Player Component**) with the drop. For a roll, call **Roll** with *Need*, *Greed* or *Pass*.
5. **Show the loot.** In your widget, list **Get Visible Drops** of the loot subsystem for the local player. Each drop gives its item, phase (free, assigned, rolling), owner and roll with the seconds left. Show the answers from **On Loot Received** and **On Rejected**.

## Overview map

`/CoopKitLoot/Maps/L_LootOverview` shows the plugin on four stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves. On each stand a chest opens for two bot players in a loop, and the last lines of the sign say what dropped and what the server answered:

1. **Personal loot**: each bot rolls alone; the sign lists what each one sees. One bot reaches for the other's drop and is refused. A client gets neither drop.
2. **Free for all**: both bots grab the same item at the same moment; one gets it, the other is refused with `Taken`.
3. **Round robin**: the items alternate between the bots; a bot that takes the other's item is refused with `NotYours`.
4. **Need or Greed**: one bot needs, the other greeds; the sign shows the server's numbers and the winner.

The bots are local players without a view of their own, so the plugin treats them as real players. The scenario (`ACKLootShowcaseChest`) calls *Drop Loot*, *Try Pick Up* and *Try Choose* of the loot subsystem. These are the same server checks that a real player's request goes through. The stands use the tables `DA_LootTable_Showcase*`, in which only players within 3 m of the chest take part. On a dedicated server there are no bots, and the scenarios do nothing.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Loot Table Data*) or with **New Loot Table** in the CoopKit Hub. The wizard asks for the name, the sharing mode, the drop mesh and the icon. It creates the table in a folder of its own with a starter entry and, if you want, its own rolls curve. A double click opens the card: *Properties*, *Summary* (the chance of each item and the rolls for 1 to 4 players), *Validation* (problems, refreshed on every change), *Parts* and *Preview*.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKLootTableData` (*Loot Table*) | Name; sharing mode (*Personal*, *Free For All*, *Round Robin*, *Need Greed*); entries: item id, name, item asset, rarity, weight, min and max count, guaranteed; *Rolls* and *Extra Rolls Per Player*, or a *Rolls Curve* by number of players; *Guarantee Rarity*; *Roll Seconds*; *Pickup Radius*, *Eligible Radius*, *Scatter Radius*; drop mesh and icon. | Name required; at least one entry; every entry needs an item id, a count of at least 1 and a max not under the min; a rolled entry needs a weight above 0; rolls need at least one rolled entry; the guaranteed rarity must exist among rolled entries; at least 1 s to choose; a pickup radius above 0. |

## How loot is shared

- **Drop.** *Drop Loot* finds the players who take part: every player of the match, or those within *Eligible Radius* of the location. *Personal* rolls the table once for each of them. The other modes roll it once, with *Rolls* plus *Extra Rolls Per Player* for each player beyond the first (or the *Rolls Curve*). Guaranteed entries drop on top of the rolls. With *Guarantee Rarity*, a drop without an item of that rarity or rarer gets its last roll again among such items.
- **Who may pick up.** A *Personal* drop belongs to its player and replicates only to that player. A *Round Robin* drop is assigned to the next player in id order, and the turn goes on across drops. A *Free For All* drop is free. A *Need Greed* drop is rolling until its roll closes.
- **Pickup.** The server checks in this order: *Invalid* (no drop, no pawn, or another player's personal drop), *Taken*, *Rolling*, *NotYours*, *OutOfRange* (farther than *Pickup Radius*), then your receiver (*Full* when it takes nothing). Units the receiver does not take stay on the floor.
- **Roll.** Each player who takes part chooses once: *RollClosed*, *NotEligible* and *AlreadyChosen* refuse a choice. The roll closes when everyone chose or when the server clock passes the deadline; a player who did not choose passes. The server draws 1 to 100 for every Need and Greed. The highest Need wins, else the highest Greed, and a tie is drawn again. The winner gets the drop assigned; if everyone passed, the drop becomes free for all.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Loot`):

- `CoopKit.Loot.Replication.DedicatedServer` and `.ListenServer`: a server and two clients in one editor process with 150 ms packet lag and 2% loss. Personal loot: each client gets its own drop, and for 30 frames no client holds a copy of another player's drop; the server lists one drop per player, and the listen host shows only its own. A client picks its drop up. Free for all: both clients grab one coin at the same moment; one gets it, the other gets `Taken`. Round robin: every client agrees who owns each potion; taking another's is refused with `NotYours`. Need or Greed: an early pickup is refused with `Rolling`; Need beats Greed on every client; a second choice is refused with `RollClosed`; the loser gets `NotYours`; the winner takes it. A roll with silent players closes at its deadline on the server clock. A far player is refused with `NotEligible` and `OutOfRange`.
- `CoopKit.Loot.Overview`: a listen server and a client on the overview map with the same lag and loss. Every stand plays, the signs show the server's answers, and the client sees the shared drops and never a bot's personal drop.
- `CoopKit.Loot.Demo.Panel`, `.RestartPlay`: in one window, personal loot is picked up and a Need roll is won and taken. The Play with 2 windows button restarts Play as a listen server with two windows; the client grabs a free item; with personal loot each window lists only its own drops. Every step is a click on a panel button.
- `CoopKit.Loot.Rules.*`, `CoopKit.Loot.Data.*`: weights, counts, the seed, roll counts and the curve, guarantees, Need or Greed with ties, turns, validation, the data editor.
- `CoopKit.Loot.Hub.*`, `.Workspace.*`, `.Skills.*`: the CoopKit Hub, the card, the wizard and the AI agent skill.

## Limitations and known issues

- No widget or input asset for your game ships with the plugin; the demo panel is a demo, not a UI to ship.
- On a listen server the host's machine holds every player's personal drops, because the server knows everything. The host's player sees only their own: the others are hidden on the host.
- The round-robin turn is one per world, not one per group.
- No pity across drops (a guarantee after a run of bad luck), no master looter and no bonus rolls. Keep a counter in your save game and call *Drop Items* with your own items.
- Dedicated servers are tested in the editor on a dedicated server world.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-loot`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change around your loot, for example "put the loot the players pick up into my inventory". In Claude Code you can also type `/coopkit-loot`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or use Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitLoot data in the open editor, without clicks. The editor module has `UCKLootAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKLootAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_loot` to `<YourProject>/.claude/skills/coopkit-loot` and to `<YourProject>/.agents/skills/coopkit-loot`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Loot talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Loot.Reject.Invalid` | Reason | Server | The drop is gone, the player has no pawn, or the drop is another player's personal loot. |
| `CK.Loot.Reject.Taken` | Reason | Server | Another player picked the drop up first. |
| `CK.Loot.Reject.Rolling` | Reason | Server | A Need or Greed roll runs on the drop. |
| `CK.Loot.Reject.NotYours` | Reason | Server | The drop is assigned to another player. |
| `CK.Loot.Reject.OutOfRange` | Reason | Server | The pawn is farther than the table's *Pickup Radius*. |
| `CK.Loot.Reject.Full` | Reason | Server | Your receiver took no unit of the item. |
| `CK.Loot.Reject.RollClosed` | Reason | Server | The drop has no running roll. |
| `CK.Loot.Reject.NotEligible` | Reason | Server | The player takes no part in the roll. |
| `CK.Loot.Reject.AlreadyChosen` | Reason | Server | The player has already chosen in the roll. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKLootPlayerComponent` | Player controller (the loot subsystem adds it to every player it drops loot for) | Yes (RPCs only) | *Pick Up*, *Roll*, *Get Received Units*. **Find Local Loot Player Component** finds the local one. |
| `ACKLootDrop` | Spawned by the loot subsystem | Yes | One item on the floor: item, phase, owner, roll. A personal drop is relevant only to its owner's connection. Subclass it in Blueprint for your own look and set it with *Set Drop Class*. |
| `ACKLootDemoHUD`, `ACKLootDemoPlayerController` | HUD class and player controller of the demo game mode | Controller: RPC only | The demo's panel of buttons and its Open the chest request. Demo only. |
| `ACKLootShowcaseChest` | Overview map | No (the server runs it) | The stands' scenario with bot players. |

### Interfaces (optional, implement on your actors or components)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKLootReceiver` (pawn, player controller, player state or one of their components; the first found in that order) | `ReceiveLoot(Player, Item, Drop)`: units taken | Without a receiver every pickup takes the whole drop and only fires the events. |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKLootSubsystem::OnLootDropped` | Server | Table, drops |
| `UCKLootSubsystem::OnLootPickedUp` | Server | Player, item (units taken), drop |
| `UCKLootSubsystem::OnRollClosed` | Server | Drop, winner (none when everyone passed) |
| `UCKLootPlayerComponent::OnLootReceived` | Server and owning client | Item (units taken), drop |
| `UCKLootPlayerComponent::OnRejected` | Server and owning client | Drop, reason (`Reject.*`) |
| `ACKLootDrop::OnDropChanged` | All machines that have the drop | Drop |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `ACKLootDrop` (personal) | Relevant only to the owner's connection (`bOnlyRelevantToOwner`, owner: the player's controller) | Actor channel |
| `ACKLootDrop::Item`, `Phase`, `AssignedTo`, `RollState` (choices, numbers, winner, deadline), `bPersonal`, `PickupRadius`, `TableName`, `MeshAsset` | Every client that has the drop (`COND_None`); `OnRep` fires *On Drop Changed* | Property |
| `UCKLootPlayerComponent::ServerPickUp`, `ServerRoll` | Owning client to server | Reliable: a click must not be lost |
| `UCKLootPlayerComponent::ClientReceived`, `ClientRejected` | Server to owning client | Reliable |

A roll's time left is counted from the server clock (`GetServerWorldTimeSeconds`) on every machine; the server sends only the deadline.

### API

| Name | Kind | Where |
|---|---|---|
| `UCKLootSubsystem` | World subsystem. Server: `DropLoot`, `DropItems`, `SetRandomSeed`, `TryPickUp`, `TryChoose`; any machine: `GetDrops`, `GetVisibleDrops`, `GetEligiblePlayers`, `SetDropClass` | Runtime module `CoopKitLoot` |
| `UCKLootPlayerComponent` | Blueprint and C++: `PickUp`, `Roll`, `GetReceivedUnits`, `FindLootPlayerComponent`, `FindLocalLootPlayerComponent` | Runtime module |
| `ACKLootDrop` | Any machine: `GetItem`, `GetPhase`, `GetMode`, `GetAssignedTo`, `IsPersonal`, `GetRollState`, `GetRollSecondsLeft`, `GetRollChoiceOf`, `IsInRoll`, `CanBePickedUpBy`, `GetRarityColor` | Runtime module |
| `CKLoot::Rules` | C++: weighted pick, roll count, guarantees, Need or Greed resolution, turns | Runtime module |

### Data assets

Create them in the Content Browser: *Add > CoopKit*. Double-click opens the CoopKit Loot data editor (Properties, Summary and Validation tabs, plus Parts and Preview).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKLootTableData` | See *Data assets* above. | See *Data assets* above. |

### CoopKit Hub

Data types of CoopKitLoot with the `CoopKitEntity` class metadata appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins.

From the hub you can:

- open an entity card: the data editor with Preview and Parts tabs;
- create an entity with the wizard of its type;
- check the project: `IsDataValid` of every entity and duplicate ids.

The Parts tab lists the assets an entity refers to through soft object properties marked with `CoopKitPart` metadata. An own part lies in the entity's folder. It is not referenced from outside the kit and follows the entity on rename, duplicate and delete. A shared part is never touched. Rename, duplicate and delete are workspace functions of the plugin; the hub does not call them yet.

New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

None: the plugin uses engine modules only (Gameplay Tags is an engine module).

## Changelog

See `CHANGELOG.md` next to `CoopKitLoot.uplugin` and the Changelog section of the Fab listing.
