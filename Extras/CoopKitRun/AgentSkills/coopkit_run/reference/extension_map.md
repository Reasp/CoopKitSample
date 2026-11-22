# CoopKitRun: extension map

Plugin module names: `CoopKitRun` (runtime), `CoopKitRunEditor` (editor only). Add `"CoopKitRun"` to the dependency lists of your project module's `Build.cs`, `"GameplayTags"` only if your code uses tags and `"GameplayAbilities"` only if it makes Gameplay Effects for upgrades. Headers are included by plain name: `#include "CKRunData.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Meta Track | `UCKRunMetaData` | `CKRunMetaData.h` | `MetaTracks` | `EntityId` | Icon (`Icon`) | no (`final`) | The meta track of runs (Content Browser > Add > CoopKit > Meta Track): the currency players earn in runs and the nodes they buy with it, which open upgrades and modifiers and give bonuses in every later run. |
| Modifier | `UCKRunModifierData` | `CKRunModifierData.h` | `Modifiers` | `EntityId` | Icon (`Icon`) | no (`final`) | A modifier of a run (Content Browser > Add > CoopKit > Modifier): a rule that changes the run for everyone, such as elite enemies, darkness or haste. |
| Run | `UCKRunData` | `CKRunData.h` | `Runs` | `EntityId` | Icon (`Icon`); DifficultyCurve (`DifficultyCurve`, Create button) | no (`final`) | A run of a co-op roguelite (Content Browser > Add > CoopKit > Run): its stages and room types, the upgrade pools of the offers, the settings of choices and votes, and the rules. |
| Upgrade Pool | `UCKRunUpgradePoolData` | `CKRunUpgradePoolData.h` | `UpgradePools` | `EntityId` | Icon (`Icon`) | no (`final`) | A pool of upgrades of a run (Content Browser > Add > CoopKit > Upgrade Pool). |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Other classes an agent will meet

| Class | Header | Role |
|---|---|---|
| `UCKRunComponent` | `CKRunComponent.h` | The run director (server authority): `StartRun`, `CompleteRoom`, `EnterNextRoom`, `TravelToRoom`, `EndRun`, `GrantUpgrade`, `AddModifier`; on every machine `GetPublicState`, `GetRoomSeed`, `GetLocalOffer`, `GetModifierValue`. One per world: `FindRunDirector`, `GetOrCreateRunDirector`. `final`: listen to its events. |
| `ACKRunDirector` | `CKRunDirector.h` | The always-relevant actor that carries the director when the game state has none. Subclass it to add components (a save component). |
| `UCKRunPlayerComponent` | `CKRunPlayerComponent.h` | On every player controller (added by the director): the player's requests `RequestPick`, `RequestReroll`, `RequestVote`, `RequestAbandon`, `RequestUnlock`, `RequestStartRun`; the answer in `OnRequestResult` with a reason tag `CK.Run.Reject.*`. Carries the player's own cards (owner only). |
| `UCKRunMetaComponent` | `CKRunMetaComponent.h` | On every player state when the run has a meta track: the player's meta progress (balance, nodes, statistics), replicated to every machine. |
| `ICKRunMetaStore` / `UCKRunLocalMetaStore` | `CKRunMetaStore.h` | Loads and saves meta progress; the default writes a save game slot per local player. |
| `ICKRunUpgradeReceiver` | `CKRunUpgradeReceiver.h` | Implement on a pawn, controller or player state (or a component of one): `OnRunUpgradeGranted(UpgradeId, Stacks)`, `OnRunUpgradeRemoved(UpgradeId)`, on the server. |
| `UCKRunPathRule`, `UCKRunOfferRule`, `UCKRunAutoPickRule` | `CKRunPathRule.h`, `CKRunOfferRule.h` | The run's rules (instanced on the run asset). Built-in: Doors, Linear; Weighted by Rarity; Highest Rarity. |
| `UCKRunRandomLibrary`, `FCKRunRandom` | `CKRunRandomLibrary.h`, `CKRunRandom.h` | Random numbers from a seed, the same on every machine and engine version; seed codes (`SeedToCode`, `CodeToSeed`), the seed of the day. |
| `CKRun::Core`, `CKRun::Rules` | `CKRunCore.h`, `CKRunRules.h` | The run's steps and checks as pure functions: what the director runs, usable in tests and tools. |
| `UCKRunAgentOps` (editor) | `CKRunAgentOps.h` | `SimulateRun`, `PreviewOffer`, `DescribeMetaTree`, `ValidateRunSet`: the card's Simulate, Odds and Unlocks tabs as JSON (`coopkit.agent/1`). |

Gameplay tags: `CK.Run.Phase.*` (the director's phase, read with `GetOwnedGameplayTags`), `CK.Run.Room.*` (room types), `CK.Run.Modifier.*`, `CK.Run.Reject.*` (reasons of refusals).

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `UCKRunComponent::OnRoomEntered(Room)` | every machine, when the team enters a room | build the room from `Room.RoomSeed` |
| `OnPhaseChanged`, `OnVotesChanged`, `OnOfferChanged`, `OnUpgradeGranted` | every machine | UI: choice screen, doors, who chose |
| `OnRunStarted`, `OnRunEnded(RunId, Reason)` | every machine | start and end screens, travel back to the hub |
| `UCKRunPathRule::BuildOptions` | server | the doors after a room (seed: `Seed` only) |
| `UCKRunOfferRule::BuildOffer` | server | the cards of an offer |
| `UCKRunAutoPickRule::ChoosePick` | server | the pick for a player whose time ran out |
| `ICKRunMetaStore::LoadMeta`, `SaveMeta` | owning client (Local Profile) or server (Server storage) | where meta progress is kept |
| `ICKRunUpgradeReceiver::OnRunUpgradeGranted`, `OnRunUpgradeRemoved` | server | an upgrade's effect without GAS |

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** Only for a *reference to another asset* (icon, curve, sound): a `TSoftObjectPtr` property marked with `meta = (CoopKitPart = "Label")`, and `CoopKitPartCreate = "true"` to offer a Create button.
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitRun/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Run.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitRun.uplugin`).

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitRunEditor/Private/`: `CKRunWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKRunDataValidation.cpp`, `SCKRunCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitRunTests/Private/` show how to make its data in code and test it.
