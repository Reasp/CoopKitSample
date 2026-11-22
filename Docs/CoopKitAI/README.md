# CoopKit AI

Copyright 2026 Ivan Iaitckikh.

CoopKit AI makes enemies fight a group of players instead of one:

- **Threat table per enemy.** Damage, AI Perception, proximity, noise and your own calls add threat towards players; threat decays and is forgotten. The enemy attacks the real foe with the most threat and switches only when another player clearly overtakes the current target (no target flicker). Taunts force a target for a while.
- **Attack tokens.** Only a few enemies attack the same player at once: an enemy asks the AI director for a token before it attacks. Limits per player and for the whole match, a hold time and a cooldown make the tokens go around.
- **Difficulty by player count.** A table of steps scales the attacker limits and reports damage, health and spawn budget multipliers for your own code.
- **Pacing and hordes.** A horde director. The players' tension (damage, enemies after them, downed teammates, your own signals) leads to build-up, peak and a break. Waves and named horde events bring enemies in groups. A wave's budget is scaled by the AI director's spawn budget multiplier. By default enemies skip spawn points a player sees, and they appear on the navigation mesh.
- **Downed players stop being targets.** Any actor that publishes a "no target" Gameplay Tag drops out of the enemies' choice; the downed and dead states of CoopKit Revive work out of the box, with no dependency between the plugins.
- **StateTree, EQS, Behavior Trees, C++.** StateTree tasks and conditions, EQS tests and a context, Blueprint and C++ API for Behavior Trees and your own code. An example enemy StateTree ships with the plugin, and an editor function builds it for your own assets.
- **Multiplayer first.** The server thinks; clients receive each enemy's current target and attack token (aggro indicators, attack telegraphs).
- **Data assets with their own editor.** Enemy threat profiles, director rules, pacing profiles and wave sets are data assets with a Summary and a Validation tab. All four also live in the CoopKit Hub, with a card and a wizard that makes a new one.
- **An AI agent skill.** `coopkit-ai` lets Claude Code, Antigravity, Codex and other agents add a targeting rule, a reaction to an enemy's target or difficulty by player count from your own C++ module.

Add one component to your game state and one to your enemy pawns; for waves, a second component on the game state. The plugin changes none of your base classes.

## Supported engine versions

Unreal Engine 5.6, 5.7 and 5.8. Platform: Windows (Win64).

## Installation

Install the plugin from Fab, then enable **CoopKit AI** in *Edit > Plugins* and restart the editor. The plugin enables the engine plugins **StateTree** and **GameplayStateTree** (stable).

## Editor tools, step by step

[EditorTools.md](EditorTools.md) shows the editor tools of this plugin in screenshots. You use only the mouse. The one thing you type is the name of a new asset.

## Quick start

The plugin ships a demo that is ready to play. Open the map and click **Play**. You do everything with the mouse, on a panel of buttons in the top left corner of each game window. Every window lists the enemies: who attacks you, who chases someone and waits its turn, and who is idle.

1. In the Content Browser, open **Settings** and turn on **Show Plugin Content**. Double-click `/CoopKitAI/Maps/L_AIDemo`, then click **Play** in the toolbar. Four enemies wait on the map.
2. Under **Send every enemy at:**, click **Player 1 (you)**. The panel numbers the players in the order they joined. Every enemy chases you. Only a few attack at a time, and the rest wait their turn. The list shows who is who.
3. Click **Calm the enemies**. Their threat is gone.
4. Click **Count 4 players (harder)**. The difficulty line shows how many enemies may attack one player at a time. Click **Count the real players** to go back.
5. To see the enemies share two players, click **Play with 2 windows** on the panel. Play restarts as a listen server with two windows. Your Play settings stay as they were. In window 1, send the enemies at either player. Window 2 lists the same enemies and shows when they attack you.
6. Click **Start the waves**. Three waves of demo enemies come around the players, away from where a player looks, in groups of two. The waves line shows the wave, the enemies still to come and the pacing: *build up* while the enemies come, *peak* when the players are tense, then *peak fades* and a *break* without new enemies. Click **Send a horde now** for a horde event on top of the waves, and **Stop the waves** to remove the horde's enemies.

How the demo is built:

