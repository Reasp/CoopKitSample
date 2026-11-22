# Changelog: CoopKitRevive

All notable changes to CoopKitRevive are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitRevive.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Revive rules in the CoopKit Hub (*Tools > CoopKit Hub*, category **Revive Rules**), next to the entities of other installed CoopKit plugins. The hub opens on categories. Click one to list its entities, and click **< Back** to return. The right side shows the selected entity with **Open**, **Where Used** and **Check Project**.
- Revive rules card: Preview and Parts tabs in the data editor. The Parts tab shows the body data the rules give to the bodies the match adds, with Open, Replace and Create.
- New Revive Rules wizard: rules to start from, and an option to make the rules' own body data in their folder. The values come from a body data you pick. Or you can let the rules use a body data you pick where it is.
- Plugin functions that rename, duplicate and delete revive rules together with their own body data. Body data outside the rules' folder is never moved or deleted. The hub has no buttons for these yet.
- **Restore Downed** on the body component (server, Blueprint and C++): puts a body down with the down count and bleed-out seconds you recorded with **Get Down Count** and **Get Bleed Out Seconds Left**, for a save or a host migration. It skips the down rules, because the body was already down when you recorded it. A running revive stops, so a teammate starts it again. It returns false on a client and on a dead body (call **Reset To Alive** first).
- AI agent skill `coopkit-revive` for Claude Code, Antigravity, Codex and other agents: changes the down, revive and respawn behavior from your C++ module, builds the Editor target with a script that names what to install when the build cannot run, and runs the tests. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKReviveAgentLibrary` (`unreal.CKReviveAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Revive rules made in the Content Browser open the New Revive Rules wizard first. By default it also makes their own body data next to them.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- Body component (pawn): down, crawl, bleed-out, revive by a teammate, give up, death; limits on downs; Character Movement crawl speed.
- Player component (player state): death camera, spectating teammates, respawn by timer, at the next checkpoint or never, respawns per player.
- Match component (game state): rules, checkpoints, respawns through the game mode, team wipe (event only or respawn everyone at the checkpoint), deaths remembered across a reconnect, automatic components for every player.
- Controller component (player controller): revive, give up and spectate actions relayed to the server.
- Placeable checkpoint actor with an activation volume and spawn points.
- Optional interface `ICKRevivable`: whether a pawn may go down and who may revive it.
- Gameplay Tags `CK.Revive.State.*`, `CK.Revive.Event.*` and `CK.Revive.Block.*`; with an Ability System Component the state tags are replicated loose tags and the events are Gameplay Events.
- Data assets with their own editor: Revive Body Data and Revive Rules (Summary and Validation tabs).
- Console commands `ck.revive.Down`, `Kill`, `Revive`, `Respawn` and the debug overlay `ck.revive.Debug`.
- Ready-to-play demo map with a panel of buttons, so the whole demo runs with mouse clicks: put a player down, go to them and revive them, give up, kill and respawn. With one Play In Editor window, a button on the panel restarts Play as a listen server with two windows.
- Automation tests for replication under lag and loss (dedicated and listen server, join in progress), the Gameplay Ability System path, rules, data editors and the demo panel.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
