# CoopKitCarry: extension map

Plugin module names: `CoopKitCarry` (runtime), `CoopKitCarryEditor` (editor only). Add `"CoopKitCarry"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` when your code uses tags. Headers are included by plain name: `#include "CKCarrierComponent.h"`. The plugin needs no other plugin: a downed state, an inventory or an interaction prompt of the project (or of another CoopKit plugin) reaches it through Gameplay Tags and the calls below.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Carry Item | `UCKCarryItemData` | `CKCarryItemData.h` | `CarryItems` | `EntityId` | Mesh (`Mesh`); Pick Up Montage (`PickUpMontage`); Throw Montage (`ThrowMontage`); Pick Up Sound (`PickUpSound`); Actor (`ActorClass`, Create button) | yes | What an item is like when somebody carries it: kind, mass, how fast its carriers walk, where the hands hold it, whether and how far it flies, when two carriers lose it. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKCarryItemData` (`CKCarryItemData.h`) | the carry item: `Kind` (`Hold` one lifts and may throw, `Heavy` two carry and one drags, `Body` one tows), `MassKg`, the four speed multipliers, `HoldSocket`/`HoldOffset` (for a `Body`, the tow point), `FirstGrip`/`SecondGrip`, throw speed and angle, `PickUpRange`, the leash (`LeashSlackCm`, `BreakDistanceCm`, `BreakSeconds`), `BlockedTags`, `RequiredTags`, the network settings; `CollectProblems` lists bad data by field. Not `final` |
| `UCKCarrierComponent` (`CKCarrierComponent.h`) | on the pawn that carries: `RequestPickUp`, `RequestJoin`, `RequestTow`, `RequestDrop`, `RequestThrow`, `GetHeldItem`, `OnCarryResult`; `ExecuteOnServer` runs a request from server code (AI, scripted sequences) and returns the answer; publishes `CK.Carry.State.Carrying` |
| `UCKCarryableComponent` (`CKCarryableComponent.h`) | on what is carried (a `ACKCarryActor`, any replicated actor whose root is the physics body, or a pawn for a `Body`): `SetItemData`, `GetCarryState` (`FCKCarryState`: phase, carriers), `GetPhase`, `OnPhaseChanged`, `ServerDropAll`, `ServerLetGo`; publishes `CK.Carry.State.Held`, `.Towed`, `.Thrown` |
| `ACKCarryActor` (`CKCarryActor.h`) | a ready item: hidden physics body, visible mesh (smoothed), Carryable; the wizard's `BP_Carry_<Name>` derives from it |
| `UCKCarrySubsystem` (`CKCarrySubsystem.h`) | every item of a world: `FindBestItem(Pawn, MaxDistance, MinFacingDot)` is the item in front of a pawn |
| `CKCarry::Rules` (`CKCarryRules.h`) | pure functions: `Decide` (the server's answer to a request, in the order of its checks), `PhaseAfterPickUp`, `HoldTransform`, `CoCarryTransform`, `LeashInput`, `ThrowVelocity` |
| `CKCarry::Tags` (`CKCarryGameplayTags.h`) | `CK.Carry.Reject.*` (one per `ECKCarryResult`: `OutOfReach`, `AlreadyHeld`, `HandsFull`, `NotTowable`, `NoFreeGrip`, `ThrowNotAllowed`, `Stretched`, ...), `CK.Carry.State.*`; `GetRejectTag` |
| `CKCarry::Metrics` (`CKCarryMetrics.h`) | pure functions that measure smoothness (`CountJumps`, `TrackError`): what the demo's live line and the network tests show |
| `UCKCarryAgentOps` (`CKCarryAgentOps.h`, editor) | JSON operations for agents: `CreateCarryItem`, `SetGrips`, `DescribeCarryItem`, `MakeLevelCarryable` |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `UCKCarrierComponent::OnCarryResult` | the machine that asked (the server's answer reaches the owning client), also when the server let an item go (`Stretched`, `NotTowable` when a body got up) | a sound on success, a prompt with the reason tag of a refusal |
| `UCKCarryableComponent::OnPhaseChanged` | every machine | animations, sounds and effects of picking up, carrying together, towing, flying, landing |
| *Blocked Tags* of the item data | server (and the player's prediction) | no carrying while the item or the carrier has a tag of the project (stunned, climbing) |
| *Required Tags* of the item data | server | a body may be towed only while it has the project's downed tag (`IGameplayTagAssetInterface` on the pawn) |
| `UCKCarrierComponent::ExecuteOnServer` | server | AI and scripted sequences carry through the same rules as players |
| `UCKCarryableComponent::ServerDropAll` | server | end a carry or a tow from the project's code (the body got up, the item is consumed) |

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

- Anything under `Plugins/CoopKitCarry/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Carry.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitCarry.uplugin`).

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitCarryEditor/Private/`: `CKCarryWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKCarryDataValidation.cpp`, `SCKCarryCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitCarryTests/Private/` show how to make its data in code and test it.
