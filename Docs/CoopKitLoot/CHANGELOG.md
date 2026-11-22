# Changelog: CoopKitLoot

All notable changes to CoopKitLoot are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitLoot.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Editor surface for AI agents: `UCKLootAgentLibrary` (`unreal.CKLootAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains how to test data validation and what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-17

### Added

- First version of CoopKit Loot: loot shared between co-op players, decided by the server.
- Loot tables as data assets (`UCKLootTableData`): items with weights, rarity, counts and guaranteed items; rolls per player or a rolls curve; a guaranteed rarity in every drop.
- Four ways to share: personal loot (each player rolls alone; other players' drops never reach a client), free for all (if two players grab one item at once, one gets it and the other gets `CK.Loot.Reject.Taken`), round robin, and Need or Greed with a time limit on the server clock.
- The loot subsystem (`UCKLootSubsystem`) rolls on the server with its own random stream, which you can seed. It checks every pickup and roll choice and answers with `CK.Loot.Reject.*` tags.
- `ICKLootReceiver`: your inventory takes picked-up items and says how many units fit; the rest stays on the floor.
- Demo map `L_LootDemo` played with the mouse on a panel of buttons, and overview map `L_LootOverview` with bot players on four stands.
- Editor: the loot table appears in the CoopKit Hub with a card (Summary with the chance of each item, Validation, Preview and Parts tabs), a "New Loot Table" wizard and a project check.
- Editor: the plugin can rename, duplicate and delete a loot table together with its own parts (C++ functions; the CoopKit Hub does not call them yet). Shared parts are not touched.
- AI agent skill `coopkit-loot` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project with a script that names what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Automation tests of replication on a listen server and a dedicated server at 150 ms lag and 2% packet loss.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
