# CoopKit Run

Copyright 2026 Ivan Iaitckikh.

CoopKit Run is the run structure of a co-op roguelite: rooms, upgrades, a path vote and meta progress for a team of players.

- **One seed, the same run on every machine.** The server sends one run seed. Every machine derives each room's seed from it, so every client builds the same level without level data on the network. The same seed and the same picks replay the same run: rooms, cards, doors and ties. A seed has an 8-character code (`4AU7EY6U`), and there is a seed of the day.
- **Hidden picks.** After a room, each player gets their own cards. Only that player's machine receives them. The others see who has chosen; the picks show when everyone has chosen. One reroll per run by default.
- **The team picks the door.** After a room the team votes on the next one. The most votes win; a tie is decided by the run seed (or by the host's vote), the same way everywhere.
- **Stages and room types as data.** A stage has its rooms, its first and final room types and its modifiers. A room type has a weight, a first possible room, a limit per stage, a currency reward and an optional map for a server travel.
- **Modifiers.** Elite foes, darkness, haste: the team chooses them at the start, and a stage adds its own or draws them from the seed. Your game reads multipliers by name (`EnemyHealth`) and tags.
- **Meta progress.** Players earn currency in runs and buy nodes of a meta track with it: more rerolls, more cards, starting upgrades, and upgrades and modifiers that only owners get. The progress is kept in each player's own save slot, by the server, or by your backend through an interface. The server cleans what a client sends.
- **Join in progress and come back.** A player who joins a running run gets the team's upgrades and catch-up picks. A player who leaves does not hold the team back; their upgrades and rewards wait for them.
- **Upgrades on your characters.** An upgrade can carry a Gameplay Effect (level = stacks) that the server applies to the player's Ability System Component, or reach your pawn through an interface.
- **The server decides and says why.** Every request is checked on the server. A refused player gets the reason as an enum and a Gameplay Tag (`CK.Run.Reject.AlreadyChosen`, `.NoRerolls`, `.InvalidOption`, `.NotHost` and others).
- **Data assets with their own editor.** Runs, upgrade pools, modifiers and meta tracks have a card in the CoopKit Hub and a wizard. The run's card simulates a whole run from any seed without Play.

The plugin changes none of your base classes. The server makes the run director on first use and gives every player controller the player component.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Run** in *Edit > Plugins* and restart the editor. It enables the engine's **Gameplay Abilities** plugin, which applies upgrade effects. No other plugin is required.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows the run, your cards, the doors and your meta progress, and tells you what to do next. The demo has no fights, so two buttons stand in for gameplay.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitRun/Maps/L_RunDemo`, then click **Play** in the toolbar.
2. Click **Start a run: random seed**. The panel shows the seed code, *Stage 1/2, room 1/5: Combat* and the room seed. The blocks in front of you are built from the room seed.
3. Click **Clear the room (stands in for the fight)**. Your cards appear as buttons, for example **Take Swift Boots (common)**. Click one, or first click **Reroll the cards (1 left)** for new ones.
4. Two doors appear, for example **Door 1: Event (0 votes)** and **Door 2: Treasure (0 votes)**. Click one. The team walks to the next room.
5. Play on to the boss of each stage. After the second boss the panel says *Run over: victory after 10 rooms* and pays shards.
6. Under the run, buy a node, for example **Buy Reroller for 20: +1 reroll in every run**. Click **+50 shards (demo shortcut)** if you need more. Every later run has the extra reroll.
7. Before a start, click modifiers to turn them on, for example **[x] Elite Foes**. **Start again: seed** (with the code) plays the same rooms and cards again. **Start a run: seed of the day** gives every player the same seed today.
8. To play as a team, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
9. Start a run in window 1 and clear the room. Each window shows only its own cards. Window 1 shows *Player 2: no upgrades - choosing*, then *- chose*, but never the card. Vote for different doors in the two windows: the run seed decides the tie, the same way in both windows. **Team wipe (the run is lost)** ends the run in defeat; both players still get shards for the rooms they cleared.

How the demo is built:

- The game mode `BP_CKRunDemoGameMode` uses the engine's `DefaultPawn`, the player controller **CKRun Demo Player Controller** (it asks the server for the demo's stand-in buttons) and the HUD class **CKRun Demo HUD**, which draws the panel.
- The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: *Request Start Run*, *Request Pick*, *Request Reroll*, *Request Vote*, *Request Abandon* and *Request Unlock* on your player component. The answers, refusals included, come from its *On Request Result*.
- The demo's run, upgrades, modifiers and meta track are built in code (`CKRunDemo.cpp`), so the demo needs no data asset. The room in front of you is a **CKRun Demo Room Builder**: it reads the room seed and places 5 to 9 blocks with **Random Integer From Seed**, the same in every window.

