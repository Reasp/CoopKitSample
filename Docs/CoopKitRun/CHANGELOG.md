# Changelog: CoopKitRun

All notable changes to CoopKitRun are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitRun.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

## [1.0.0] - 2026-11-21

### Added

- First version of CoopKitRun: the run structure of a co-op roguelite.
- Run director (`UCKRunComponent`, on `ACKRunDirector` or your game state): stages and rooms, an upgrade choice after a room, a path vote, travel to the next room or a server travel to the room type's map, victory, defeat and abandon. Players act through `UCKRunPlayerComponent`; every request is checked on the server and refused with a reason tag `CK.Run.Reject.*`.
- One run seed for every machine: room seeds, offers, doors and ties derived from it (PCG32 and SplitMix64, tested against fixed control values). Seed codes of 8 characters and the seed of the day. `UCKRunRandomLibrary` for level generators in Blueprint.
- Hidden picks: each player's cards reach only that player; the picks show when everyone has chosen. Rerolls, a time limit with an auto pick, team upgrades.
- Join in progress and return: the team's upgrades and catch-up picks for a newcomer; a player who leaves does not hold the team back.
- Modifiers (`UCKRunModifierData`): chosen at the start, set per stage or drawn from the seed; value multipliers and run tags.
- Meta progress (`UCKRunMetaData`, `UCKRunMetaComponent`): currency from runs, nodes with bonus rerolls, bonus cards, starting upgrades and locked upgrades and modifiers. Kept per local player (`UCKRunLocalMetaStore`), by the server, or by your store through `ICKRunMetaStore`; the server cleans what a client sends.
- Upgrades on characters: a Gameplay Effect per upgrade (level = stacks) on the player's Ability System Component, or `ICKRunUpgradeReceiver`.
- Saves: `PrepareForSave` and `RestoreAfterLoad` keep a run, the phase's time included; a snapshot carries the run across a server travel.
- Rules as instanced objects of the run: path (Doors, Linear), offer (Weighted by Rarity), auto pick (Highest Rarity); subclass them in your project.
- Demo map `L_RunDemo`, played by mouse: a run with cards, doors, modifiers and meta nodes, alone or in two windows. Overview map `L_RunOverview` with five stands.
- Editor: Run, Upgrade Pool, Modifier and Meta Track in the CoopKit Hub with wizards and cards: Stages, Simulate (a whole run from a seed without Play), Odds and Unlocks tabs, and validation that names the field.
- Editor surface for AI agents, run operations: `UCKRunAgentOps` simulates a run, previews an offer, describes a meta track and validates a run with what it uses, in JSON.
- Editor: data types with `CoopKitEntity` metadata appear in the CoopKit Hub with a card (Preview and Parts tabs), a "New" wizard and a project check.
- Editor: workspace functions rename, duplicate and delete an entity together with its own parts. Shared parts are not touched.
- AI agent skill `coopkit-run` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project with a script that names what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKRunAgentLibrary` (`unreal.CKRunAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64). Requires the engine plugin Gameplay Abilities.
