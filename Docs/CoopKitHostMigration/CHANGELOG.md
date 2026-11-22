# Changelog: CoopKit Host Migration

All notable changes to CoopKitHostMigration are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitHostMigration.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- First version of CoopKit Host Migration: a listen-server group keeps playing when its host leaves or crashes. The first player in the successor order hosts the same map from the last snapshot, and the other players join it.
- Snapshots sent to every client in pieces within a bandwidth budget, checked by length and SHA-1 hash before a client keeps them. A broken transfer keeps the previous snapshot.
- A pulse from the host: a crashed or frozen host is noticed after the profile's Pulse Timeout Seconds instead of the engine's connection timeout.
- A successor order in every snapshot (best ping, then join order), fallback hosting by the next player in the order, and a hand-over when two players host the group at once.
- Repeated migrations, and Rejoin Group for a player who left.
- Host Migration Profile data asset with a card, a "New" wizard and validation; it appears in the CoopKit Hub.
- Light snapshot (SaveGame properties of the game state, players, pawns and tracked actors) and a session driver on the Online Subsystem, both replaceable through the `ICKHostMigrationSnapshotSource` and `ICKHostMigrationSessionDriver` interfaces.
- Demo map `L_HostMigrationDemo` played with the mouse in three windows: host, join, push the crates, leave or simulate a crash, rejoin.
- AI agent skill `coopkit-hostmigration` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project with a script that names what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKHostMigrationAgentLibrary` (`unreal.CKHostMigrationAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