In your project:

1. **Make a run.** In the CoopKit Hub, click **New Run** (or *Add > CoopKit > Run* in the Content Browser). The wizard makes a run with a first stage and room types, and an upgrade pool with three upgrades. The run is valid at once. Add room types, stages, modifiers and a meta track on the card. Its **Simulate** tab plays the run from any seed.
2. **Start it.** Server code calls **Get Or Create Run Director**, then **Start Run** with the run, a seed (0: random) and the modifiers. Or a player calls **Request Start Run** on the local player component (**Find Local Player Component**). By default only the host may start.
3. **Build each room from its seed.** Bind **On Room Entered** of the run director on every machine. Build the level from the room's *Room Seed* with **Random Integer From Seed** or **Random Float From Seed**, one index per draw. Never use the engine's random nodes for the layout. A room type with a *Map* moves the team to that map instead: call **Travel To Room** on the server.
4. **End each room.** On the server, call **Complete Room** with *Success* true when the room is won. Call it with false when the team is wiped: the run ends in defeat.
5. **Make your screens.** The choice screen reads **Get Local Offer** (this player's cards). The doors read **Get Public State** (*Path Options*, every player's *Vote*). The HUD reads **Get Seed Code**, **Get Phase Seconds Left** and **Get Modifier Value**. Refresh on **On Phase Changed**, **On Offer Changed** and **On Votes Changed**, and show refusals from **On Request Result**.
6. **Apply the upgrades.** Give an upgrade a *Gameplay Effect*: the server applies it to the player's Ability System Component at the level of its stacks. Or implement **CK Run Upgrade Receiver** on the pawn, controller or player state.
7. **Keep it in saves** (optional). Call **Prepare For Save** on the run director before you save and **Restore After Load** after you load. With CoopKit Save, subclass **CKRun Director** with a Saveable component and call them from *On Before Save* and *On After Load*. Meta progress saves itself.

## Overview map

`/CoopKitRun/Maps/L_RunOverview` shows the plugin on five stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The stands play by themselves, in a loop, and the last line of each sign says what happened:

1. **One seed, one dungeon**: Ada and Ben play seed 4AU7EY6U. Room after room, both draw the same number of blocks from the room seed. The stand only counts the blocks; the demo map builds them.
2. **Hidden picks**: Cid chooses; Dee sees only that Cid chose. When Dee chooses too, both picks show.
3. **The team picks the door**: Eli and Fay vote for the same door. In the next loop they vote for different doors, and the run seed decides the tie.
4. **Refusals with a reason**: Gus picks twice (`AlreadyChosen`), Hal rerolls once too often (`NoRerolls`), Gus votes for a missing door (`InvalidOption`), and Hal tries to give up the run (`NotHost`).
5. **Join a running run**: Ivy plays alone. Jon joins with the team's Bulwark and one catch-up pick, and takes it.

A world has one run director, for the game's run. So each stand (`ACKRunShowcaseTrial`, one class with a mode per stand) keeps a run of its own and calls the same rules the director calls (`CKRun::Rules`, `CKRun::Core`) with its bots' player keys.

## Data assets

Create them in the Content Browser (*Add > CoopKit*) or with **New** in the CoopKit Hub. A double click opens the card: *Properties*, *Summary*, *Validation* (problems, refreshed on every change), *Parts*, *Preview*, and the tabs of each type.

