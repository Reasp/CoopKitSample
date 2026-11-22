# Changelog: CoopKitSupport

All notable changes to CoopKitSupport are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitSupport.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Editor surface for AI agents: `UCKSupportAgentLibrary` (`unreal.CKSupportAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

## [1.0.0] - 2026-11-20

### Added

- Support abilities on the Gameplay Ability System: heal, shield, buff and aura, on the caster, on one ally or on every ally around. Each ability is one data asset (`UCKSupportAbilityData`) with its effect, targeting, amount, duration, cost, cooldown, team charge, synergy marks, Gameplay Cue, animation, icon and input action.
- Support component (`UCKSupportComponent`) for the player state, a pawn or an ally actor: *Request Cast* predicts the cooldown and the animation on the owning client, the server checks and answers with *On Cast Result*; a refusal takes the prediction back.
- Soft lock on allies: the ally in the cone of view, wounded allies first; the server checks the ally, health, blocked tags and range with room for lag.
- Synergies between players: an ability marks its targets, another player's ability consumes the mark for a stronger effect and extra team charge (*On Synergy* on every machine).
- One ultimate charge per team (`ACKSupportTeamState`): every cast adds to it with a share per player; a team ultimate spends it, whoever of the team casts it.
- Shields absorb damage written to an Incoming Damage attribute before health drops (or give health back after a hit without one); statuses replicate and are published as Gameplay Tags.
- Refusals with a reason: `ECKSupportResult` and `CK.Support.Reject.*` tags (`OnCooldown`, `NotEnoughCharge`, `NotEnoughResource`, `NoTarget`, `NotAnAlly`, `TargetDown`, `TargetBlocked`, `TooFar`, `RateLimited` and others).
- Health and costs on your attributes, found by name (`Health`, `MaxHealth`, `IncomingDamage`) or set on the component; a simple attribute set (`UCKSupportAttributeSet`) for projects without one; a Gameplay Ability (`UCKSupportGameplayAbility`) for input through the Ability System Component; *Bind Ability Inputs* for Enhanced Input.
- Editor: the **New Support Ability** wizard creates the ability and its own input action; the ability's card in the CoopKit Hub with Preview and Parts tabs, a Summary of the whole ability and a Validation tab that names every bad field.
- Demo map `L_SupportDemo` played with the mouse (six abilities, two training dummies, Hurt everyone, Restore everyone, Play with 2 windows) and an overview map `L_SupportOverview` with three stands: synergy, team ultimate, refusals.
- Automation tests on listen and dedicated servers at 150 ms lag and 2% loss: soft lock on an ally with a predicted cooldown, refusals on the client and on the server with the prediction taken back, a synergy between two clients, a shield that takes a hit, the team ultimate, a client that joins in progress; the demo panel by clicks; the overview map; the rules and the server path without network.
- AI agent skill `coopkit-support` for Claude Code, Antigravity, Codex and other agents: extends the plugin from your C++ module and builds the project, explaining what to install when the build cannot run. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
