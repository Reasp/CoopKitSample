# Changelog: CoopKitInventory

All notable changes to CoopKitInventory are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitInventory.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Items in the CoopKit Hub (*Tools > CoopKit Hub*, category **Inventory Item**), next to the types of other installed CoopKit plugins. Inventory rules stay in the Content Browser.
- Inventory item card: Properties, Preview, Parts, Summary and Validation. Parts lists the icon, the world mesh and every granted ability and effect as its Blueprint, with Open and Replace. Replace lists only Gameplay Ability or Gameplay Effect Blueprints. An empty slot after the last grant adds one, and Create makes a new ability or effect Blueprint in the item's folder.
- New Inventory Item wizard: pick an item to start from, a display name, max stack, equip slot, icon and world mesh. It can also create an ability Blueprint that the item grants while equipped. New items go to `/Game/CoopKit/InventoryItems/<Name>/`.
- Plugin functions to rename, duplicate and delete items together with their own ability and effect Blueprints. The hub has no buttons for them.
- AI agent skill `coopkit-inventory` for Claude Code, Antigravity, Codex and other agents. From your own C++ module it adds an item field, a rule the server checks before a pick-up, drop, give or equip, what using an item does, or an inventory UI. It then builds the Editor target and runs the tests. If the build cannot run, the agent tells you what to install. Install the skill with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKInventoryAgentLibrary` (`unreal.CKInventoryAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Creating an item in the Content Browser (*Add > CoopKit > Inventory Item*) now opens the New Inventory Item wizard first.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- First version of CoopKit Inventory: `UCKInventoryComponent` with a bag and equipment slots in one replicated FastArray, server-validated requests (pick up, drop, give, equip, unequip, use) with reason codes and a request rate limit, `ACKInventoryPickup` for items in the world.
- Item and inventory rules data assets (`UCKInventoryItemData`, `UCKInventoryRulesData`) with the CoopKit Inventory data editor (Properties, Summary, Validation).
- Equipment grants Gameplay Ability System abilities, effects and tags when the owner has an Ability System Component (player state or pawn) and moves them to a new pawn after a respawn.
- SaveGame support: the item list survives a SaveGame round trip (CoopKit Save or your own save code); equipment grants come back after a load.
- Gameplay Tags `CK.Inventory.State.*`, `CK.Inventory.Slot.*`, `CK.Inventory.Event.*`, `CK.Inventory.Block.*`.
- Console commands `ck.inventory.*`, debug overlay `ck.inventory.Debug 1`.
- Ready-to-play demo map `L_InventoryDemo` with a panel of buttons, so the whole demo runs with mouse clicks: go to an item and pick it up, equip, use, drop, go to a teammate and give. The panel shows the server's refusals. With one Play In Editor window, a button on the panel restarts Play as a listen server with two windows.
- Automation tests `CoopKit.Inventory.*`: replication on listen and dedicated servers under 150 ms lag and 2% loss, cheat requests, GAS equipment, save round trip, rules, data editors, the demo panel.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
