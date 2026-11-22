# Changelog: CoopKitVoice

All notable changes to CoopKitVoice are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitVoice.uplugin`, and the notes of a released version are pasted into
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

- First version of CoopKitVoice: proximity voice chat on the engine's own voice, routed by the server.
- Voice Profile data asset: falloff, walls, talk mode, radio, team and ghost channels, rules by Gameplay Tags. Presets Coop Horror, Team Shooter and Open Friendslop in the New Voice Profile wizard; route preview in the card.
- The server mutes every voice a listener cannot hear with the engine's own filter, with hysteresis at the edge of a range.
- Clients place each voice at its source, make it quieter and muffled behind walls, and play radios in the head.
- Test tone: talk with a tone instead of a microphone to check voice without a second person.
- Project check and fix of the engine's voice settings.
- Demo map with a panel of buttons: talk, test tone, radio, state, team, mute.
- Editor: data types with `CoopKitEntity` metadata appear in the CoopKit Hub with a card (Preview and Parts tabs), a "New" wizard and a project check.
- Editor: workspace functions rename, duplicate and delete an entity together with its own parts. Shared parts are not touched.
- AI agent skill `coopkit-voice` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project with a script that names what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKVoiceAgentLibrary` (`unreal.CKVoiceAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