- Four enemies `BP_CKAIDemoEnemy` (an engine Character with the threat component, controlled by `BP_CKAIDemoController` with a StateTree AI Component running `/CoopKitAI/AI/ST_CKAIExampleEnemy`), the game state `BP_CKAIDemoGameState` with the AI director, and a navigation mesh.
- The game mode `BP_CKAIDemoGameMode` uses the HUD class **CKAI Demo HUD**, which draws the panel. The panel is C++, without Widget Blueprints. It calls only the Blueprint API: Add Threat and Clear All Threat of the threat component, Set Player Count Override of the director. It reads Get Current Target and Holds Attack Token, which every window receives.
- Enemies also gain threat by themselves when a player comes within 8 m, as in your game.
- The waves come from a **CKAI Horde Component** that the panel adds to the demo's game state when you click **Start the waves**, with a wave set and a pacing profile made in code. In your game they are assets you edit in the CoopKit Hub.

In your project:

1. **Add the director.** Add **CKAI Director Component** to your game state Blueprint. With no data set it uses the plugin's default director data (*Project Settings > Plugins > CoopKit AI > Default Director Data*).
2. **Add the threat component** (**CKAI Threat Component**) to your enemy pawn. With no profile set it uses the plugin's default profile. Engine damage (*Apply Damage*, `TakeDamage`) adds threat by itself; call **Add Threat**, **Report Damage** or **Force Target** from your own code for anything else.
3. **Use the target.** In a StateTree (schema *StateTree AI Component*): start from the example tree (copy `ST_CKAIExampleEnemy`, or run `UCKAIEditorLibrary::CreateExampleStateTree` from an Editor Utility or Python), or use the tasks and conditions below. In a Behavior Tree or your own code: read **Get Current Target**, call **Request Attack Token** before an attack and **Release Attack Token** after it. On an AI controller, turn **Start Logic Automatically** off on the StateTree AI Component. The controller's **Start AI Logic On Possess** then starts the tree when it possesses the pawn. If you leave the option on, an AI created with Spawn Actor during play can start its tree before it has a pawn, and the tree does not run.
4. **Teams** (optional): implement `IGenericTeamAgentInterface` on your AI controller (or enemy pawn) and on your player controller, pawn or player state. With teams on both sides an enemy targets only hostile teams. Without teams, players are the enemies' foes (the engine's default attitude would call two actors without teams friends).
5. **Perception** (optional): give the AI controller an **AI Perception** component with the senses you need (for example *AI Sight config*); every successful sensing adds the profile's *Perception Threat*. The demo enemies use proximity instead.
6. **Scale your game** (optional): bind **On Scaling Changed** of the director, or read **Get Scaling**, to apply the damage, health and spawn budget multipliers.
7. **Waves and pacing** (optional): add **CKAI Horde Component** to your game state next to the director. Create a **Wave Set** and a **Pacing Profile** in the CoopKit Hub (*Tools > CoopKit Hub*), set the wave set as the component's *Wave Data* and the profile as the wave set's *Pacing* part. Place **CK AI Spawn Point** actors where enemies may come and give them the *Group* of the waves (a wave without a group spawns around the players on the navigation mesh). Turn on *Auto Start* or call **Start Waves** from your mission code. Apply health and damage multipliers to new enemies on **On Enemy Spawned**.

## Console commands for developers

The demo needs no console. For development and automation the plugin also has console commands: `ck.ai.AddThreat [amount]` makes the enemy nearest to the host angry with it, `ck.ai.SetPlayerCount [count]` sets the difficulty step, `ck.ai.Tokens` prints who holds attack tokens, and `ck.ai.Debug 1` in the server window draws every enemy's target, top threats and attack token (see *Settings, console and API* below).

## Overview map

The demo map `/CoopKitAI/Maps/L_AIOverview` has five stands. Open it and click **Play**: each stand plays by itself with bot players (blue) and enemies (red), and the sign at each stand says what happens. The enemies show their target, and whether they attack or wait for a token, over their heads.

1. **Threat decides the target.** One enemy, two bots. Ada makes noise (**Report Stimulus**), Ben hits the enemy twice (**Report Damage**). The first hit is not enough to take the target from Ada; the second one overtakes. Then Ada taunts (**Force Target**).
2. **Attack tokens.** Four enemies run the plugin's StateTree against one bot. Only as many attack as the director allows; they step forward while they hold a token, the others wait for their turn.
3. **Difficulty follows player count.** The director counts one to four players in turn (**Set Player Count Override**). Seats show the counted players; three bars show the damage, health and spawn budget multipliers of the step. The plugin only reports the multipliers, your game applies them.
4. **StateTree tasks and EQS tests.** One enemy runs the tasks *Threat Target*, *Hold Attack Token* and *Wait*. An EQS query with the *CK AI Threat* test ranks the bots around it, and the sign lists the ranking.
5. **Pacing and waves.** The stand has its own horde. The bot Gus looks across the stand. Of three spawn points, one is in front of him, one behind a wall and one behind his back. Raiders come in waves only at the last two. Their hits make Gus tense: the sign shows the tension rise to the peak, the peak fade, a break without raiders and the next wave.

