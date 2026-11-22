# Changelog: CoopKitTraversal

All notable changes to CoopKitTraversal are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitTraversal.uplugin`, and the notes of a released version are pasted into
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

- First version of CoopKit Traversal: a character movement component with predicted traversal moves for co-op games.
- Moves alone: vault, low mantle, high mantle, climb over. The plugin finds the obstacle ahead, measures its height and depth and picks the move.
- Moves together: a boost up a wall from a friend's brace, a pull-up onto a ledge from a friend's kneel, a launch from a friend's cupped hands. The helper opens an offer (*Open Offer*, *Toggle Offer*); the server gives it to the first friend whose move arrives.
- Network: the move runs in the character's movement as a custom mode, predicted on the player's machine and run on the server along the same path from the same moment. The start travels with every movement update until the server acknowledges it. Other players replay the move from its replicated start. In the network test at 150 ms lag and 2 % packet loss, 400 moves needed no server correction.
- Refusals with a reason: `ECKTraversalResult` and a `CK.Traversal.Reject.*` Gameplay Tag for each, on *On Traversal Result*.
- State as Gameplay Tags (`CK.Traversal.State.Traversing`, `CK.Traversal.State.Assisting`, `CK.Traversal.Offer.*`); *Blocked Tags* on the profile and the component stop moves by another plugin's state; `CK.Traversal.NoTraverse` marks actors nobody traverses; *Require Same Team* through `IGenericTeamAgentInterface`.
- Traversal profile data asset: heights, durations and exit speeds of the seven moves, offers, server tolerances, animation, icon and two input actions (*Bind Profile Inputs*). The card has Summary, Validation, Preview (the path of every move for an obstacle height on a slider) and Parts tabs; the **New Traversal Profile** wizard starts from the Standard, Agile or Heavy preset.
- `ACKTraversalCharacter`, a character with the movement component, and `UCKTraversalAutopilotComponent`, which walks a character and asks for moves the way a player does (AI, scripted scenes, tests).
- Demo map `L_TraversalDemo`: a course of seven lanes, all moves and a refusal on buttons of a panel, helper bots, network lag buttons and the server correction count. Overview map `L_TraversalOverview`: three stands that play by themselves.
- Editor: the traversal profile appears in the CoopKit Hub with its card, its wizard and the project check; rename, duplicate and delete keep its own input actions with it.
- AI agent skill `coopkit-traversal` for Claude Code, Antigravity, Codex and other agents. Editor surface for AI agents: `UCKTraversalAgentLibrary` (list, create, describe, validate, export and import profiles) and `UCKTraversalAgentOps` (create from a preset, change a move, sample a move's path, the moves for an obstacle height, an actor of the level read as an obstacle), in Python and Editor Utility Blueprints, answering in JSON.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
