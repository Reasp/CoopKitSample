# Changelog: CoopKitCarry

All notable changes to CoopKitCarry are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitCarry.uplugin`, and the notes of a released version are pasted into
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

- First version of CoopKit Carry: pick up, carry, drop and throw items in a co-op game, carry a heavy item together, tow a downed teammate.
- Network: the server decides who carries what and replicates only that; every machine puts an item in the hands at the hands of its carrier, so it never lags behind the hand. A throw flies with the server's physics; the player who threw it sees the flight at once and the server's flight corrects it smoothly. In the plugin's network tests at 150 ms lag and 2 % packet loss: the item 0.0 cm from the hands on every machine, the thrower's flight 0.1 cm or less off the server at the 95th percentile in our runs on UE 5.8, also when the game thread stalls for 0.15 s on every 7th frame during the throw (the test's limit is 15 cm), with no hard correction, rest within 2 cm on every machine.
- Two carriers: one player drags a heavy item alone, a second takes the free grip (*Request Join*) and both carry it; each limits only their own walk by a leash, and the server lets go when they stay too far apart (*Stretched*).
- Tow a body: a pawn with a body item is towed at the tow point behind the tower on every machine, without movement corrections; the body's *Required Tags* name the game's downed tag (no dependency on another plugin).
- Refusals with a reason: `ECKCarryResult` and a `CK.Carry.Reject.*` Gameplay Tag for each (*Already Held*, *Hands Full*, *Not Towable*, *No Free Grip*, *Throw Not Allowed*, *Out Of Reach* and others), on *On Carry Result*; the prediction goes back smoothly. State as Gameplay Tags (`CK.Carry.State.*`); *Blocked Tags* stop carrying by another system's state.
- Carry item data asset (`UCKCarryItemData`): kind (Hold, Heavy, Body), mass, speeds, grips, throw, leash, network settings, tags, mesh, montages, sound and the item's actor. The **New Carry Item** wizard creates the item and its Blueprint `BP_Carry_<Name>`; the card has Summary, Validation, Preview and Parts tabs, and every data problem names its field.
- `UCKCarrierComponent` (its *Hold Origin* lifts the items for pawns whose root is not at hip height), `UCKCarryableComponent`, the ready item actor `ACKCarryActor` and `UCKCarrySubsystem` (*Find Best Item*); the rules and the smoothness metrics as pure functions (`CKCarry::Rules`, `CKCarry::Metrics`).
- Demo map `L_CarryDemo`: crates, a lantern, a chest and a training dummy, everything on buttons of a panel (Go to, Walk, Turn, Pick up, Join, Tow, Drop, Throw, Down, Restore, Play with 2 windows), the server's last answer and a live line of how far the item is from the hands. Overview map `L_CarryOverview`: four stands that play by themselves.
- Editor: the carry item appears in the CoopKit Hub with its card, its wizard and the project check; rename, duplicate and delete keep its own Blueprint with it (a duplicated item gets a Blueprint that carries the copy).
- AI agent skill `coopkit-carry` for Claude Code, Antigravity, Codex and other agents. Editor surface for AI agents: `UCKCarryAgentLibrary` (list, create, describe, validate, export and import items) and `UCKCarryAgentOps` (create an item with its Blueprint, set the grips, describe an item, make an actor of the level carryable), in Python and Editor Utility Blueprints, answering in JSON.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