The enemies and bots need no navigation mesh: the scenario moves them. The map uses the data assets `DA_AIOverviewProfile` (no proximity threat, so the stands decide who gets threat) and `DA_AIOverviewDirector` (the default steps), and the tree `ST_CKAIOverviewEnemy` (the example tree without the move task). On a dedicated server the map has no bots, so the stands do nothing there. `ck.ai.Debug 1` on the host draws the threat table over every enemy.

## Data assets

Create them in the Content Browser: *Add > CoopKit*, or with **New Threat Profile**, **New Director Rules**, **New Pacing Profile** and **New Wave Set** in the CoopKit Hub (below). A double click opens the CoopKit AI data editor: *Properties*, *Summary* and *Validation* (problems, refreshed on every change).

| Data type | What it sets | Validation |
|---|---|---|
| `UCKAIProfileData` (*AI Threat Profile*) | Target filter (*Players Only* or *Any Hostile*); untargetable tags; whether an untargetable actor's threat is cleared or frozen; switch threshold (a new target needs this many times the current one's threat); threat per point of damage and whether engine damage counts; threat per perception sensing; proximity radius and threat per second; stimulus threat scale; decay per second; forget after seconds without new threat and beyond a distance; whether the enemy needs an attack token. Summary: what gives threat and how fast, how the enemy forgets and switches targets. | Switch threshold at least 1; no negative weights, radii or times. Warnings: the enemy never forgets; nothing but Add Threat gives threat. |
| `UCKAIDirectorData` (*AI Director Data*) | Evaluation interval (how often enemies re-evaluate threat and tags); attack token hold time and cooldown; difficulty steps: from how many players, attackers per target, attackers in total (0: no limit), damage, health and spawn budget multipliers. Summary: the difficulty table and the token timing. | Interval above 0; times not negative; at least one step, steps in ascending order of players, at least 1 attacker per target, no negative limits or multipliers. Warning: the first step starts above 1 player. |
| `UCKAIPacingData` (*AI Pacing Profile*) | What makes a player tense (0 to 100): tension per point of damage, per second for every enemy after the player, per second for contract tags by name (by default downed and being revived in CoopKit Revive, reloading in CoopKit Combat); decay per second and whether it decays under pressure; the match takes the most tense player or the average. Phases: Build Up until the peak tension, Peak for some seconds, Peak Fade until the relax tension (or a time limit), Relax for some seconds. Spawn scale per phase (Peak Fade spawns nothing). Interval of automatic horde events during Build Up. Summary: the sources and the phases in words. | Peak above 0 and at most 100; relax below the peak; no negative values. Warnings: tension never decays and the fade has no limit; Build Up spawns nothing; enemies come during the break. |
| `UCKAIWaveData` (*AI Wave Set*) | Waves in order and named horde events, each with a budget at one player, kinds of enemies (class, cost, fixed count, fill weight, *Min Players*, *Outside Budget*), spawn group, group size and interval, its own limit of enemies alive, pause after, whether it waits until all its enemies are defeated, repeat interval (events), random fill, whether it follows the pacing. The event the pacing starts during Build Up. Enemies alive at once. Spawn rules: distance from players, out of sight (view cone, sight distance), on the navigation mesh, reachable, radius around the players without spawn points. Part *Pacing*: the pacing profile. Summary: every wave's enemies at 1 and 4 players, groups, pauses and where enemies come. | At least one wave or event; every wave has enemies, every enemy a class and a cost of at least 1; group size at least 1; no negative times or distances; event names unique and set; the pacing event exists. Warnings: fixed enemies cost more than the budget; the budget is not spent; no limit of enemies alive. |

The plugin's defaults live in `/CoopKitAI/Data`: `DA_AIDefaultProfile` and `DA_AIDefaultDirector` (1 to 4 players: 2 attackers per player, 3 from 4 players; health x1.25 per extra player).

### Cards and the New wizards

The CoopKit Hub (*Tools > CoopKit Hub*) lists threat profiles under the category **Threat Profile**, director rules under **Director Rules**, pacing profiles under **Pacing Profile** and wave sets under **Wave Set**. It shows each one by its asset name, because no type has a display name field.

