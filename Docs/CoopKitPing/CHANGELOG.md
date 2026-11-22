# Changelog: CoopKitPing

All notable changes to CoopKitPing are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitPing.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Ping rules in the CoopKit Hub (*Tools > CoopKit Hub*, category **Ping Rules**), next to the entities of other installed CoopKit plugins.
- Ping rules card: Preview and Parts tabs in the data editor. The Parts tab shows the quick ping type and every wheel sector, with Open, Replace and Create. Replace in the empty row adds a sector. Replace with none removes one.
- New Ping Rules wizard: pick the rules to start from, and choose whether the new rules get their own copies of those rules' ping types in their folder. If you turn that off, the new rules use the ping types of the rules to start from, where they are.
- Plugin functions that rename, duplicate and delete ping rules together with their own ping types. Ping types outside the rules' folder are never moved or deleted. The hub has no buttons for these yet.
- AI agent skill `coopkit-ping` for Claude Code, Antigravity, Codex and other agents: changes the ping behavior from your C++ module, builds the Editor target with a script that names what to install when the build cannot run, and runs the tests. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKPingAgentLibrary` (`unreal.CKPingAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Ping rules made in the Content Browser open the New Ping Rules wizard first. By default they start from the project's default rules and get their own copies of its ping types. Before, new rules started with an empty wheel.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- Match component (game state): live pings, server checks (type, distance, block tag, rate limit), visibility by team, automatic controller components for every player.
- Controller component (player controller): ping requests relayed to the server, the player's visible pings replicated to the owner only, ping sound, world marker actors, HUD layer.
- Input component (local player controller): ping key and command wheel on Enhanced Input (tap: quick ping; hold: wheel with mouse or right stick; release: command; center: cancel).
- CommonUI widgets built in C++: HUD layer, marker (icon, color, distance, edge arrow), wheel.
- Gameplay Tag `CK.Ping.Block.Ping`.
- Data assets with their own editor: Ping Type and Ping Rules (Summary and Validation tabs); default rules with four ping types and input assets ship with the plugin.
- Default input assets are cooked with the plugin, so a packaged game has the ping key and the command wheel with no setup.
- Five settings under Project Settings > Plugins > CoopKit Ping > Input for the default input assets of new input components: ping key action, wheel pointer action, wheel stick action, ping mapping context and wheel mapping context.
- The input component logs one line per local player with the input assets it loaded.
- The input component warns when a set input asset cannot be loaded. In a packaged game this means it was not cooked.
- Ready-to-play demo map with a panel of buttons, so the whole demo runs with mouse clicks: pick a ping type, ping an object of the map or what you look at, clear your pings. With one Play In Editor window, the panel restarts Play as a listen server with two windows.
- Console commands `ck.ping.Ping`, `ck.ping.Clear` and the server debug draw `ck.ping.Debug` for developers.
- Automation tests for replication by team under lag and loss (dedicated and listen server, join in progress), the server rate limit, wheel and marker math, data editors and the demo panel.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64). Depends on CommonUI (Beta in UE 5.6 and 5.7).
