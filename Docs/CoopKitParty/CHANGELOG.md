# Changelog: CoopKitParty

All notable changes to CoopKitParty are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitParty.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- First version of CoopKitParty: a party that forms in the menu, on the Online Subsystem (v1) and its online beacons.
- Party Subsystem (Game Instance): create a party, find open parties, join, leave; members, leader, Ready marks, chat with ready phrases and short texts, kick, ban, promote, party attributes from the leader, Gameplay Tags `CK.Party.State.*` and `CK.Party.Reject.*`.
- Member attributes: each member, the leader too, sets its own keys and values with Set Member Attribute, for example a platform id for a group matchmaking ticket or a chosen hero. The leader checks the key, the size and the rate, then sends them to everyone in the party list. Get Member Attribute reads them, and On Member Attribute Changed reports each change. They stay through leader changes and map changes and go away when the member leaves.
- The leader pulses its members. When the leader leaves or goes silent, the next member by join order leads and the others come back by themselves; a report gives the times.
- The party survives map changes: the leader's beacon comes back after each map load in a standalone world, and the party waits without a beacon in a networked world.
- The leader takes the party into a match: Host Game hosts one, Find Games and Join Game join someone else's. Every member must fit: otherwise the leader is refused before anyone travels. The members follow by a ticket the leader sends before it travels. After the match the party is back together in the menu with the same leader and cleared Ready marks. A member that cannot follow stays in the party and hears On Game Travel with Follow Failed.
- Friends and invites: Read Friends (a list with presence), Invite Friend, Show Invite UI (the Steam overlay) and Join Friend Party. They need Steam or EOS; on Null they return Not Supported. An accepted invite to a party of your game is joined by itself: from the menu, from another party (left first) or after a match. On Invite Accepted reports it.
- Friends Only and Invite Only parties are checked by the leader. A player gets in when the leader or a member invited them with Invite Friend. Friends Only also lets in a friend of the leader or of a member. Invite Only lets in such a friend only after an accepted invite.
- Game driver interface `ICKPartyGameDriver` with the plugin's driver on the Online Subsystem; set your own class in the profile (Game Driver Class) or with Set Game Driver.
- The plugin adds the engine's `BeaconNetDriver` definition when the engine has none, with the classes of `GameNetDriver`.
- Party Profile data asset with its wizard, card and validation (size, privacy, beacon port, Ready resets, chat phrases and limits, pulse and reconnect timings, friend check time, game driver class, follow timeout).
- Ready-made party menu `CKPartyMenuWidget`, built in C++ with no Widget Blueprint and no CommonUI: the party, the chat with phrase buttons and an optional text box, and the friends list. It works with the mouse or a gamepad (the D-pad moves the highlight, A clicks, B clears the selected member).
- Demo map `L_PartyDemo`: click Play, then Play with 3 windows, and form a party with the mouse or a gamepad. Overview map `L_PartyOverview`: stands play the leader's rules live.
- Editor: the Party Profile appears in the CoopKit Hub with a card, a "New" wizard and a project check.
- AI agent skill `coopkit-party` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project with a script that names what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKPartyAgentLibrary` (`unreal.CKPartyAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
