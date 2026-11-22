# CoopKitInteract: extension map

Plugin module names: `CoopKitInteract` (runtime), `CoopKitInteractEditor` (editor only). Add `"CoopKitInteract"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKInteractableComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Interaction | `UCKInteractTypeData` | `CKInteractTypeData.h` | `Interactions` | `EntityId` | Icon (`Icon`); ProgressCurve (`ProgressCurve`, Create button) | no (`final`) | One kind of interaction: a lever, a heavy door for two, a valve to hold, a pickup. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKInteractTypeData` (`CKInteractTypeData.h`) | one kind of interaction: Instant or Hold, hold time, Required Players (at once), Max Players (the lock), Speed Per Extra Player, keep progress on release, range and break distance, line of sight, required and blocked tags, cooldown, single use; icon and progress curve parts |
| `UCKInteractableComponent` (`CKInteractableComponent.h`) | on the interactable actor (it must replicate): server checks, the lock (players taking part), progress on the server clock, completion, cooldown; replicated state and events on every machine |
| `UCKInteractorComponent` (`CKInteractorComponent.h`) | on the player controller: focus and prompt of the local player, `StartInteract`/`StopInteract` to the server (reliable RPCs), predicted start, answers `OnStarted`/`OnStopped`/`OnRejected` with a reason tag |
| `ICKInteractable` (`CKInteractable.h`) | optional interface of the interactable actor: `CanInteract` (extra rule, own reason tag) and `GetPromptText` |
| `UCKInteractSubsystem` (`CKInteractSubsystem.h`) | registry of the interactables of a world; `FindInteractableOf(Pawn)` |
| `CKInteract::Rules` (`CKInteractRules.h`) | pure functions: progress rate, progress at a server time, the order of the start checks |
| `CKInteract::Tags` (`CKInteractGameplayTags.h`) | `CK.Interact.Reject.*` (Disabled, Invalid, OutOfRange, NotVisible, Locked, Busy, Cooldown, UsedUp, MissingTag, Blocked, Refused), `CK.Interact.Stop.*` (Released, Completed, Cancelled), `CK.Interact.State.*` (InUse, Cooldown, UsedUp, Interacting), `CK.Interact.Block.Interact` |
| `ACKInteractShowcaseDevice` | ready demo interactable (moving part and label); an example of an actor that uses the component |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `ICKInteractable::CanInteract` | server (decision) and clients (prompt) | rules the data cannot express; return false and a reason tag |
| `ICKInteractable::GetPromptText` | the local player's machine | a prompt text of the actor ("Unlock (needs a key)") |
| `UCKInteractableComponent::OnCompleted` | every machine | the effect of the interaction; gameplay on the server only |
| `UCKInteractableComponent::OnParticipantJoined` / `OnParticipantLeft` | every machine (the leave reason on the server) | animations, sounds, "holding" poses |
| `UCKInteractableComponent::OnStateChanged` | every machine | refresh a world-space indicator |
| `UCKInteractorComponent::OnFocusChanged` | the local player | show or hide the prompt |
| `UCKInteractorComponent::OnRejected` / `OnStopped` | server and the owning player | tell the player why (`Reason` tag) |
| `CK.Interact.Block.Interact` on the pawn | server reads it | forbid interactions while downed, stunned, in a cutscene |
| `SetInteractionEnabled`, `SetInteraction`, `Cancel`, `CompleteNow`, `ResetInteractable` | server | scripted control of an interactable |

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

- Anything under `Plugins/CoopKitInteract/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Interact.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitInteractEditor/Private/`: `CKInteractWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKInteractDataValidation.cpp`, `SCKInteractCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitInteractTests/Private/` show how to make its data in code and test it.
