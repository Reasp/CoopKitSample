# Changelog: CoopKitCombat

All notable changes to CoopKitCombat are listed here, newest first. The plugin follows [Semantic Versioning](https://semver.org/):
the version below matches `VersionName` in `CoopKitCombat.uplugin`, and the notes of a released version are pasted into
the Changelog field of the Fab listing.

Sections used in a version (omit the empty ones):

- **Breaking** - changes to the integration contract: Gameplay Tags, interfaces, Blueprint API, replication. Bumps the major version.
- **Added** - new features. Bumps the minor version.
- **Changed** - behavior changes that keep the contract.
- **Fixed** - bug fixes. Bumps the patch version.
- **Engine** - Unreal Engine versions added or dropped.

## [Unreleased]

### Added

- Editor surface for AI agents: `UCKCombatAgentLibrary` (`unreal.CKCombatAgentLibrary` in Python, also callable from Editor Utility Blueprints) lists, creates, describes, validates, exports and imports the plugin's data assets and runs the project check, answering in JSON. The AI agent skill has a new section, "Drive the editor", with Python examples.

## [1.1.0] - 2026-11-20

### Added

- Ranged combat. An attack step can now be a hitscan shot or a projectile (`Kind`). A shot has a range, a spread cone, pellets (for a shotgun), a projectile speed, radius and class, and a cost in rounds. *Combo Window Start* sets the fire rate. A move set has a magazine: capacity, reload time and ammo type.
- The shot and its rounds are predicted on the player's machine. A hitscan shot traces every pellet from the eyes along the aim. A projectile (`ACKCombatProjectile`) flies on every machine that shows the shot, and only the shooter's machine reports what it hits.
- The server checks every reported hit of a shot against the shot it started: time, range, spread cone, line of sight, team, target state and the number of reports. It refuses shots faster than the fire rate (`FireRate`) and shots without rounds (`NoAmmo`). The owner then gets its round back.
- Every refusal reason has a Gameplay Tag `CK.Combat.Refusal.<Reason>` (`UCKCombatComponent::GetRefusalTag`). The server sends the attacker the Gameplay Event `CK.Combat.Event.Refused` with the reason in its instigator tags.
- Rounds are the new attribute `Ammo` of `UCKCombatAttributeSet` (setting *Ammo Attribute*; the resource effect reads `CK.Combat.SetByCaller.Ammo`). The new ability `UCKCombatReloadAbility` (`CK.Combat.Ability.Reload`, state `CK.Combat.State.Reloading`) refills the magazine. Reserve rounds come from the new interface `ICKCombatAmmoProvider`, for example over an inventory; without one, reloads are unlimited. Each move set keeps its own magazine when you switch weapons.
- Combat component: `Reload`, `StartFiring` and `StopFiring` (hold the trigger), `SetMoveSet` (equip a weapon without an ability subclass), `GetMoveSet`, `GetAmmo`, `GetMagazineCapacity`, `IsReloading`, `IsFiring`, `GetShotsFired`. Gameplay Cue `GameplayCue.CK.Combat.Shot` plays locally on every machine.
- Editor: the New Move Set wizard has *Start As*: a melee combo, a rifle, a shotgun or a projectile launcher. The hub creates them the same way. The move set card's Summary shows each shot (kind, shots a minute, range, spread, pellets, damage, rounds) and the magazine. Validation reports a shot without a range and a shot that costs more rounds than the magazine holds.
- Demo: the panel has **Switch weapon** (sword, rifle, shotgun, launcher), **Shoot**, **Hold the trigger** / **Release the trigger** and **Reload**, and shows your rounds. The overview map has a fifth stand with a burst, a reload and a projectile.
- Automation tests `CoopKit.Combat.Ranged.DedicatedServer` and `.ListenServer` (150 ms lag, 2% loss), `CoopKit.Combat.Demo.Ranged` and `CoopKit.Combat.Data.RangedMoveSetEditor`.

### Changed

- CoopKit Hub: the single table of all entities is replaced by categories. The window opens with one button per entity type of every installed CoopKit plugin, showing the number of its entities. Click a category to list only its entities, with search and **New**. Click **< Back** to return to the categories. The right side shows the selected entity with **Open**, **Where Used** and **Check Project**. The card still opens in its own window. If several CoopKit plugins are installed, the plugin with the newest hub shows the tab, so this layout appears as soon as one of them has it.
- AI agent skill: the build steps are now plain text in `SKILL.md` instead of scripts. The agent still finds the engine, builds the project with the engine's `Build.bat` and reads the build log using `reference/build_and_diagnose.md`. We removed `scripts/build.ps1` and `scripts/build.sh` because Fab does not accept executables in plugin packages. How you use the skill does not change.

### Fixed

- Card: an entity with no parts now says so in the Preview and Parts tabs, instead of asking you to pick a part in an empty Parts tab.
- Card: a part in the entity's folder that another asset also uses is now marked "shared" in the Parts tab, as the warning above the list says, instead of "own".
- AI agent skill: it now says the plugin supports Windows (Win64) only, instead of implying Mac support. Its version-check example uses an engine version from the supported range (5.6 to 5.8). It also explains what to do when a test that starts several Play In Editor instances cannot start.

## [1.0.0] - 2026-11-13

### Added

- First version of CoopKit Combat: `UCKCombatComponent` for any pawn, with a three-hit combo (combo window and input buffer), block and parry, a root motion dodge with invulnerability frames; client prediction with rollback on the Gameplay Ability System; server-validated hits with reason codes (`ECKCombatHitResult`).
- Attribute set `UCKCombatAttributeSet` (Health, Stamina, Energy, Armor) and effects driven by SetByCaller; an attribute map in the project settings for your own attributes.
- Move set and profile data assets (`UCKCombatMoveSetData`, `UCKCombatProfileData`) with the CoopKit Combat data editor (Properties, Summary, Validation).
- Gameplay Cues: swings and dodges played locally on every machine, hits sent once and predicted by the attacker; placeholder cue `UCKCombatDebugCue`.
- Training dummy `ACKCombatDummy` (`GetMode` tells what it does), shown as the engine's mannequin in a color for its mode: gray Idle, blue Block, gold Parry, red Attack. Demo character `ACKCombatDemoCharacter`, also a mannequin, with a camera behind its left shoulder so the target in front stays in view.
- Ready-to-play demo map `L_CombatDemo` with a panel of buttons, so the whole demo runs with mouse clicks: go to a training dummy or a player, attack, raise and lower the block, dodge. With one Play In Editor window, a button on the panel restarts Play as a listen server with two windows.
- Gameplay Tags `CK.Combat.State.*`, `.Ability.*`, `.Event.*`, `.SetByCaller.*`, `.Input.*`, `.Block.*`, `GameplayCue.CK.Combat.*`.
- Console commands `ck.combat.*`, debug overlay `ck.combat.Debug 1`.
- Move sets in the CoopKit Hub (*Tools > CoopKit Hub*, type **Move Set**), next to the entities of other installed CoopKit plugins. After **New**, the hub selects the new move set in the list and shows where it was created.
- Move set card: Preview and Parts tabs in the data editor. The Parts tab lists the montage and the hit cue of every attack with Open and Replace; the hit cue row shows its Gameplay Cue tag.
- New Move Set wizard: a name, a move set to start from, and one animation per attack, for which it makes own montages in the move set's folder.
- Workspace functions that rename, duplicate and delete a move set together with its own montages. Gameplay Cue notifies and assets outside the move set's folder are never moved or deleted. The hub does not offer these actions yet.
- AI agent skill `coopkit-combat` for Claude Code, Antigravity, Codex and other agents: changes combat from your C++ module, builds the Editor target with a script that names what to install when the build cannot run, and runs the tests. Install it with *Tools > CoopKit > Install AI Agent Skills* or the button in the CoopKit Hub.
- A move set made in the Content Browser opens the New Move Set wizard first; its display name defaults to the asset's name.
- Automation tests `CoopKit.Combat.*`: combo, parry, block and dodge on listen and dedicated servers under 150 ms lag and 2% loss, with attributes converging on every machine. Another test covers a block raised during an attack and lowered at once, followed by a dodge, under the same lag and loss. Further tests cover forged hits, network load of Gameplay Cues, rules, attributes, data editors, the CoopKit Hub, the agent skill and the demo panel.

### Engine

- Unreal Engine 5.6, 5.7 and 5.8 on Windows (Win64).
