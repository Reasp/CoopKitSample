# CoopKit Objectives

Copyright 2026 Ivan Iaitckikh.

CoopKit Objectives adds quests and objectives that a party of a co-op game plays together:

- **One quest log for the whole party.** The server keeps the quests of the match. Every player sees the same steps, counts and contributions, players who join later included.
- **Shared, every-player and any-player objectives in one step.** A shared objective counts the events of the whole party ("defeat 8 raiders together"). An every-player objective needs each player in the game to reach the count ("everyone picks up their gear"); a player who left does not hold the party back. An any-player objective is done by the first player who reaches it ("someone reaches the gate").
- **Events by Gameplay Tag.** Your game reports a kill, a pickup, a place or a used terminal with a tag on the server; every running quest counts it. A tag under the objective's tag counts too (`CK.Objectives.Event.Kill.Boss` counts for "any kill"). A ready area actor reports players who reach a place.
- **Branches, failures and time limits.** In an any-objective step, the objective that completes it chooses the next step. An objective may fail the quest. A step may have a time limit that fails the quest or moves it on.
- **Every player's contribution.** The server counts each player's events over the quest. A reward goes to every participant, to the players who contributed, or to the top contributor. A reward for a player who is away waits for that player and is handed out when the player comes back.
- **Start votes.** A quest may need a vote of the party: a share of the players must vote yes in time, and the players who voted yes play it.
- **The server decides and says why.** Starts, votes and abandons are checked on the server. A refused player gets the reason as an enum and a Gameplay Tag (`CK.Objectives.Reject.AlreadyActive`, `.NotYourQuest`, `.MissingPrerequisite` and others).
- **Your rewards, through an interface.** Rewards go through `ICKObjectivesRewardReceiver` on the player's pawn, controller or player state. The plugin ships a simple reward bag for games without an inventory.
- **Data assets with their own editor.** A quest is a data asset with a card in the CoopKit Hub: the **New Quest** wizard, a Steps tab that lists the steps with their branches, and Summary and Validation tabs.
- **Saved with the game.** The quests, counts, contributions and pending rewards are SaveGame properties of the quest log. Two functions keep votes and time limits across a save and a load.

