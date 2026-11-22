# Changelog: CoopKitNetTest

All notable changes to CoopKitNetTest are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitNetTest.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- AI agent skill `coopkit-nettest`. It adds a network preset to your project, starts a test session from your own code, reads network stats, writes an automated multiplayer test, builds with `scripts/build.ps1` and runs the tests. Install it with *Tools > CoopKit > Install AI Agent Skills*.

### Changed

- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains how to test data validation and what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- Network emulation presets for Play In Editor (Off, Good, Average, Bad, Terrible) stored as project data in `DefaultCoopKitNetTest.ini`, editable in Project Settings.
- One-click multiplayer sessions from the Play toolbar (**Net Play** and the **Net** menu): dedicated or listen server, 1-8 clients, one process or separate processes.
- Console commands `ck.nettest.Preset` and `ck.nettest.Launch`, editor subsystem `CKNetTestEditorSubsystem` for Editor Utility Blueprints and Python.
- Stats overlay (`ck.nettest.Overlay`): ping, packet loss and traffic of every connection on each Play viewport; Blueprint library `CKNetTestStatsLibrary` with the same data. A server lists its clients as *Client 1*, *Client 2*, ... in connection order.
- Demo level `L_NetTestDemo` with a panel of buttons: one button per preset restarts Play as a listen server and a client under that preset, and the panel shows the ping and packet loss of each connection and switches the overlay.
- Editor automation tests `CoopKit.NetTest.*`, including the demo panel.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