- **Card** (double-click an asset, or *Open* in the hub): Properties, **Preview**, **Parts**, Summary and Validation. A wave set has one part, its **Pacing** profile: pick one, or click **Create** to make a new one in the wave set's folder. The other types refer to no other asset, so their Parts tab is empty and says so.
- **New Pacing Profile** and **New Wave Set** wizards work the same way. A new wave set starts with one wave of the plugin's demo enemy; replace the enemy class with your own.
- **New Threat Profile** and **New Director Rules** wizards (the hub buttons, or *Add > CoopKit > AI Threat Profile* / *AI Director Data* in the Content Browser): in **Start From**, pick an asset of the same type to copy its values from, or leave it empty for the plugin's defaults. Click **Create** or **Cancel**. Change the values in the card afterwards.
- **Folders.** New threat profiles go to `/Game/CoopKit/ThreatProfiles/<Name>/`, new director rules to `/Game/CoopKit/DirectorRules/<Name>/`, pacing profiles to `/Game/CoopKit/PacingProfiles/<Name>/`, wave sets to `/Game/CoopKit/WaveSets/<Name>/`. To change that, set the key `EntityPathTemplate.Threat Profile`, `EntityPathTemplate.Director Rules`, `EntityPathTemplate.Pacing Profile` or `EntityPathTemplate.Wave Set` in the `[CoopKit.Workspace]` section of your project's `Config/DefaultEditor.ini`. Assets you already have stay where they are, and the hub lists them there.

## AI agent skills

The plugin includes a skill for AI coding agents. It is called `coopkit-ai`. It works with Claude Code, Antigravity, Codex and other agents that read skills from the `.agents/skills` folder of a project.

The skill needs a C++ project to build. If your project is Blueprint-only, the skill tells you how to add a C++ class first.

Ask your agent for a change to how enemies pick and attack players, for example "enemies do not chase players farther than 30 meters from home", "enemies get more health with four players" or "the enemy roars when it spots a player". In Claude Code you can also type `/coopkit-ai`. The skill changes your project, not the plugin. If it has to edit the plugin as a last resort, it records the change in `CHANGES-LOCAL.md`. It adds an automation test for the change. Then it builds the Editor target with the engine's own build tool and runs the tests.

If the build cannot run, the skill tells you what to do:

- Install Visual Studio 2022 with the "Game development with C++" workload, including MSVC and the Windows SDK.
- If the editor is open with the project, close it, or press Ctrl+Alt+F11 in the editor so Live Coding compiles the change.

The skill can also edit CoopKitAI data in the open editor, without clicks. The editor module has `UCKAIAgentLibrary`, a function library for Python and Editor Utility Blueprints (`unreal.CKAIAgentLibrary` in Python). With it the agent lists, creates, describes, validates, exports and imports the data assets and gets the answers as JSON. You can ask your agent, for example: "make a new one, set its values and check it". The editor needs the *Python Editor Script Plugin* enabled in *Edit > Plugins*.

Install the skill into your project in one of two ways:

- In the editor, use *Tools > CoopKit > Install AI Agent Skills* or the *Install AI Agent Skills* button in the CoopKit Hub window. This copies the skills of every enabled CoopKit plugin into `.claude/skills/` and `.agents/skills/` of your project. It asks before replacing skills that are already there. Replacing removes any edits you made to those skill files.
- By hand, copy the plugin's folder `Extras/AgentSkills/coopkit_ai` to `<YourProject>/.claude/skills/coopkit-ai` and to `<YourProject>/.agents/skills/coopkit-ai`. The new folder name has a hyphen. The plugin's folder has an underscore because Fab file names allow only letters, digits and underscores.

Then open your agent in the project folder and ask for the change.

## StateTree and EQS