| Data type | What it sets | Card tabs | Validation |
|---|---|---|---|
| `UCKRunData` (*Run*) | Name, description, icon and a difficulty curve; *Stages* (id, rooms, room types, first and final room type, modifiers, random modifiers); *Room Types* (id, tag, weight, choice, first room, limit per stage, map, currency); *Upgrade Pools*; *Choice* (cards, rerolls, seconds, catch-up); *Vote* (seconds, vote change, tie break); *Flow* (travel seconds, start policy); *Modifiers*; *Meta Track*; *Rewards*; the *Path Rule*, *Offer Rule* and *Auto Pick Rule*. | Stages; Simulate (a run from a seed, 1 to 4 players, by buttons) | Name required; room types and stages present, ids unique; every first, final and stage room type and every stage modifier exists; pools present, without repeated ids, with enough upgrades for an offer; numbers not negative; the meta track's starting upgrades exist in the pools. |
| `UCKRunUpgradePoolData` (*Upgrade Pool*) | *Upgrades*: id, name, description, icon, rarity, weight, max stacks, scope (player or team), granted, required and blocked tags, the meta node that unlocks it, a Gameplay Effect. | Odds (the chance of each rarity and upgrade) | Upgrades present; ids unique; names present; max stacks at least 1; weights not negative (a weight of 0 is a warning). |
| `UCKRunModifierData` (*Modifier*) | Id, name, description, icon, run tags, *Values* (name and multiplier), difficulty points, reward multiplier, the meta node that unlocks it. | | Id and name present; multipliers not negative; a modifier without values and tags is a warning. |
| `UCKRunMetaData` (*Meta Track*) | Id, name, currency name, icon, maximum balance, *Storage* (Local Profile or Server), *Store Class*, *Unlocks* (id, name, cost, prerequisites, bonus rerolls, bonus cards, starting upgrades). | Unlocks (the tree in buying order) | Id and name present; node ids unique; costs not negative; prerequisites exist; no cycle. |

## How a run plays

