# CoopKitObjectives: extension map

Plugin module names: `CoopKitObjectives` (runtime), `CoopKitObjectivesEditor` (editor only). Add `"CoopKitObjectives"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKObjectivesExampleData.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Quest | `UCKObjectivesQuestData` | `CKObjectivesQuestData.h` | `Quests` | `EntityId` | Icon (`Icon`); PlayerScaleCurve (`PlayerScaleCurve`, Create button) | no (`final`) | A quest of CoopKit Objectives: steps of objectives that count gameplay events by tag, where the quest goes after each step (branches), what the players get and who starts it. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKObjectivesQuestData` (`CKObjectivesQuestData.h`) | one quest: steps (`FCKObjectivesStep`: objectives, All or Any Objective, next step, time limit and timeout), objectives (`FCKObjectivesObjective`: event tag, count, scope `Shared`/`EveryPlayer`/`AnyPlayer`, scale with players, branch, fails the quest, target actor tag), rewards (`FCKObjectivesReward`: id, asset, amount, recipients), required quests, repeatable, start vote; `GetDataProblems`, `DescribeStepGraph` |
| `UCKObjectivesQuestLogComponent` (`CKObjectivesQuestLogComponent.h`) | the server's judge, on the game state or a `ACKObjectivesTracker` (made on first use): `StartQuest`, `StartQuestForPlayers`, `Vote`, `AbandonQuest`, `ReportEvent`, `CompleteObjective`, `SetObjectiveTarget`, `FailQuest`, `PrepareForSave`, `RestoreAfterLoad`; static `FindQuestLog`, `GetOrCreateQuestLog`, `ReportObjectiveEvent` |
| `UCKObjectivesPlayerComponent` (`CKObjectivesPlayerComponent.h`) | on the player controller: `RequestStart`, `RequestVote`, `RequestAbandon` (server RPCs with the quest's path), `OnRequestResult`, request limit |
| `ICKObjectivesRewardReceiver` (`CKObjectivesRewardReceiver.h`) | where rewards go: `ReceiveQuestReward`; on the pawn, controller, player state or one of their components |
| `UCKObjectivesRewardBagComponent` (`CKObjectivesRewardBagComponent.h`) | a simple replicated reward store (reward ids with amounts) that implements the receiver |
| `ACKObjectivesArea` (`CKObjectivesArea.h`) | a place to reach: reports its event tag for a player whose pawn comes within its radius |
| `ACKObjectivesTracker` (`CKObjectivesTracker.h`) | an always relevant actor that carries the quest log |
| `CKObjectives::Rules` (`CKObjectivesRules.h`) | pure functions: result tags, player keys, event matching, scaled targets, objective and step completion, branches, votes, reward recipients, the request limiter |
| `CKObjectives::Tags` (`CKObjectivesGameplayTags.h`) | `CK.Objectives.Reject.*` (one per `ECKObjectivesResult` value) and the ready event tags `CK.Objectives.Event.Kill`, `.Collect`, `.Reach`, `.Interact` |
| `CKObjectives::Demo` (`CKObjectivesDemoQuests.h`) | `FQuestValues`, `MakeStep`, `MakeObjective`, `MakeReward`, `MakeTransientQuest`: quests in code for tests |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `UCKObjectivesQuestLogComponent::OnQuestAdded`, `OnStepStarted`, `OnObjectiveProgress`, `OnQuestsChanged` | every machine (server at once, clients when the log arrives) | panels, markers, the game's reaction to a step (on the server only for gameplay) |
| `UCKObjectivesQuestLogComponent::OnQuestFinished` | every machine (reliable multicast) | the end of a quest: `Completed`, `Failed`, or `Removed` (abandoned, vote failed) |
| `UCKObjectivesQuestLogComponent::OnRewardGranted` | server | effects, analytics, achievements when a receiver took a reward |
| `UCKObjectivesPlayerComponent::OnRequestResult` | server and the owning player | the panel's line for a refusal (`Reason` tag) |
| `ICKObjectivesRewardReceiver::ReceiveQuestReward` | server | the game's inventory, wallet or experience |

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

- Anything under `Plugins/CoopKitObjectives/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Objectives.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitObjectives.uplugin`).

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitObjectivesEditor/Private/`: `CKObjectivesWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKObjectivesDataValidation.cpp`, `SCKObjectivesCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitObjectivesTests/Private/` show how to make its data in code and test it.
