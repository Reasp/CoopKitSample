# CoopKit Draft

Copyright 2026 Ivan Iaitckikh.

Hero selection for a lobby or the start of a match. A draft mode is a data asset built from swappable rules: the order of turns, the heroes each player is offered, the checks every pick and ban must pass, and the hero the server picks when a player runs out of time. Optional rules add blind picks, reserve time, hero swaps between teammates and team votes for bans. The server runs the draft and checks every request. The draft state replicates to every client. In a blind pick or a team vote, the server sends each player only the picks and votes that player may see. Players act through a component on their player controller. Seven ready modes ship with the plugin. To make a new mode, combine rules in the editor. To add a new kind of rule, subclass a rule base in C++ or Blueprint. Neither needs a change to the plugin.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit Draft** in *Edit > Plugins* and restart the editor. The plugin uses only engine modules and enables no engine plugins.

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. The host's panel has a button for each ready mode; during a draft every panel shows the turn, the time left, the bans, a button for each hero and the players (*Player 1*, *Player 2*, ...) with their picks.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitDraft/Maps/L_DraftDemo`, then click **Play** in the toolbar.
2. Click **Half Pool**. The draft starts: the server offers a random half of the heroes, and the other heroes are grey. Click a hero. The draft completes with your pick.
3. Team modes need a player in each team. Click **Play with 2 windows**. Play restarts as a listen server with two windows. Your Play settings stay as they were.
4. In window 1, click **Snake Draft**. Each captain bans one hero, then the teams pick in turn. When the panel says *Your turn*, click a hero in that window. Banned and taken heroes turn grey. When a turn's time runs out, the server picks for the player.
5. In window 1, click **Blind Pick** or **Team Vote Bans** to try the other rules, or **Cancel the draft** to stop one.

How the demo is built:

- The game mode `BP_CKDraftDemoGameMode` uses the game state `BP_CKDraftDemoGameState`, which carries the draft component. The modes are the ready modes in `/CoopKitDraft/Data` with the hero pool `DA_DraftDemoHeroPool`.
- The game mode uses the HUD class **CKDraft Demo HUD**, which draws the panel; its *Demo Modes* list the mode buttons. The panel is C++, without Widget Blueprints. A mode button calls *Start Draft* on the server, a hero button calls *Request Pick* or *Request Ban* on your draft player component, and the swap buttons call *Request Swap* and *Answer Swap*. The panel greys out the heroes *Get Offered Heroes* and *Preview Action* rule out, and shows the refusals from *On Action Rejected*.
- Automation tests `CoopKit.Draft.Demo.MapsLoad`, `.ListenServer` and `.RestartPlay` load the map with no warning, then click the same buttons in one window and on a listen server with a client under 150 ms lag: a Half Pool pick, a cancelled draft, and a Snake Draft with two bans and two picks that completes on both machines.

In your project:

1. Add **CK Draft Component** to your lobby's game state, in its Blueprint or at runtime on the server.
2. *Content Browser > Add > CoopKit > Draft Hero Pool*. Add your heroes. Each hero has a unique **Id** (a name such as `Knight`), a display name, an optional role tag, an icon and a pawn class.
3. Pick a ready mode from `/CoopKitDraft/Data` (see Ready modes), or create one: *Add > CoopKit > Draft Mode*. Set its **Hero Pool**, **Team Count** and rules.
4. On the server, call **Start Draft** on the draft component with the mode. Use **Start Draft With Teams** when your game decides the teams.
5. Players act through **Request Pick**, **Request Ban**, **Hover Hero**, **Request Swap** and **Answer Swap** on their **CK Draft Player Component**. The library nodes *Request Local Pick*, *Request Local Ban* and *Hover Local Hero* call it for the local player.
6. Bind **On Hero Picked**, **On Turn Changed** and **On Draft Completed** on the draft component to update your UI. Bind **On Action Rejected** on the player component to show why the server refused a request. Read the picks with **Get Picked Hero** when the draft completes.

## Ready modes

