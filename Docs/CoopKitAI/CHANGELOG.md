# Changelog: CoopKitAI

All notable changes to CoopKitAI are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitAI.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

This version will be 1.1.0: it adds features and keeps the integration contract.

### Added

- Horde director: the component **CKAI Horde Component** on the game state, next to the AI director. It keeps a tension for every player (damage, enemies after the player, contract tags such as CoopKit Revive's downed state, and your own **CK AI Tension Source**), and paces the fight in phases: build up, peak, peak fade and a break without new enemies.
- Waves and horde events: a wave set runs waves in order with pauses, and named horde events your code starts on top of them (an alarm, a final rush, a group every few seconds). A wave's budget grows with the AI director's spawn budget multiplier for the counted players. Fixed enemies come first, the rest of the budget is spent by fill weight, and some enemies come only from a number of players. Enemies come in groups, with a limit of enemies alive at once.
- Spawning out of sight: enemies come at **CK AI Spawn Point** actors of the wave's group, or around the players on the navigation mesh. Points a player sees (in the view cone, not behind cover) and points too close to a player are skipped. When every point is seen the enemies wait, and the horde says why.
- Pacing profiles and wave sets are data assets with their own editor (Summary and Validation tabs). The CoopKit Hub lists them under **Pacing Profile** and **Wave Set**, with cards and the **New Pacing Profile** and **New Wave Set** wizards. A wave set's card creates its pacing profile in the **Parts** tab.
- Every client sees the horde state: phase, tension, wave, enemies alive and to come, and the pause before the next wave.
- Project setting *Contract Defeated Tag Names*: a horde enemy with such a tag no longer counts as alive (by default CoopKit Combat's out of health).
- Demo: the panel of the demo map has **Start the waves**, **Send a horde now** and **Stop the waves**, and a line with the wave and the pacing in every window. The overview map gets a fifth stand, **Pacing and waves**.
- Automation tests for the horde: pacing and wave rules, spawning out of sight, replication with packet lag and loss on listen and dedicated servers for one to four players, the demo buttons, the overview stand and the new data editors.
- Threat profiles and director rules in the CoopKit Hub (*Tools > CoopKit Hub*, categories **Threat Profile** and **Director Rules**), next to the types of other installed CoopKit plugins.
- Threat profile and director rules cards: Properties, Preview, Parts, Summary and Validation. Neither type uses another asset, so Parts is empty and says so, and Preview has no picture.
- New Threat Profile and New Director Rules wizards: pick an asset to start from, or the plugin's defaults. New assets go to `/Game/CoopKit/ThreatProfiles/<Name>/` and `/Game/CoopKit/DirectorRules/<Name>/`.
- Plugin functions to rename, duplicate and delete threat profiles and director rules. The hub has no buttons for them.
- AI agent skill `coopkit-ai` for Claude Code, Antigravity, Codex and other agents. From your own C++ module it adds a targeting rule, a reaction to an enemy's target or attack token, or difficulty by player count. It then builds the Editor target and runs the tests. If the build cannot run, the agent tells you what to install. Install the skill with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKAIAgentLibrary` (`unreal.CKAIAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Creating a threat profile or director rules in the Content Browser (*Add > CoopKit > AI Threat Profile* / *AI Director Data*) now opens the wizard first.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- The demo enemy `BP_CKAIDemoEnemy` now runs its StateTree when you create it with Spawn Actor during play. Its controller began play before it possessed the pawn. On newer engine versions the tree then started without a pawn, logged "Could not find context actor of type Pawn" and did not run. `BP_CKAIDemoController` no longer starts its StateTree AI Component at Begin Play. The controller starts it when it possesses the pawn.
- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- Threat component (enemy pawn): threat table from damage (including engine damage), AI Perception, proximity, stimuli and Add Threat; decay and forgetting; target choice with a switch threshold; taunts; replicated current target and attack token.
- AI director (game state): attack tokens with limits per target and per match, hold time and cooldown; difficulty steps by player count with damage, health and spawn budget multipliers for your code; stimuli for nearby enemies.
- Downed players stop being targets: untargetable tags read from any actor, CoopKit Revive's downed and dead states by name, no plugin dependency.
- Gameplay Tags `CK.AI.State.Attacking`, `CK.AI.Block.Target`, `CK.AI.Block.Attack`.
- StateTree tasks (Threat Target, Hold Attack Token, Move To Threat Target, Wait) and conditions (Has Threat Target, Attack Token Available, Actor Has Tag); example enemy StateTree and an editor function that builds it.
- EQS tests (Threat, Attack Slots, Actor Tags) and the Threat Target context.
- Data assets with their own editor: AI Threat Profile and AI Director Data (Summary and Validation tabs); defaults ship with the plugin.
- Ready-to-play demo map with enemies on StateTree and a navigation mesh, and a panel of buttons, so the whole demo runs with mouse clicks: send every enemy at a player, calm them, count four players for a harder step. With one Play In Editor window, the panel restarts Play as a listen server with two windows.
- Console commands `ck.ai.AddThreat`, `ck.ai.SetPlayerCount`, `ck.ai.Tokens` and the server debug draw `ck.ai.Debug` for developers.
- Automation tests for threat, attack tokens, StateTree, EQS, the demo and its panel, replication under lag and loss (dedicated and listen server, join in progress), the pure rules and the data editors.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