StateTree tasks and conditions (category *CoopKit|AI*; they bind *Actor* to the schema's context and work with the *StateTree AI Component* and *StateTree Component* schemas):

| Node | What it does |
|---|---|
| Task *CK AI Threat Target* | Outputs *Target Actor*, the enemy's current target. Running while there is a target; fails without one. |
| Task *CK AI Hold Attack Token* | Asks for an attack token: fails when refused, runs while held, succeeds when the token expires; returns it with a cooldown when the state exits. Put the attack in this state. |
| Task *CK AI Move To Threat Target* | Moves to the current target with the AI controller's path following (needs a navigation mesh); succeeds after *Run Seconds* (0: keeps running). |
| Task *CK AI Wait* | Runs until the enemy has a target, has lost it, or can get an attack token. |
| Condition *CK AI Has Threat Target* | The enemy has a target. |
| Condition *CK AI Attack Token Available* | The enemy holds a token or would get one now (takes nothing). |
| Condition *CK AI Actor Has Tag* | The actor publishes one or all of the tags (read from the actor and its components), for example `CK.Revive.State.Downed` on the target. |

The example tree `ST_CKAIExampleEnemy`: *Engage* (has a target, *Threat Target*) with *Attack* (token available, *Hold Attack Token* and *Move To*) and *Wait Turn* (*Wait* for a token and *Move To*), then *Idle* (*Wait* for a target).

EQS (category *CoopKit|AI*):

| Node | What it does |
|---|---|
| Test *CK AI Threat* | Scores or filters actors by the querier's threat towards them. |
| Test *CK AI Attack Slots* | Scores or filters actors by free attack slots (attackers per target minus tokens held for them): spreads enemies over the players. |
| Test *CK AI Actor Tags* | Passes actors that publish the tags, read from the actor and its components (the engine's Gameplay Tags test reads the actor only). |
| Context *CK AI Threat Target* | The querier's current target. |

Run queries from a Behavior Tree (*Run EQS Query*), from your code, or from the engine's StateTree task *Run Env Query*.

## How it works

- **Threat.** The server keeps a threat table on every enemy. Every evaluation (director data: *Evaluation Interval Seconds*, 0.25 s by default) the enemy adds proximity threat, decays and forgets rows, checks every row's tags and picks the target. An actor that publishes an untargetable tag keeps its threat frozen until the tag goes away (or loses it, see the profile).
- **Real foes.** A row is a valid target when the actor is alive, passes the profile's filter and is hostile: with teams on both sides by the enemy's team attitude, otherwise when the actor is a player's pawn. Override **Is Valid Target** in a subclass of the threat component for your own rule.
- **Untargetable tags.** `CK.AI.Block.Target`, the profile's *Untargetable Tags* and the project's *Contract Untargetable Tag Names* (names, not tags: by default `CK.Revive.State.Downed` and `CK.Revive.State.Dead`; a name nobody registers is skipped, so nothing breaks without CoopKit Revive).
- **Attack tokens.** The director grants a token when the target has fewer attackers than the step allows and the match has fewer than the total. A token returns by itself after the hold time, when the target changes or is lost, when the enemy is destroyed, or when the enemy gets `CK.AI.Block.Attack`. After a token returns from an attack, the enemy waits the cooldown. Without a director every enemy may always attack.
- **Difficulty.** The director counts player states that are not spectators, bots or inactive (or the number set by **Set Player Count Override**) and picks the step with the largest *Players* not above that count.
- **Stimuli.** **Report Stimulus** on the director gives every enemy within the radius threat towards the instigator: noise, alarms, pings.
- **Tension.** The horde component keeps a tension from 0 to 100 for every player on the server. Engine damage to the player's pawn (or **Report Player Damage** for damage that bypasses the engine, such as GAS), every enemy whose target is the player, the pacing profile's contract tags on the pawn or player state, and **CK AI Tension Source** on the pawn, the player state or their components raise it. It decays when nothing raises it. The match tension is the most tense player's (or the average).
- **Pacing.** Build Up turns into Peak at the peak tension; Peak lasts its seconds; Peak Fade waits until the tension falls to the relax level (or a time limit); Relax lasts its seconds; then Build Up again. Each phase has a spawn scale: group intervals are divided by it, and 0 brings no new enemies. Without a pacing profile it is always Build Up.
- **Waves.** **Start Waves** runs the waves of a set in order. When a wave starts, its budget is multiplied by the director's spawn budget multiplier for the counted players; fixed enemies come first, the rest of the budget is spent by fill weight, by cost (weights 2 and 1 spend 2 : 1), kinds with *Min Players* come only with enough players. Enemies come in groups while the pacing allows and fewer than *Max Alive* live. A wave ends when all its enemies are defeated (or, if *Wait Until Cleared* is off, when the last one came), then a pause, then the next.
- **Spawning out of sight.** Candidates are the wave group's **CK AI Spawn Point** actors, or random reachable points of the navigation mesh around the players. A point is dropped when it is too close to a player, too far, off the navigation mesh, or seen: inside a player's view cone with nothing of the world in between. Hidden points nearest to the players come first. When every point is seen the enemies wait; **Get Last Spawn Problem** says why. A pawn without a controller gets its default AI controller. An enemy leaves the count when it is destroyed, publishes a *Contract Defeated Tag Names* tag (by default `CK.Combat.State.OutOfHealth`), or **Report Enemy Defeated** is called.

## Automation tests

Editor automation tests (Session Frontend, or `Automation RunTests CoopKit.AI`):

- `CoopKit.AI.Threat` - threat from a player makes them the target; another player's damage (including engine damage) switches the target; a non-player pawn and an ally gain no threat; a hostile team counts; a downed player (`CK.Revive.State.Downed`) loses the target and gets it back when revived; `CK.AI.Block.Target`; taunts.
- `CoopKit.AI.Tokens` - six enemies on one player: never more attackers than the limit, tokens expire and go to others, a difficulty step raises the limit, a stunned enemy gets none, a destroyed enemy frees its token.
- `CoopKit.AI.StateTree` - the example tree on an AI controller idles without a target, takes and returns attack tokens, idles again when the target is lost.
- `CoopKit.AI.EQS` - the threat, actor tags and attack slots tests and the threat target context.
- `CoopKit.AI.Demo.Chase` - the demo map: enemies chase a player along the navigation mesh and take attack tokens.
- `CoopKit.AI.Demo.MapsLoad`, `.ListenServer`, `.RestartPlay` - the demo map loads with no warning in Map Check and Play. On a listen server with a client, the host clicks **Player 2**. Every enemy chases the client, and never more attack at once than the difficulty allows. The client's panel shows it. **Calm the enemies** and **Count 4 players (harder)** work. **Play with 2 windows** restarts Play with two windows.
- `CoopKit.AI.Overview` - a listen server and a client on the overview map with 150 ms packet lag and 2% loss. The stands and scenarios are in place, the bots are players and mannequins on both machines, and the client sees the enemies. The signs show Ada's noise, a first hit that is not enough and a second one that switches the target, a taunt; the client sees imps holding tokens while others wait, never above the director's limit; the client reads the one to four player difficulty steps; the Stalker runs its StateTree and the EQS test ranks the bots.
- `CoopKit.AI.Replication.DedicatedServer` / `.ListenServer` - with 150 ms packet lag and 2% loss every client sees the enemy's target and attack token change, a downed player's loss of the target, and a player joining the running game sees the current state.
- `CoopKit.AI.Rules` - target choice, attack tokens under random request orders, difficulty steps.
- `CoopKit.AI.Horde.Rules` - pacing phases, tension, wave budgets of a game with two filler enemies and a heavy one at one to four players, random fill, spawn point ranking.
- `CoopKit.AI.Horde.SpawnOutOfSight` - enemies come behind the player and behind cover, never at the point the player sees; with only the seen point left nobody comes and the horde says why; without spawn points demo enemies come around the player on the navigation mesh, out of sight, each with its AI controller.
- `CoopKit.AI.Horde.Replication.DedicatedServer` / `.ListenServer` - with 150 ms packet lag and 2% loss every client sees the wave, its enemies and the budget for one to four players; the enemies come only at the hidden point; the tension peaks, fades into a break; the next wave waits for the break to end; the waves complete; a named event starts.
- `CoopKit.AI.Demo.Waves` - on a listen server with a client the host clicks **Start the waves**, **Send a horde now** and **Stop the waves**; both panels show the waves and the pacing.
- `CoopKit.AI.Overview.Horde` - the horde stand: raiders never come at the point Gus sees, the client sees the peak and the break.
- `CoopKit.AI.Data.ProfileEditor`, `.DirectorEditor`, `.PacingEditor`, `.WaveEditor`, `CoopKit.AI.Tags.GetActorTags` - data editors and tag reading.
- `CoopKit.AI.Hub.*`, `CoopKit.AI.Workspace.*` - the CoopKit Hub (which plugin raises the hub window, entity types, categories, New and Open), the New wizards, the cards (a wave set's Pacing part), the workspace functions that rename, duplicate and delete the plugin's data, and the plugin's own data in its older folders.
- `CoopKit.AI.Skills.*` - the AI agent skill is found and installed into a project, also with the hub button.

## Limitations and known issues

- Not included: Smart Objects (they would pull in Beta and Experimental engine plugins), attack token priorities (a stronger attacker taking a token away).
- The plugin owns no damage or health: the damage and health multipliers are reported for your code. The horde spawns enemies and scales the wave budgets itself.
- Horde: enemy classes load when a wave or event starts (synchronously); load large classes in advance. The reachable-path check of spawn points is synchronous and off by default. The tension tags are read at every horde update (0.25 s by default). Not included: endless waves with a growing budget, spawn volumes, removing enemies left far behind, object pooling.
- Downed players are noticed at the next evaluation (0.25 s by default): tags have no change events.
- The demo enemies use proximity instead of AI Perception.
- A map's own navigation mesh set to *Dynamic* builds empty tiles while the project's *Runtime Generation* is *Static*: build the navigation in the editor, as the demo map does, or set the project setting.
- Dedicated servers: tested in the editor on a dedicated server world. The plugin also compiles into the sample project's Server target, built on Unreal Engine 5.8 from source, and two clients join that packaged server in a smoke test. On UE 5.6 and 5.7 the Server target is not compiled, and any Server target needs an engine built from source.

## Integration

How CoopKit AI talks to your project and to other plugins without compile-time links.

### Gameplay Tags

| Tag | Kind | Set by | Meaning |
|---|---|---|---|
| `CK.AI.State.Attacking` | State | The threat component (`IGameplayTagAssetInterface`) | The enemy holds an attack token |
| `CK.AI.Block.Target` | Block | Anyone, on a possible target (the actor or its components) | Enemies do not pick the actor as a target |
| `CK.AI.Block.Attack` | Block | Anyone, on an enemy pawn | The enemy gets no attack token |
| `CK.Revive.State.Downed`, `CK.Revive.State.Dead` | Read by name | CoopKit Revive (or anyone) | The actor is no target (*Contract Untargetable Tag Names*) |
| `CK.Revive.State.Downed`, `CK.Revive.State.BeingRevived`, `CK.Combat.State.Reloading` | Read by name | CoopKit Revive, CoopKit Combat (or anyone) | Default tension tags of a pacing profile: the player's tension rises while the tag is there |
| `CK.Combat.State.OutOfHealth` | Read by name | CoopKit Combat (or anyone) | A horde enemy with the tag no longer counts as alive (*Contract Defeated Tag Names*) |

### Components

| Component | Put on | Replicated | Notes |
|---|---|---|---|
| `UCKAIDirectorComponent` | Game state | No (server only) | Attack tokens, difficulty steps, stimuli; threat components register by themselves |
| `UCKAIThreatComponent` | Enemy pawn | Yes: current target and attack token | Threat table (server), target choice, token requests. Blueprintable: override *Is Valid Target* |
| `UCKAIHordeComponent` | Game state (or an arena actor with *Area Radius*) | Yes: the horde state (phase, tension, wave, enemies, pause) | Tension, pacing, waves and events, spawning out of sight; optional |
| `ACKAISpawnPoint` (actor) | Your level | No | A place where a wave group's enemies may come; *Group*, *Spawn Enabled* |

### Interfaces (optional, implement on your actors)

| Interface | Method | Default when not implemented |
|---|---|---|
| `IGenericTeamAgentInterface` (engine, AIModule) on the AI controller or enemy pawn, and on the player controller, pawn or player state | `GetGenericTeamId`, `GetTeamAttitudeTowards` | No team on either side: players are the enemies' foes |
| `ICKAITensionSource` (CoopKit AI) on the player's pawn, player state or one of their components | `GetTensionPerSecond` | Damage, enemies and tags make the tension |

### Events

| Delegate / Gameplay Event | Fires on (Server / Owning Client / All) | Payload |
|---|---|---|
| `UCKAIThreatComponent::OnTargetChanged` | All | New target, old target |
| `UCKAIThreatComponent::OnAttackTokenChanged` | All | Whether the enemy holds a token |
| `UCKAIDirectorComponent::OnScalingChanged` | Server | Difficulty step, player count |
| `UCKAIHordeComponent::OnHordeStateChanged` | All | None: read `GetHordeState` |
| `UCKAIHordeComponent::OnPacingPhaseChanged` | Server | Phase |
| `UCKAIHordeComponent::OnWaveStarted`, `OnWaveCompleted`, `OnAllWavesCompleted` | Server | Wave number (from 1), enemies of the wave |
| `UCKAIHordeComponent::OnEnemySpawned` | Server | The new enemy, the wave or event name |
| `UCKAIHordeComponent::OnHordeEventStarted` | Server | Event name |

### Replication

| Property / RPC | Condition | Reliable |
|---|---|---|
| `UCKAIThreatComponent::CurrentTarget` | Every client (`COND_None`), RepNotify | Property |
| `UCKAIThreatComponent::bHoldsAttackToken` | Every client (`COND_None`), RepNotify | Property |
| `UCKAIHordeComponent::State` (`FCKAIHordeState`) | Every client (`COND_None`), RepNotify | Property |

The threat table and the director stay on the server. The plugin has no RPCs: players affect enemies through your game's own replicated actions (damage, abilities) on the server.

### Settings, console and API

| Name | Kind | Where |
|---|---|---|
| `UCKAISettings` | Project settings: *Default Director Data*, *Default Profile*, *Contract Untargetable Tag Names*, *Contract Defeated Tag Names*; section `[/Script/CoopKitAI.CKAISettings]` in `DefaultCoopKitAI.ini` | Runtime module `CoopKitAI` |
| Threat component | Blueprint and C++ (server): `AddThreat`, `ReportDamage`, `ClearThreat`, `ClearAllThreat`, `ForceTarget`, `RequestAttackToken`, `ReleaseAttackToken`, `EvaluateNow`, `SetProfile`; any machine: `GetCurrentTarget`, `HoldsAttackToken`; server: `GetThreat`, `GetThreatEntries`, `CanGetAttackToken` | Runtime module |
| Director component | Blueprint and C++ (server): `ReportStimulus`, `SetPlayerCountOverride`, `SetData`, `GetScaling`, `GetPlayerCount`, `GetAttackerCount`, `GetTotalAttackers` | Runtime module |
| Horde component | Blueprint and C++ (server): `StartWaves`, `StopWaves`, `StartNextWaveNow`, `StartEvent`, `StopEvent`, `ReportPlayerDamage`, `AddTension`, `ReportEnemyDefeated`, `SetPacingData`, `SetAreaRadius`, `GetPlayerTension`, `GetSpawnedEnemies`, `GetLastSpawnProblem`; any machine: `FindHorde`, `GetHordeState`, `GetPhase`, `GetTension`, `GetWaveNumber`, `GetWaveCount`, `GetWaveStage`, `GetAliveEnemies`, `GetPendingEnemies`, `GetPauseSecondsLeft` | Runtime module |
| `UCKAIEditorLibrary::CreateExampleStateTree` | Editor function (Blueprint, Python): creates or rebuilds the example enemy StateTree asset | Editor module `CoopKitAIEditor` |
| `ACKAIDemoHUD` | HUD class of the demo game mode: the panel of buttons and the mouse cursor for the local player; set it as the HUD Class of your own test map's game mode to get the same panel | Runtime module |
| `ck.ai.AddThreat [amount]` | Console command, server: the enemy nearest to the host gains threat towards the host | Any build |
| `ck.ai.SetPlayerCount [count]` | Console command, server: difficulty for this many players (0: the real number) | Any build |
| `ck.ai.Tokens` | Console command, server: prints targets and attack tokens | Any build |
| `ck.ai.Debug` | Console variable, 0/1: the server draws targets, top threats and tokens (debug draw is compiled out of Shipping) | Any build |
| `LogCKAI` | Log category | |

### CoopKit Hub

| Name | Kind | Where |
|---|---|---|
| Entity type **Threat Profile** | `UCKAIProfileData` with the class metadata `CoopKitEntity="Threat Profile"`, `CoopKitEntityFolder="ThreatProfiles"` | Runtime module (metadata exists in the editor only) |
| Entity type **Director Rules** | `UCKAIDirectorData` with the class metadata `CoopKitEntity="Director Rules"`, `CoopKitEntityFolder="DirectorRules"` | Runtime module (metadata exists in the editor only) |
| Entity type **Pacing Profile** | `UCKAIPacingData` with the class metadata `CoopKitEntity="Pacing Profile"`, `CoopKitEntityFolder="PacingProfiles"` | Runtime module (metadata exists in the editor only) |
| Entity type **Wave Set** | `UCKAIWaveData` with the class metadata `CoopKitEntity="Wave Set"`, `CoopKitEntityFolder="WaveSets"`; part *Pacing* (`CoopKitPart="Pacing"`, *Create* in the card) | Runtime module (metadata exists in the editor only) |
| Hub contract version | Marker class `UCKAIHubInfo` with `CoopKitHubVersion="2"`. If several CoopKit plugins are installed, the one with the highest hub version shows the hub window. | Editor module `CoopKitAIEditor` |
| Folder settings | `[CoopKit.Workspace]` in `Config/DefaultEditor.ini`: `EntityPathTemplate`, `EntityPathTemplate.Threat Profile`, `EntityPathTemplate.Director Rules`, `EntityPathTemplate.Pacing Profile`, `EntityPathTemplate.Wave Set`, `SharedPathTemplate` | Your project |
| *Install AI Agent Skills* | Hub window button and *Tools > CoopKit* menu entry | Editor module |

### Engine plugin dependencies

| Plugin | Required / Optional | Stability (Beta / Experimental) |
|---|---|---|
| StateTree | Required (enabled by the plugin) | Stable |
| GameplayStateTree | Required (enabled by the plugin) | Stable |

## Changelog

See `CHANGELOG.md` next to `CoopKitAI.uplugin` and the Changelog section of the Fab listing.