| Asset | Teams | Order | Pool | Checks |
|---|---|---|---|---|
| `DA_DraftMode_TeamUniquePick` | 2 | Everyone picks at once (30 s) | Every hero | A hero once per team |
| `DA_DraftMode_SnakeDraft` | 2 | Each captain bans one hero (15 s per turn), then teams pick one player per turn in snake order: A, B, B, A... (20 s per turn) | Every hero | A hero once per match |
| `DA_DraftMode_HalfPool` | 1 | Everyone picks at once (30 s) | A random half of the heroes, the same for everyone | A hero once per team |
| `DA_DraftMode_RandomChoices` | 1 | Everyone picks at once (30 s) | Three random heroes for each player. No hero is offered to two players while the pool has enough heroes (3 per player: up to 3 players with the demo pool) | A hero once per match |
| `DA_DraftMode_BlindPick` | 2 | Everyone picks at once (30 s). Picks stay hidden from the other team until the turn ends. Then teammates may swap heroes for 30 s | Every hero | A hero once per team |
| `DA_DraftMode_ReserveDraft` | 2 | As Snake Draft (bans 15 s, picks 20 s). When a turn runs out, the player keeps going on 30 s of reserve time for the whole draft | Every hero | A hero once per match |
| `DA_DraftMode_TeamVoteBans` | 2 | Each team bans one hero by a vote (20 s per turn). Every player of the team votes and may change the vote until the turn ends. The hero with the most votes is banned. In a tie, the captain's vote wins if it is one of the tied heroes. Then teams pick one player per turn in snake order (20 s per turn) | Every hero | A hero once per match |

In every mode, a player who runs out of time gets the hero they hovered, if it is still valid. Otherwise they get a random valid hero. A ban that runs out of time is skipped. In Reserve Draft, the player first spends the reserve, and only then gets the hero or loses the ban. In Team Vote Bans, players who do not vote are not counted, and if nobody votes, the team's ban is skipped. All seven modes use the demo pool `DA_DraftDemoHeroPool`, which has ten heroes. Copy a mode and set your own pool.

## Rules

A mode has one rule for each axis. In the details panel, pick a rule class and set its options.

| Axis | Base class | Rules included |
|---|---|---|
| Order | `UCKDraftOrderRule` (*Build Turns*) | **Simultaneous Order**: everyone picks at once, after optional ban turns of the team captains. **Turn Order (Snake)**: captains ban, teams alternating, then one player per turn, in snake order or always team 0 first. |
| Pool | `UCKDraftPoolRule` (*Build Pools*) | **Random Pool**: a share or a number of heroes, the same for everyone. **Personal Choices**: N heroes for each player, optionally exclusive. Without a pool rule, everyone is offered every hero. |
| Checks | `UCKDraftSelectionRule` (*Check Action*) | **Unique Hero**: once per team or once per match. **Role Limit**: a maximum number of heroes per role tag in a team. A mode can chain any number of checks. |
| Timeout | `UCKDraftAutoPickRule` (*Choose Hero*) | **Auto Pick**: the hovered hero, else a random valid hero. Override *Choose Hero* to use a preferred list instead. |
| Visibility (optional) | `UCKDraftVisibilityRule` (*Get Turn Visibility*) | **Blind Pick**: picks, and optionally bans, stay hidden until their turn ends. **Team** hides them from the other teams, **Self** from everyone but the player. Use it with Simultaneous Order. |
| Reserve (optional) | `UCKDraftReserveRule` (*Get Reserve Seconds*, *Uses Reserve*) | **Time Reserve**: extra seconds for each player for the whole draft, used after a turn's timer runs out, on picks and optionally on bans. |
| Swap (optional) | `UCKDraftSwapRule` (*Check Swap*) | **Team Swap**: a player offers a teammate a swap of heroes, and the teammate accepts or declines. By default swaps happen in a window after the last pick (*Window Seconds*). *During Picks* allows them earlier. Offers expire after *Offer Seconds*. |
| Ban vote (optional) | `UCKDraftBanVoteRule` (*Is Vote Turn*, *Choose Ban*) | **Team Ban Vote**: each ban turn of the captain becomes a vote of the captain's whole team. *Tie Break* settles a tie: *Captain* takes the captain's vote if it is one of the tied heroes, otherwise the tied hero voted for first. *Random* takes a random tied hero. *Allow Vote Change* lets players change their vote until the turn ends. *Ban Without Votes* bans a random hero when no vote counts; without it the ban is skipped. |

The first team member is the team's captain. The captain does the bans. In a team vote, the captain's vote settles a tie. Turn times can be scaled for testing with the console variable `ck.draft.TurnTimeScale` (for example `0.1`).

