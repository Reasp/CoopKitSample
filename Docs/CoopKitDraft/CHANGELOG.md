# Changelog: CoopKitDraft

All notable changes to CoopKitDraft are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitDraft.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Draft modes in the CoopKit Hub (*Tools > CoopKit Hub*, category **Draft Mode**), next to the entities of other installed CoopKit plugins. The hub opens on categories. Click one to list its entities, and click **< Back** to return. The right side shows the selected entity with **Open**, **Where Used** and **Check Project**.
- Draft mode card: Preview and Parts tabs in the data editor. The Parts tab shows the hero pool of the mode, with Open, Replace and Create.
- New Draft Mode wizard. **Start From** copies the rules of a mode you pick. **Create Hero Pool** makes the mode's own hero pool in its folder, with the heroes of a pool you pick, of the Start From mode, or of the demo pool. With it off, the mode uses the pool you pick where it is.
- Plugin functions that rename, duplicate and delete a draft mode together with its own hero pool. A hero pool outside the mode's folder is never moved or deleted. The hub has no buttons for them.
- AI agent skill `coopkit-draft` for Claude Code, Antigravity, Codex and other agents. It adds draft rules, changes modes and reacts to the draft events from your C++ module, builds the Editor target with a script and runs the tests. If the build cannot run, the agent tells you what to install. Install the skill with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- Editor surface for AI agents: `UCKDraftAgentLibrary` (`unreal.CKDraftAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

### Changed

- Draft modes made in the Content Browser open the New Draft Mode wizard first. By default it also makes their own hero pool next to them.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains how to test data validation and what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-15

### Added

- First version of CoopKit Draft.
- Draft Mode and Draft Hero Pool data assets with their own editor window (Properties, Summary, Validation).
- Swappable rules for each axis of a draft: turn order (Simultaneous Order, Turn Order with snake order and captain bans), pool (Random Pool, Personal Choices), checks (Unique Hero per team or per match, Role Limit) and the pick on timeout (Auto Pick: the hovered hero, else a random valid one). Subclass any rule base in C++ or Blueprint for your own rules.
- Draft component for any game state: the server checks every pick and ban, ends turns by the server clock and keeps the random seed. The state replicates to every client.
- Player component on the player controller for requests from clients, with reject reasons as Gameplay Tags (`CK.Draft.Reject.*`).
- Blueprint events: phase changed, turn changed, hero picked, banned and hovered, draft completed, action rejected.
- Four ready modes: Team Unique Pick, Snake Draft, Half Pool, Random Choices, with a demo pool of ten heroes.
- Demo map `L_DraftDemo` and overview map `L_DraftOverview` with the four modes drafting live with bots.
- The demo map is ready to play with the mouse: on a panel of buttons the host starts and cancels a draft in any ready mode, and every player picks, bans, votes and swaps by clicking. Heroes the rules rule out are greyed out. Players are named *Player 1*, *Player 2* in join order. With one Play In Editor window, a button on the panel restarts Play as a listen server with two windows.
- Console commands `ck.draft.Start`, `Cancel`, `Pick`, `Ban`, `Hover`, `Status` and the variable `ck.draft.TurnTimeScale`.
- Blind picks (Blind Pick rule): picks, and optionally bans, stay hidden from the other teams, or from everyone but the player, until their turn ends. The server sends each player only the picks that player may see, through the player component.
- Reserve time (Time Reserve rule): extra seconds for each player for the whole draft, used after a turn's timer runs out.
- Hero swaps between teammates (Team Swap rule): offer, accept or decline, offers that expire, and a swap window after the last pick. New events On Swap Offered and On Heroes Swapped, new reject reasons `CK.Draft.Reject.SwapDisabled`, `SwapClosed`, `NotTeammate`, `NoHero`, `NoOffer`, `SwapDeclined`, `SwapExpired`.
- Two more ready modes: Blind Pick (with a swap window) and Reserve Draft.
- Console commands `ck.draft.Swap` and `ck.draft.AnswerSwap`; Pick, Ban, Hover and the swap commands take an optional player controller index for split screen.
- Team votes for bans (Team Ban Vote rule): a ban turn becomes a vote of the whole team. Players vote with Request Ban and may change their vote until the turn ends. The hero with the most votes is banned; the captain or the server's random stream breaks a tie. Only the voter's team receives the votes. New event On Ban Vote Changed.
- A seventh ready mode: Team Vote Bans.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
