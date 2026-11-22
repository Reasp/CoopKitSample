# Changelog: CoopKitSave

All notable changes to CoopKitSave are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitSave.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Save profiles in the CoopKit Hub (*Tools > CoopKit Hub*, category **Save Profile**), next to the entities of other installed CoopKit plugins.
- Save profile card with Preview and Parts tabs. A save profile has no parts, and both tabs say so.
- New Save Profile wizard: a profile to start from and a slot prefix.
- Plugin functions that rename, duplicate and delete save profiles. The hub has no buttons for these yet.
- AI agent skill `coopkit-save` for Claude Code, Antigravity, Codex and other agents: saves a new value of your game, keeps old saves working after a change, matches players by your own key, builds the Editor target with a script that names what to install when the build cannot run, and runs the tests. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- **Capture To Bytes** and **Load From Bytes** on the save subsystem (server only). They save the world and the players to an array of bytes with no slot and no disk. Load From Bytes runs the same checks as Load From Slot. The bytes use the slot file format.
- Editor surface for AI agents: `UCKSaveAgentLibrary` (`unreal.CKSaveAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Save profiles made in the Content Browser open the New Save Profile wizard first.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains how to test data validation and what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- Save subsystem (game instance): save, load and delete slots on the host, asynchronous disk work through the platform save system, slot list with metadata, slot limit, autosave by timer and when a player leaves, loading a save of another map by travelling there first.
- World capture and apply: the game state, actors placed on the map (moved, changed, destroyed) and actors spawned during play (spawned again on load), streaming sub-levels.
- Player records by key (unique net id, player name, `ICKSavePlayerIdentity` or a C++ resolver): restored on load, when a player joins a running game and when a player reconnects; records of absent players kept from save to save.
- Saveable component and optional interface `ICKSaveable` (On Before Save, On After Load).
- Format version with upgrade steps, Game Data Version of the game with the On Migrate Save event and record edits.
- Gameplay Tags `CK.Save.Block.Save` and `CK.Save.Block.Load`.
- Save Profile data asset with its own editor (Summary and Validation tabs); project settings with the default profile.
- Blueprint latent nodes Save Game To Slot (CoopKit) and Load Game From Slot (CoopKit).
- Ready-to-play demo map with a panel of buttons, so the whole demo runs with mouse clicks: save the game, move every player and crate, load the slot and see everyone back. With one Play In Editor window, the panel restarts Play as a listen server with two windows.
- Console commands `ck.save.Save`, `Load`, `Delete`, `List`, `Dump` and the debug overlay `ck.save.Debug` for developers.
- Automation tests: save, load and join in progress under lag and loss (dedicated and listen server), world save and load, format and game data migration, data editor, the demo panel.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
