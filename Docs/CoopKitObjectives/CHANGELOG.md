# Changelog: CoopKitObjectives

All notable changes to CoopKitObjectives are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitObjectives.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Editor surface for AI agents: `UCKObjectivesAgentLibrary` (`unreal.CKObjectivesAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

## [1.0.0] - 2026-11-20

### Added

- First version of CoopKit Objectives: quests and objectives that a party of a co-op game plays together.
- Quest log on the server for the whole party, replicated to every player and to players who join later.
- Steps of objectives that count gameplay events by Gameplay Tag (a tag under the objective's tag counts too): shared objectives of the party, every-player objectives (each player in the game reaches the count) and any-player objectives (the first player who reaches it), in the same step.
- Branches (the objective that completes an any-objective step chooses the next step), objectives that fail the quest, step time limits that fail the quest or complete the step, shared counts that grow with the number of players, required quests, repeatable quests.
- Start votes: a share of the players votes yes in time, and the players who voted yes play the quest.
- Contributions per player; rewards to everyone, to contributors or to the top contributor through `ICKObjectivesRewardReceiver`; a reward for a player who is away waits until the player comes back. A simple reward bag component for games without an inventory.
- Refusals of the server with a reason: `ECKObjectivesResult` and a `CK.Objectives.Reject.*` Gameplay Tag (already active, already completed, missing prerequisite, not active, not voting, already voted, not your quest, rate limited and others). Ready event tags `CK.Objectives.Event.Kill`, `.Collect`, `.Reach`, `.Interact`.
- Player component on the player controller: *Request Start*, *Request Vote*, *Request Abandon*, with *On Request Result* on the server and the owning client and a limit of requests per second. Server API on the quest log: *Report Event*, *Complete Objective*, *Set Objective Target*, *Fail Quest* and more.
- Area actor that reports players who reach a place.
- Quest data asset with validation, the **New Quest** wizard, a card in the CoopKit Hub with a Steps tab that lists the steps with their branches, and Summary and Validation tabs.
- Saving: the quests, counts, contributions and pending rewards are SaveGame properties; *Prepare For Save* and *Restore After Load* keep votes and time limits.
- Demo map `L_ObjectivesDemo` with a panel of buttons (start, vote, abandon, four gameplay stand-ins, Play with 2 windows) and overview map `L_ObjectivesOverview` with four stands: a shared count with a medal for the top contributor, an every-player objective, both branches, refusals with a reason.
- Automation tests on listen and dedicated servers at 150 ms lag and 2% loss: shared and every-player objectives, contributions, a branch, refusals, rewards, a vote, a player who joins in progress.
- AI agent skill `coopkit-objectives` for Claude Code, Antigravity, Codex and other agents. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
