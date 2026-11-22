# Changelog: CoopKitInteract

All notable changes to CoopKitInteract are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitInteract.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Editor surface for AI agents: `UCKInteractAgentLibrary` (`unreal.CKInteractAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- CoopKit Hub: the single table of all entities is replaced by categories. The window opens with one button per entity type of every installed CoopKit plugin, showing the number of its entities. Click a category to list only its entities, with search and **New**. Click **< Back** to return to the categories. The right side shows the selected entity with **Open**, **Where Used** and **Check Project**. The card still opens in its own window. If several CoopKit plugins are installed, the plugin with the newest hub shows the tab, so this layout appears as soon as one of them has it.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- Card: an entity with no parts now says so in the Preview and Parts tabs, instead of asking you to pick a part in an empty Parts tab.
- Card: a part in the entity's folder that another asset also uses is now marked "shared" in the Parts tab, as the warning above the list says, instead of "own".
- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains how to test data validation and what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-17

### Added

- First version of CoopKit Interact: co-op interactions checked by the server.
- Interactable component: participants with a lock (*Max Players*), Instant or Hold, actions that need several players at once, extra speed per extra player, progress kept or reset on release, cooldown and single use. Progress runs on the server clock and replicates as a start time and a rate.
- Interactor component on the player controller: focus and a prompt as data for your UI, *Start Interact* and *Stop Interact* over reliable RPCs, the local press shown before the server answers, and the server's answer with a `CK.Interact.*` reason tag.
- Server checks in a fixed order: disabled or no data, single use, cooldown, a player busy elsewhere, the lock, range, required tags, blocked tags and `CK.Interact.Block.Interact`, line of sight, then your own rule through the `ICKInteractable` interface. A player who walks past the break distance or gets a blocked tag stops.
- Interaction data asset with its own editor: Summary and Validation tabs. Six demo interactions, a demo map and an overview map with four stands played by bots.
- The demo map is ready to play with the mouse: a panel of buttons lists the six devices with Go to, Hold, Use and Let go, and shows the server's refusals. Go to stops 2 m from the device, and the label over each device is small and turns to face the camera. Players are named *Player 1*, *Player 2* in join order. With one Play In Editor window, a button on the panel restarts Play as a listen server with two windows for the door that needs two players.
- Editor: the interaction type appears in the CoopKit Hub with a card (**Preview** and **Parts** tabs), a **New Interaction** wizard and a project check. After **New**, the hub selects the new interaction and shows where it was created.
- Editor: workspace functions rename, duplicate and delete an entity together with its own parts. Shared parts are not touched.
- AI agent skill `coopkit-interact` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project with a script that names what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).

