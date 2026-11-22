# CoopKitTraversal: extension map

Plugin module names: `CoopKitTraversal` (runtime), `CoopKitTraversalEditor` (editor only). Add `"CoopKitTraversal"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` when your code uses tags, `"EnhancedInput"` when it binds input. Headers are included by plain name: `#include "CKTraversalMovementComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Traversal Profile | `UCKTraversalProfileData` | `CKTraversalProfileData.h` | `TraversalProfiles` | `EntityId` | Icon (`Icon`); Animation (`Animation`); Input (`Input`, Create button); Assist Input (`AssistInput`, Create button) | no (`final`) | Traversal profile of a character: which obstacles each move takes and how long it lasts, the two-player moves, what the server tolerates and which states block a move. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKTraversalMovementComponent` (`CKTraversalMovementComponent.h`) | the character's movement component (a `UCharacterMovementComponent`): `RequestTraverse`, `OpenOffer`, `CloseOffer`, `ToggleOffer`, `BindProfileInputs`, `IsTraversing`, `GetTraversalKind`, `GetTraversalProgress`, `GetOffer`, `SetProfile`, `SetBlockedTags`, `GetNetStats`; publishes `CK.Traversal.State.*` and `CK.Traversal.Offer.*` tags. Not `final`: a project subclass may override `PhysCustom` modes of its own, but must call `Super` for the traversal mode |
| `ACKTraversalCharacter` (`CKTraversalCharacter.h`) | an `ACharacter` whose movement component is the one above; derive the project's character from it, or call `ObjectInitializer.SetDefaultSubobjectClass<UCKTraversalMovementComponent>(ACharacter::CharacterMovementComponentName)` in the project character's constructor |
| `UCKTraversalProfileData` (`CKTraversalProfileData.h`) | the profile: `FCKTraversalSettings` (one `FCKTraversalMoveSettings` per move: `bEnabled`, `MinHeight`, `MaxHeight`, `DurationSeconds`, `ExitSpeed`; probe sizes; server tolerances; offers; launch), `BlockedTags`, animation, icon, input actions; `GetDataProblems`, `MakePresetSettings` |
| `UCKTraversalAutopilotComponent` (`CKTraversalAutopilotComponent.h`) | walks a character to a point and asks for moves on the way (`RunTo`), or to a spot where it offers help (`OfferAt`); runs on the machine that predicts the character (AI on the server) |
| `UCKTraversalSubsystem` (`CKTraversalSubsystem.h`) | the open offers of a world: `FindNearestOffer`, `FindHelperByOfferId` |
| `CKTraversal::Rules` (`CKTraversalRules.h`) | pure functions: `PickMove` (which move an obstacle height gets alone and with an offer), `ClassifyObstacle`, `BuildPath` and `EvaluatePath` (a move's path as a function of time), `ValidateRequest` and `ValidateOffer` (the server's checks) |
| `CKTraversal::Tags` (`CKTraversalGameplayTags.h`) | `CK.Traversal.State.Traversing`, `.Assisting`, `CK.Traversal.Offer.Brace`, `.Kneel`, `.Cup`, `CK.Traversal.NoTraverse`, `CK.Traversal.Reject.*` (one per `ECKTraversalResult`); `GetRejectTag`, `GetOfferTag` |
| `UCKTraversalAgentOps` (`CKTraversalAgentOps.h`, editor) | JSON operations for agents: `CreateProfile`, `ApplyPreset`, `SetMoveSettings`, `SampleTrajectory`, `PickMoves`, `AnalyzeObstacle` |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `UCKTraversalMovementComponent::OnTraversalStarted` | every machine (the player's, the server, observers replaying) | sounds, camera shake, a quest step |
| `UCKTraversalMovementComponent::OnTraversalFinished` | every machine | landing sound, re-enable an ability |
| `UCKTraversalMovementComponent::OnTraversalResult` | the machine that asked (a server refusal reaches the owning client) | a prompt with the reason tag of a refusal |
| `UCKTraversalMovementComponent::OnOfferChanged`, `OnOfferTaken` | every machine | the helper's prompt, a "thanks" line, an achievement |
| *Blocked Tags* of the profile or `SetBlockedTags` | the player's machine and the server | stop moves while the project's state tag is on the character or its player state (downed, stunned, carrying) |
| `CK.Traversal.NoTraverse` on an actor (`IGameplayTagAssetInterface`) | the probe on every machine | obstacles nobody may climb |
| `IGenericTeamAgentInterface` on the pawn or controller | server | teams, with the profile's *Require Same Team* |

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

- Anything under `Plugins/CoopKitTraversal/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Traversal.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitTraversal.uplugin`).

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitTraversalEditor/Private/`: `CKTraversalWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKTraversalDataValidation.cpp`, `SCKTraversalCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitTraversalTests/Private/` show how to make its data in code and test it.