**Your own rule.** Subclass a base in C++ or Blueprint (they are *Blueprintable* and *Edit Inline New*) and override its event. Then select your class in the mode. For example, a selection rule returns an empty tag to allow a request, or a reason tag to refuse it (your own tag, or `CK.Draft.Reject.Rule`). Rules run on the server. Clients can run the same checks on their copy of the state with **Preview Action** to grey out heroes, but the server's answer is final.

## How a draft runs

- **Start.** The server builds the pools with the pool rule and the turns with the order rule. Only the server has the random seed; clients receive the result. *Set Random Seed* fixes the seed of the random pools and timeout picks, which helps replays and tests.
- **Turns.** Each turn has an action (pick or ban), the players who act in it, and a time limit. The turn ends when every one of those players has acted. It also ends when the time runs out: the server measures time with `GetServerWorldTimeSeconds`, so every machine counts down by the server clock. A turn whose players all left the game ends at once.
- **Checks.** The plugin checks every request first: the draft is running, the player is in it, the hero exists, it is the player's turn and the right action, the player has not acted yet, the hero is offered to the player and is not banned, and a hero to ban is not already taken. Then the mode's checks run in order. The first refusal is the reason the player receives.
- **Blind picks.** In a hidden turn, the server keeps the pick and sends it only to the players who may see it. The other players learn that the player has picked (locked in), but not the hero. When the turn ends, the server sends the pick to everyone. A listen server host sees the draft as a player too. Server code that needs the hidden picks reads *Get Full State*.
- **Reserve time.** When a turn's timer runs out, a player who has not acted keeps going on their reserve. The reserve runs down by the server clock. When it is used up, a pick turn gives the timeout pick and a ban turn is skipped. *Get Reserve Seconds Left* and *Is Using Reserve* show it on any machine.
- **Team votes.** In a vote turn, a player's *Request Ban* is a vote. It passes the same checks as a ban. A player outside the voting team gets `NotYourTurn`. The turn ends when every member has voted or when the time runs out, so a vote can change only while someone in the turn has not voted yet. Then the server bans the hero with the most votes. Votes for a hero that can no longer be banned are not counted. When two teams vote in the same turn, each team's ban is counted in team order, so they never get the same hero. Only the voter's team receives the votes, and its members see *On Ban Vote Changed*. The other team sees who has voted, but not the votes. It learns the ban when the turn ends.
- **Swaps.** *Request Swap* offers a teammate a swap. The teammate sees *On Swap Offered* and answers with *Answer Swap*. The server checks the swap when it is offered and again when it is accepted: same team, both players have a hero, and swaps are open. Two offers to each other swap at once. A refusal, a declined offer or an expired offer comes back as *On Action Rejected*, with the action Swap.
- **End.** After the last turn, the draft is *Completed* and the picks are final. Players who join during a draft are not part of it. Start a new draft to include them.

## Integration

CoopKit Draft talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.Draft.State.Drafting` | State, on the game state (draft component) | CoopKit Draft | A draft is running. |
| `CK.Draft.State.Completed` | State, on the game state (draft component) | CoopKit Draft | The draft is over and the picks are final. |
| `CK.Draft.Reject.NotDrafting`, `.NotParticipant`, `.NotYourTurn`, `.WrongAction`, `.AlreadyActed`, `.UnknownHero`, `.NotInPool`, `.Banned`, `.Taken`, `.RoleLimit`, `.Rule` | Reject reason | CoopKit Draft (and your rules) | Why the server refused a pick or ban. *Get Reject Reason Text* gives a short English text for each. |
| `CK.Draft.Reject.SwapDisabled`, `.SwapClosed`, `.NotTeammate`, `.NoHero`, `.NoOffer`, `.SwapDeclined`, `.SwapExpired` | Reject reason | CoopKit Draft (and your swap rules) | Why a swap was refused, declined or expired. |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKDraftComponent` | Game state | Yes | Runs the draft on the server. Holds the whole draft state. Works with any game state class. |
| `UCKDraftPlayerComponent` | Player controller | Yes (owner) | The player's requests, and in a blind pick the player's own copy of the draft. The draft component adds it on the server (*Add Player Components*). |
| `ACKDraftDemoHUD` | HUD class of the demo game mode | No | The demo's panel of buttons and mouse cursor for the local player; *Demo Modes* lists the mode buttons. Demo only. |

