# Changelog: CoopKitHeroes

All notable changes to CoopKitHeroes are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitHeroes.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Editor surface for AI agents: `UCKHeroesAgentLibrary` (`unreal.CKHeroesAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- CoopKit Hub: the single table of all entities is replaced by categories. The window opens with one button per entity type of every installed CoopKit plugin, showing the number of its entities. Click a category to list only its entities, with search and **New**. Click **< Back** to return to the categories. The right side shows the selected entity with **Open**, **Where Used** and **Check Project**. The card still opens in its own window. If several CoopKit plugins are installed, the plugin with the newest hub shows the tab, so this layout appears as soon as one of them has it.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- Card: a part in the entity's folder that another asset also uses is now marked "shared" in the Parts tab, as the warning above the list says, instead of "own".
- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-17

### Added

- First version of CoopKitHeroes.
- Hero data asset: id, name, description, role tag, pawn class, mesh, Animation Blueprint, own materials by slot, icon, portrait, skins, abilities, effects, granted tags and hero modules.
- Hero Roster data asset: the heroes players may choose, a hero once per team or per match, a default hero, switching while the pawn lives on or off.
- Hero component for the player state: the server checks every choice, with reject reasons as Gameplay Tags (`CK.Heroes.Reject.*`) and an optional selection gate on your game mode or game state.
- The hero is applied to every pawn the player gets: abilities, effects and tags on the server, the look on every machine, players who join a running game included. For seamless travel, call `CopyTo` from your player state's `CopyProperties` to keep the choice.
- Hero modules (C++ or Blueprint) for the systems of your game, such as a starting inventory or revive settings.
- Hero workspace in the editor: the hero card with a 3D preview and every part of the hero, the New Hero wizard, the Hero Roster window (rename, duplicate and delete a hero with its own parts), Hero Comparison, CSV export and import, and the CoopKit Hub. After **New**, the hub selects the new hero in the list and shows where it was created.
- Demo map `L_HeroesDemo`, Overview map `L_HeroesOverview` and four demo heroes.
- The demo map is ready to play with the mouse: a panel of buttons lists the roster's heroes with Play and a skin, the players (*Player 1*, *Player 2* in join order) with their heroes and a Look at button, and the server's refusals. The demo pawn has a third-person camera behind and to the left of the character. Look at turns the view so the other player's pawn is in the middle of it. With one Play In Editor window, a button on the panel restarts Play as a listen server with two windows.
- Console command `ck.heroes.Select <HeroId> [SkinId]`.
- AI agent skill `coopkit-heroes` in `Extras/AgentSkills/coopkit_heroes`. It extends heroes from your own C++ module, adds validation and an automation test, builds the Editor target, runs the tests, and names what to install when the build cannot run. Works with Claude Code and with agents that read `.agents/skills`. Install it from Tools > CoopKit > Install AI Agent Skills or the Hub button, or copy the folder by hand.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).