The plugin changes none of your base classes. Put a player component on the player controller and report events from your game; the server makes the quest log on first use.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Objectives** in *Edit > Plugins* and restart the editor. No other plugin is required.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The panel shows the quests, their steps and counts, your rewards, and tells you what to do next. The demo has no enemies or pickups, so four panel buttons stand in for gameplay.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitObjectives/Maps/L_ObjectivesDemo`, then click **Play** in the toolbar.
2. Click **Start Clear the Outpost**. The quest runs, and its row shows the first step: *Defeat raiders 0/4 (together)*.
3. Click **Defeat a raider** four times. The row counts each click, and the quest moves on to *Grab your supplies*.
4. Click **Pick up a part** twice. Every player must pick up two parts; you are the only player, so the quest moves on to *Get out*.
5. Click **Reach the gate**. The quest is completed, and the panel shows your rewards: *Credits 100, Medal 1*. Next time, click **Use the terminal** twice instead: the quest takes the other branch, *Hold the terminal*.
6. To play as a party, click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
7. In window 1, click **Start Supply Run (vote)**. Window 2 shows **Vote yes** and **Vote no**. Click **Vote yes** in window 2: the quest runs for both of you and needs 4 crates, 2 per player.
8. Click **Pick up a part** in both windows until the crates are in. Both windows show every player's part, and both players get *Supplies 1*.
9. In window 2, click **Start Clear the Outpost**. Only window 2 shows **Abandon**: only the player who started a quest may abandon it. Click it, and the quest leaves the log in both windows.

How the demo is built:

- The game mode `BP_CKObjectivesDemoGameMode` uses the engine's `DefaultPawn`, the player controller **CKObjectives Demo Player Controller** (it carries the objectives player component and asks the server to report the panel's gameplay events), the player state **CKObjectives Demo Player State** (it carries the reward bag) and the HUD class **CKObjectives Demo HUD**, which draws the panel.
- The panel is C++, without Widget Blueprints. It calls only the plugin's Blueprint API: *Request Start*, *Request Vote* and *Request Abandon* on your player component. The answers, refusals included, come from its *On Request Result*. The rows read the quest log's *Get Quests*, *Get Phase Remaining Seconds* and the counts of each objective.
- The demo quests are built in code (`CKObjectivesDemoQuests.cpp`), so the demo needs no quest asset. The map has a gate area (**CKObjectives Area**): walking into it counts like the **Reach the gate** button.

In your project:

1. **Give players a voice.** Add **CKObjectives Player Component** to your player controller Blueprint.
2. **Make a quest.** In the CoopKit Hub, click **New Quest** (or *Add > CoopKit > Quest* in the Content Browser). Add steps; in each step, objectives with an *Event Tag*, a *Target Count* and a *Scope*; the *Next Step Id* of each step (empty: the quest is completed); rewards with their recipients. The Steps tab of the card lists the result.
3. **Report events.** From your game's code on the server, call **Report Objective Event** with the player's pawn, controller or player state and a tag, for example when an enemy dies (`CK.Objectives.Event.Kill`), an item is picked up, or an interaction finishes. Place a **CKObjectives Area** for places to reach.
4. **Start quests.** A player starts a quest with *Request Start* on the local player's component (**Find Local Player Component**). Server code calls *Start Quest* (no vote) or *Start Quest For Players* (only those players) on the quest log (**Get Or Create Quest Log**).
5. **Connect your rewards.** Implement **CK Objectives Reward Receiver** on the player state, pawn or controller (or a component of one of them): *Receive Quest Reward*. Each reward names its item by *Reward Id* and, optionally, *Reward Asset*. Without an inventory of your own, add **CKObjectives Reward Bag Component** to the player state.
6. **Build your panel.** Read the quest log's *Get Quests* (state, step, objective counts per player, contributions, votes), *Get Current Step* and *Get Phase Remaining Seconds*. Refresh on **On Quests Changed** and show refusals from **On Request Result**.
7. **Keep it in saves** (optional): call **Prepare For Save** on the quest log before you save and **Restore After Load** after you load. With CoopKit Save, implement `ICKSaveable` on the actor that carries the quest log and call them from *On Before Save* and *On After Load*.

## Overview map

`/CoopKitObjectives/Maps/L_ObjectivesOverview` shows the plugin on four stands and a welcome sign. Open it and click **Play**, alone or as a listen server. The overview map has no panel: the stands play by themselves, in a loop, and the last line of each sign says what the server counted or answered:

1. **One count, two players**: Ada and Ben hunt toward one shared count of 3; Ada counts 2, Ben 1; both get credits and the medal goes to Ada, the top contributor.
2. **Every player counts**: Cid picks up both of his gear items, and the quest waits for Dee; when Dee has hers, it is completed.
3. **The party picks the path**: Fay uses the terminal and the quest goes to *Hack*; in the next loop Eli reaches the gate and it goes to *Sneak*.
4. **Refusals with a reason**: Gus starts a quest; Hal starts it again and is refused with `AlreadyActive`, then tries to abandon it and is refused with `NotYourQuest`.

The bots are local players without a view of their own, so the plugin treats them as real players. The scenario (`ACKObjectivesShowcaseTrial`, one class with a mode per stand) reports the bots' events to the quest log and calls *Request Start* and *Request Abandon* on their player components, the same server path a player's click takes. On a dedicated server there are no bots, and the scenarios do nothing.

## Data assets

Create them in the Content Browser (*Add > CoopKit > Quest*) or with **New Quest** in the CoopKit Hub. The wizard asks for the name, the briefing, the steps, the rewards and the vote, and creates the asset in a folder of its own with a first step, so a new quest is valid at once. A double click opens the card: *Properties*, *Steps* (each step with its objectives, branches, next step and time limit, and a warning for a step no path reaches), *Summary*, *Validation* (problems, refreshed on every change), *Parts* and *Preview*.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKObjectivesQuestData` (*Quest*) | Name, briefing and icon; *Steps* (step id, name, *Objectives*, *Completion* all or any, *Next Step Id*, *Time Limit Seconds*, *Timeout*); objectives (id, name, *Kind*, *Event Tag*, *Target Count*, *Scope*, *Scale With Players*, *Next Step Id* as a branch, *Fails Quest*, *Target Actor Tag*); *First Step Id*; *Rewards* (id, name, optional asset, *Amount*, *Recipients*); *Required Quests*; *Repeatable*; *Require Start Vote*, *Vote Share*, *Vote Seconds*; an optional *Player Scale Curve* (players to a multiplier of shared counts). | Name and id required; at least one step; every step has an id, no id twice, at least one objective; every objective has an id (unique in its step), an event tag and a count of at least 1; every next step and branch names a step of the quest; time limits not negative; rewards have an id and an amount of at least 1; a quest does not require itself; a vote share above 0 and at most 1. |