### Events

| Delegate | Fires on | Payload |
|---|---|---|
| `UCKDraftComponent::OnPhaseChanged` | All | New phase (Idle, Drafting, Completed) |
| `UCKDraftComponent::OnTurnChanged` | All | Turn index |
| `UCKDraftComponent::OnHeroPicked` | All | Participant index, player state, hero id (also for picks made on timeout) |
| `UCKDraftComponent::OnHeroBanned` | All | Participant index, player state, hero id. For a team vote's ban: participant -1 and no player state; the ban's *Team* names the team |
| `UCKDraftComponent::OnBanVoteChanged` | The voter's team (and a dedicated server) | Participant index, player state, hero id |
| `UCKDraftComponent::OnHeroHovered` | All | Participant index, player state, hero id |
| `UCKDraftComponent::OnSwapOffered` | All | Offering participant, asked participant |
| `UCKDraftComponent::OnHeroesSwapped` | All | The two participants (On Hero Picked fires for both too) |
| `UCKDraftComponent::OnDraftCompleted` | All | - |
| `UCKDraftPlayerComponent::OnActionRejected` | Owning Client (and a listen server's own player) | Action, hero id, reason tag |

"All" means the server at once and every client when the state replicates. Events follow what the machine may see: in a blind pick, the other team gets On Hero Picked when the turn ends.

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKDraftComponent::PublicState` (phase, mode, pool, participants with team, pick, hover, choices and reserve, turns, turn end time, bans, swap offers; never ban votes) | All clients | Property |
| `UCKDraftPlayerComponent::PrivateState` (the same state as this player may see it, with the ban votes of the player's team; empty without hidden turns or team votes) | Owning client only (`COND_OwnerOnly`) | Property |
| `UCKDraftPlayerComponent::ServerRequestAction`, `ServerHoverHero`, `ServerSwap` | Client to server | Reliable |
| `UCKDraftPlayerComponent::ClientActionRejected` | Server to owning client | Reliable |

Without hidden turns or team votes, every client receives the whole state. With them, the public state leaves out every hidden pick and ban and every vote, and each player receives their own state on their player controller. No client ever receives a pick hidden from that player, or another team's vote.

### Data assets

Create them in the Content Browser: *Add > CoopKit*, or make a draft mode with **New Draft Mode** in the CoopKit Hub (below). Double-click opens the CoopKit Draft data editor: *Properties*, *Summary* and *Validation*; a draft mode opens as a card with two more tabs. In a mode, the Validation tab updates when you edit the mode or one of its rules.

| Data type | What it sets | Validation |
|---|---|---|
| `UCKDraftModeData` (primary asset type `CKDraftMode`) | Display name, description, hero pool, team count, rules | A hero pool with heroes and an order rule are required. Each rule checks its own options (no negative times, a valid fraction, role tags set). |
| `UCKDraftHeroPoolData` (primary asset type `CKDraftHeroPool`) | Heroes: id, display name, role tag, icon, pawn class | At least one hero. Ids must be unique and not empty. Warns about heroes without a name. |

### Draft mode card and the New Draft Mode wizard

Draft modes are an entity of the CoopKit Hub (*Tools > CoopKit Hub*, category **Draft Mode**). The hub lists each mode by its display name next to its asset name; a mode without a display name (a new one from the wizard) shows its asset name.

- **Draft mode card** (double-click a draft mode, or *Open* in the hub): Properties, **Preview**, **Parts**, Summary and Validation. The Parts tab shows the **Hero Pool** of the mode. The row has a thumbnail, a label (own or shared) and the buttons **Open**, **Replace** and **Create**. **Create** makes a new hero pool `DA_DraftHeroPool_<Name>` in the mode's folder and gives it to the mode.
- **New Draft Mode wizard** (the hub button **New Draft Mode**, or *Add > CoopKit > Draft Mode* in the Content Browser): **Start From** (a mode to copy; its rules are copied, not shared; leave it empty for a plain mode where everyone picks at once), **Create Hero Pool** (on by default) and **Hero Pool**. With *Create Hero Pool* on, the wizard makes the mode's own hero pool `DA_DraftHeroPool_<Name>` next to it, with the heroes of the pool you pick. If you pick none, it takes the heroes of the *Start From* mode, and if there are none, the plugin's demo heroes. With *Create Hero Pool* off, the mode uses the pool you pick where it is. If you pick none, it uses the pool of the *Start From* mode.
- **Own and shared parts.** An own part lies in the mode's folder, and nothing outside the mode uses it. A hero pool elsewhere, or one that other modes use too (the demo's pool), is shared and is never moved or deleted. The plugin has functions that rename, duplicate and delete a draft mode together with its own hero pool. The hub has no buttons for them.
- **Folders.** New draft modes go to `/Game/CoopKit/DraftMode/<Name>/`. To change that, set the key `EntityPathTemplate.Draft Mode` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Modes you already have stay where they are, and the hub lists them there. A folder that is not named after the mode (such as the demo's `/CoopKitDraft/Data`) is never treated as its own folder.

A hero pool is not an entity of the hub. Create it with *Add > CoopKit > Draft Hero Pool*. It keeps its own data editor (Properties, Summary, Validation).

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Draft Mode** | `UCKDraftModeData` with the class metadata `CoopKitEntity="Draft Mode"`, `CoopKitEntityFolder="DraftMode"` | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKDraftHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitDraftEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Draft Mode`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| None | - | - |

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-draft`. It works with Claude Code, Antigravity, Codex and other agents that read `.agents/skills`.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to hero selection, for example "heroes the player has not unlocked cannot be picked". In Claude Code you can also type `/coopkit-draft`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds a validation and an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitDraft data in the open editor, without clicks. The editor module has `UCKDraftAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKDraftAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_draft` to `<YourProject>/.claude/skills/coopkit-draft` and to `<YourProject>/.agents/skills/coopkit-draft`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## Console commands for developers

The demo needs no console. For development and automation the plugin has console commands:

| Command | Where | What it does |
|---|---|---|
| `ck.draft.Start <mode path> [seed]` | Server | Starts a draft with every player |
| `ck.draft.Cancel` | Server | Cancels the running draft |
| `ck.draft.Pick <hero>`, `ck.draft.Ban <hero>`, `ck.draft.Hover <hero>` | Any player | The local player's request. In a team vote, `ck.draft.Ban` is the player's vote. An optional last argument picks another player controller of this machine by index (split screen) |
| `ck.draft.Swap <participant>`, `ck.draft.AnswerSwap <participant> <1 or 0>` | Any player | Offers the teammate with this participant index a swap. Or accepts (1) or declines (0) the offer from the teammate with this index. `ck.draft.Status` prints the indexes. An optional last argument picks another player controller of this machine by index |
| `ck.draft.Status` | Any machine | Prints the draft to the log |
| `ck.draft.TurnTimeScale <x>` | Server | Scales the turn times of the next drafts |

## Overview map

`/CoopKitDraft/Maps/L_DraftOverview` shows the four ready modes on four stands. Three bots and the players draft live; the stands take turns, and turns are shorter than in the modes while the map runs. The viewer is in every draft too; the server picks for you when your turn runs out. The overview map has no panel; to pick yourself, use the demo map from Quick start.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.Draft`):

- `CoopKit.Draft.Hub.*`, `CoopKit.Draft.Workspace.*` - the CoopKit Hub (which plugin raises the hub window, entity types, categories, New and Open), the New Draft Mode wizard with its hero pool, the card's Hero Pool part, the workspace functions that rename, duplicate and delete a draft mode with its own hero pool, and the demo modes in their older folder.
- `CoopKit.Draft.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.
- `CoopKit.Draft.Demo.*`, `.Replication.*`, `.Overview`, `.Core*`, `.Data.*Editor` - the demo map, the draft on a listen and a dedicated server, the overview map, the rules and the data editors.

## Known limits

- No draft screen for your game in the plugin: build yours from the Blueprint events. The demo panel is a demo, not a screen to ship.
- A team vote gives every player one vote of the same weight. For weighted votes, override *Choose Ban*. Reserve time is per player, not per team, and a vote turn uses no reserve.
- In a blind pick with a hero once per match, a Taken refusal tells a player that someone picked that hero. The mode's validation warns about this; use a hero once per team.
- The order of a tournament draft (for example ban-ban-pick phases with fixed sides) needs its own order rule.

## Changelog

See `CHANGELOG.md` next to `CoopKitDraft.uplugin` and the Changelog section of the Fab listing.