- **Start.** The server checks the run data, that no run is going on, and the start policy (host only by default). It sets the run's modifiers and enters the first room. Every player gets the run's rerolls plus their meta bonuses and starting upgrades.
- **Room.** The phase is *In Room*. Your game plays the room and calls *Complete Room*. Success pays the room's currency to every player present; the choice, the vote and the travel follow. Failure ends the run in defeat.
- **Choice.** Each present player gets cards from the offer rule, drawn from a seed of their own (run, room, player, reroll). A pick and a reroll carry the round and the reroll they were made for: a pick of old cards is refused, never applied. When everyone has chosen, or the time runs out (then the auto pick rule picks the highest rarity), the picks are revealed in run order and granted. A team upgrade goes to every player.
- **Vote.** The path rule builds the doors from the room's path seed; with one door there is no vote. The vote closes when everyone has voted or the time runs out. A tie is decided by the tie seed, or by the host's door with *Tie Break: Host*.
- **Travel.** *Enter Next Room* (same map) or *Travel To Room* (a server travel to the room type's map; the run continues on the new map). The final room of the last stage ends the run in victory.
- **End.** On a victory, a defeat or an abandon, the server pays meta currency to every player: rooms, stages and the victory, times the modifiers' reward multiplier. A player who is away gets it on return.

## Integration

How CoopKitRun talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Run.Phase.InRoom`, `.Choosing`, `.PathVote`, `.Travel`, `.Ended` | state | the run director (`GetOwnedGameplayTags`) | The run's phase on every machine. |
| `CK.Run.Room.Combat`, `.Elite`, `.Treasure`, `.Rest`, `.Event`, `.Boss` | data | room types | The kind of a room. Add your own tags under `CK.Run.Room`. |
| `CK.Run.Modifier.Elite`, `.Darkness`, `.Haste`, `.Glass` | data | the demo's modifiers | Run tags of a modifier; your game reads the active modifiers' tags. |
| `CK.Run.Reject.*` (`AlreadyChosen`, `NoRerolls`, `InvalidOption`, `NotHost`, `WrongPhase`, `NotEnoughCurrency` and others) | event payload | the server | Why a request was refused. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKRunComponent` (run director) | `ACKRunDirector` (spawned on first use), or your game state | yes | The run: a public state for everyone, the hidden offers on the server only. |
| `UCKRunPlayerComponent` | player controller (added by the director) | yes; the cards to the owner only | The player's requests and own cards. |
| `UCKRunMetaComponent` | player state (added by the director when the run has a meta track) | yes | The player's meta progress, visible to teammates. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKRunUpgradeReceiver` (pawn, controller or player state, or a component of one) | `OnRunUpgradeGranted(UpgradeId, Stacks)`, `OnRunUpgradeRemoved(UpgradeId)` | Only the upgrade's Gameplay Effect applies. |
| `ICKRunMetaStore` (the meta track's Store Class) | `LoadMeta(MetaId, SlotKey, OutProgress)`, `SaveMeta(MetaId, SlotKey, Progress)` | `UCKRunLocalMetaStore`: a save game slot per local player. |

### Events

| Delegate | Fires on | Payload |
|---|---|---|
| `OnRunStarted` | All | Run id |
| `OnPhaseChanged` | All | Phase |
| `OnRoomEntered` | All | The room: type, tag, stage, depth, room seed, final |
| `OnOfferChanged` | Owning client (and a listen host) | The local player's cards |
| `OnVotesChanged` | All | |
| `OnUpgradeGranted` | All | Player key, upgrade id, stacks |
| `OnPlayerJoinedRun` | All | Player key |
| `OnRunEnded` | All | Run id, reason |
| `UCKRunPlayerComponent::OnRequestResult` | Owning client | Action, result, reason tag |
| `UCKRunMetaComponent::OnMetaChanged` | All | Progress |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKRunComponent::PublicState` (seed, room, phase, players, votes, modifiers) | all | property |
| `UCKRunPlayerComponent::PrivateOffer` (the player's cards) | owner only | property |
| `UCKRunMetaComponent::Progress`, `MetaPath`, `bReady` | all | property |
| `ServerRequestStartRun`, `ServerRequestPick`, `ServerRequestReroll`, `ServerRequestVote`, `ServerRequestAbandon`, `ServerRequestUnlock`, `ServerSubmitMeta` | client to server, at most 10 requests per second | yes |
| `ClientRequestResult` | server to the requesting client | yes |
| `MulticastRunEnded` | server to all | yes |

### CoopKit Hub

The run, upgrade pool, modifier and meta track types appear in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. From the hub you open an entity's card, create an entity with the wizard of its type, and check the project (`IsDataValid` of every entity and duplicate ids). New entities are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| Gameplay Abilities | Required (upgrade effects) | Stable |

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-run`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change, for example "add a shop room before every boss" or "save meta progress to our backend". In Claude Code you can also type `/coopkit-run`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also work with CoopKitRun data in the open editor, without clicks. The editor module has `UCKRunAgentLibrary` (list, create, describe, validate, export and import the data assets) and `UCKRunAgentOps` (simulate a run from a seed, preview an offer, describe a meta track, validate a run with everything it uses), for Python and Editor Utility Blueprints. The answers are JSON. You can ask your agent, for example: "what does seed 4AU7EY6U play with three players?". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_run` to `<YourProject>/.claude/skills/coopkit-run` and to `<YourProject>/.agents/skills/coopkit-run`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Tests

The plugin's automation tests run in the editor (*Tools > Session Frontend > Automation*, filter `CoopKit.Run`) or from the command line. They cover the seed contract (the generator's output against fixed control values), the rules and their order, a whole run on the pure functions, meta progress, the data editors, the demo by clicks, and the network: a dedicated and a listen server with two clients at 150 ms of lag and 2% packet loss, a player who joins during a vote, and a server travel.

A player who drops and comes back is tested on the server without a network: the engine keeps the player's state while they are away, the team goes on without them, and on return their upgrades, their waiting cards and their meta reward are there. The tests do not reconnect a client over the network, because Play In Editor connects each client once when Play starts.

## Limitations

- One run director per world: one run at a time in a match.
- The plugin decides the run's structure and its randomness. Your game builds the rooms, fights and rewards from the seeds and the events.
- The run does not carry your game's state between rooms (health, ammunition): your game keeps it.

## Changelog

See `CHANGELOG.md` next to `CoopKitRun.uplugin` and the Changelog section of the Fab listing.