## How a quest plays

- **Start.** The player component sends the request to the server (a reliable RPC with the quest's asset path). The server checks, in order: the quest is valid, it is not running already, it is not completed (unless *Repeatable*), the required quests are completed. A player's start of a quest with *Require Start Vote* opens a vote; otherwise the quest runs at once.
- **Party.** A quest a player starts is played by every player of the game, and players who join later join it. A quest that passed a vote is played by the players who voted yes. *Start Quest For Players* (server) names the players.
- **Vote.** The starter votes yes. The vote passes when *Vote Share* of the players in the game (rounded up) voted yes, and fails when that can no longer happen or *Vote Seconds* run out. A failed vote removes the quest from the log.
- **Count.** *Report Event* counts the event for every active quest whose current step has an objective with a matching tag. Shared objectives add to the party's count; every-player and any-player objectives add to the player's own count. A count stops at the target. Each counted event is also the player's contribution.
- **Step.** A step is done when all its objectives are (*All Objectives*), or when one is (*Any Objective*). The quest goes on with the branch of the objective that completed it, or the step's *Next Step Id*; no next step completes the quest. An objective with *Fails Quest* fails it; a time limit fails it or completes the step, by *Timeout*.
- **Rewards.** When the quest is completed, each reward goes to its recipients through their reward receiver. A recipient who is away, or whose receiver refuses, gets the reward later: the server tries again every second while the player is in the game.
- **Abandon.** Only the player who started a quest may abandon it; server code may abandon or fail any quest.

## Automation tests

Editor automation tests (Session Frontend, filter `CoopKit.Objectives`):

- `CoopKit.Objectives.Replication.DedicatedServer` and `.ListenServer`: a server and two clients in one editor process with 150 ms packet lag and 2% loss. Client A starts a quest for the party; the server counts A's and B's kills toward one shared count and every client sees each player's part; an every-player objective waits for B; the terminal branches to *Defend*; the refusals `AlreadyActive`, `NotYourQuest` and `NotVoting` reach the right client; the quest completes with credits for both and the medal for the top contributor; B opens a vote and A's yes starts it for both; a third client joins in progress and sees the log at once.
- `CoopKit.Objectives.Demo.Panel`, `.SharedQuest`, `.RestartPlay`: every step is a click on a panel button. One window plays Clear the Outpost to its rewards; a listen server and a client with 150 ms lag vote and collect crates together and both get the reward; the Play with 2 windows button restarts Play with two windows.
- `CoopKit.Objectives.Overview`: a listen server and a client on the overview map with the same lag and loss: the shared hunt and its medal, the every-player wait, both branches, both refusals; the client sees the completed quests.
- `CoopKit.Objectives.Rules.*`: the rules as pure functions; the quest log's server path (an open party, capped counts, a player who leaves and comes back for a waiting reward, branches, a vote that passes and one that runs out of time, abandon rules, time limits, targets and objectives set by the server, a closed party, scaled counts, a failing objective, a receiver that refuses); a save and load of the quest log with a time limit continuing on the new clock.
- `CoopKit.Objectives.Data.*`, `CoopKit.Objectives.Hub.*`, `.Workspace.*`, `.Skills.*`: validation of the quest data, the wizard and the card (Steps tab), the CoopKit Hub and the AI agent skill.

## Limitations and known issues

- No quest journal widget for your game ships with the plugin; the demo panel is a demo, not a menu to ship. You build the quest panel from the quest log's data and events.
- One entry per quest asset: a quest runs once at a time in the log. Personal tasks of single players are quests started for that player (*Start Quest For Players*).
- Quests are referenced over the network and in saves by their asset path. Use saved assets in your game. Quests built in code work inside one process only: the demo and overview maps use them, so they run in one editor process (Play with 2 windows included).
- No graph editor and no dialogue: the card lists the steps as text.
- The server counts the events your game reports; the plugin does not watch kills, pickups or interactions by itself.
- Dedicated servers are tested in the editor on a dedicated server world.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-objectives`. It works with Claude Code, and with Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change, for example "count our enemy kills toward quests". In Claude Code you can also type `/coopkit-objectives`. The skill adds the change from your own C++ module and does not edit the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It also adds a validation and an automation test. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- On Windows, install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitObjectives data in the open editor, without clicks. The editor module has `UCKObjectivesAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKObjectivesAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_objectives` to `<YourProject>/.claude/skills/coopkit-objectives` and to `<YourProject>/.agents/skills/coopkit-objectives`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Integration

How CoopKit Objectives talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Objectives.Event.Kill`, `.Collect`, `.Reach`, `.Interact` | Event | Your game (server) | Ready event tags for objectives; your own tags work the same way. `.Reach` is the default tag of the area actor. |
| `CK.Objectives.Reject.InvalidQuest` | Reason | Server | No quest, or the quest has no steps. |
| `CK.Objectives.Reject.NoQuestLog` | Reason | Server | The world has no quest log (a vote or an abandon before any quest ran). |
| `CK.Objectives.Reject.AlreadyActive` | Reason | Server | The quest is voting or running. |
| `CK.Objectives.Reject.AlreadyCompleted` | Reason | Server | The quest is completed and not *Repeatable*. |
| `CK.Objectives.Reject.MissingPrerequisite` | Reason | Server | A quest of *Required Quests* is not completed. |
| `CK.Objectives.Reject.NotActive` | Reason | Server | The quest is not running. |
| `CK.Objectives.Reject.NotVoting` | Reason | Server | The quest is not waiting for votes. |
| `CK.Objectives.Reject.AlreadyVoted` | Reason | Server | The player voted on this quest already. |
| `CK.Objectives.Reject.NotParticipant` | Reason | Server | The request has no player (a component outside a player controller). |
| `CK.Objectives.Reject.NotYourQuest` | Reason | Server | Only the player who started the quest may abandon it. |
| `CK.Objectives.Reject.UnknownObjective` | Reason | Server API | The current step has no objective with this id. |
| `CK.Objectives.Reject.RateLimited` | Reason | Server | More requests per second than the player component allows. |
| `CK.Objectives.Reject.NotAuthority` | Reason | Server API | A server function was called on a client. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKObjectivesQuestLogComponent` | The game state, or the tracker actor the server makes on first use | Yes | The quests. Server API: *Start Quest*, *Start Quest For Players*, *Vote*, *Abandon Quest*, *Report Event*, *Complete Objective*, *Set Objective Target*, *Fail Quest*, *Prepare For Save*, *Restore After Load*. **Find Quest Log**, **Get Or Create Quest Log**, **Report Objective Event** find it. |
| `UCKObjectivesPlayerComponent` | Player controller | Yes (RPCs only) | *Request Start*, *Request Vote*, *Request Abandon*; *Max Requests Per Second*. **Find Local Player Component** finds the local one. |
| `UCKObjectivesRewardBagComponent` | Player state, pawn or controller | Yes | A simple reward store: reward ids with amounts. Implements `ICKObjectivesRewardReceiver`. |
| `ACKObjectivesTracker` | Spawned by the server, or placed | Yes, always relevant | Carries the quest log. Subclass it to add components (for example a save component). |
| `ACKObjectivesArea` | Actor in the level | No (the server checks) | A place to reach: reports *Event Tag* for each player whose pawn comes within *Radius*, once per player by default. |
| `ACKObjectivesDemoHUD`, `ACKObjectivesDemoPlayerController`, `ACKObjectivesDemoPlayerState` | HUD, player controller and player state of the demo game mode | Controller: RPC only | The demo's panel of buttons, the gameplay stand-ins and the reward bag. Demo only. |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `ICKObjectivesRewardReceiver` (pawn, controller, player state or a component of one) | `ReceiveQuestReward(Player, Reward, Quest)` returns whether the reward was taken | Without any implementation, rewards stay pending in the quest log for the player (*Get Pending Rewards*). |

### Events

| Delegate | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKObjectivesQuestLogComponent::OnQuestsChanged` | All | Quest log |
| `UCKObjectivesQuestLogComponent::OnQuestAdded`, `OnStepStarted`, `OnObjectiveProgress` | All (on clients from the replicated log) | Quest log, quest, step id or objective id |
| `UCKObjectivesQuestLogComponent::OnQuestFinished` | All | Quest log, quest, state (Completed, Failed, or Removed when abandoned or the vote failed) |
| `UCKObjectivesQuestLogComponent::OnRewardGranted` | Server | Quest log, quest, player state, reward |
| `UCKObjectivesPlayerComponent::OnRequestResult` | Server and owning client | Request id, action, result, reason tag, quest |
| `UCKObjectivesRewardBagComponent::OnRewardBagChanged` | All | Bag |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKObjectivesQuestLogComponent::Quests` (quest path, state, step, step history, counts per objective and player, participants, contributions, votes, phase start on the server clock) | Every client (`COND_None`); `OnRep` fires the events | Property |
| `UCKObjectivesQuestLogComponent::PendingRewards`, `CompletedCount` | Every client (`COND_None`) | Property |
| `UCKObjectivesQuestLogComponent::MulticastQuestFinished` | Server to every client | Reliable: the log alone cannot tell an abandoned quest from a failed vote |
| `UCKObjectivesPlayerComponent::ServerRequest` | Owning client to server | Reliable: explicit player actions |
| `UCKObjectivesPlayerComponent::ClientResult` | Server to owning client | Reliable: the panel shows every refusal |
| `UCKObjectivesRewardBagComponent::Rewards` | Every client (`COND_None`) | Property |

Time limits and vote time are not replicated as numbers: the log replicates the start of the phase, and every machine computes the time left from `GetServerWorldTimeSeconds`.

### API

| Name | Kind | Where |
|---|---|---|
| Quest log | Blueprint and C++ (server): see Components; any machine: `GetQuests`, `GetQuestProgress`, `GetQuestState`, `IsQuestRunning`, `GetCurrentStep`, `GetObjectiveCount`, `GetObjectiveTarget`, `IsObjectiveComplete`, `GetContribution`, `IsParticipant`, `HasVoted`, `GetPhaseRemainingSeconds`, `GetObjectiveTargetActors`, `GetPendingRewards`, `GetPendingRewardsFor`, `GetCompletedCount` | Runtime module `CoopKitObjectives` |
| Player component | Blueprint and C++: `RequestStart`, `RequestVote`, `RequestAbandon`, `GetLastResult` | Runtime module |
| `CKObjectives::Rules` | C++: result tags, player keys, event matching, scaled targets, completion, branches, votes, reward recipients | Runtime module |
| `CKObjectives::Demo` | C++: `FQuestValues`, `MakeStep`, `MakeObjective`, `MakeReward`, `MakeTransientQuest` (quests in code, for tests) | Runtime module |

### With other CoopKit plugins

The plugins do not know each other; the sample project joins them with bridges:

- **CoopKit Save**: an actor that carries the quest log with a CoopKit Saveable component calls *Prepare For Save* and *Restore After Load* from `ICKSaveable`.
- **CoopKit Ping**: when a step starts, a bridge pings the actors named by the objectives' *Target Actor Tag* (*Get Objective Target Actors*), so every player sees where to go.
- **CoopKit Interact** and others: your code reports their events (an interaction finished, an enemy defeated) with *Report Objective Event*.

### CoopKit Hub

The quest type appears in the CoopKit Hub (*Tools > CoopKit Hub*), next to the types of other installed CoopKit plugins. The hub opens on categories, one per entity type. Click **Quest** to list your quests, and click **< Back** to return.

From the hub you can:

- open a quest's card: the data editor with Steps, Preview and Parts tabs;
- create a quest with the **New Quest** wizard;
- check the project: `IsDataValid` of every quest and duplicate ids.

The Parts tab lists the icon (a shared part) and the player scale curve (an own part in the quest's folder; **Create** makes one).

New quests are created in the folder from `EntityPathTemplate` in the `[CoopKit.Workspace]` section of `Config/DefaultEditor.ini`. The default is `/Game/CoopKit/{Folder}/{Name}`.

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| none | | |

## Changelog

See `CHANGELOG.md` next to `CoopKitObjectives.uplugin` and the Changelog section of the Fab listing.
