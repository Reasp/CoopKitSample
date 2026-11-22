# CoopKitLoot: extension map

Plugin module names: `CoopKitLoot` (runtime), `CoopKitLootEditor` (editor only). Add `"CoopKitLoot"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKLootTableData.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Loot Table | `UCKLootTableData` | `CKLootTableData.h` | `LootTables` | `TableId` | Drop Mesh (`DropMesh`); Icon (`Icon`); Rolls Curve (`RollsCurve`, Create button) | no (`final`) | A loot table of CoopKit Loot: what may drop (items with weights, rarity, counts and guaranteed items), how many rolls for how many players, and how the drops are shared between the players (personal loot, free for all, round robin, Need / Greed rolls). |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKLootTableData` (`CKLootTableData.h`) | one loot table: sharing mode (Personal, Free For All, Round Robin, Need / Greed), entries (item id, name, optional item asset, rarity, weight, min and max count, guaranteed), rolls and extra rolls per player or a rolls curve, guaranteed rarity, roll time, pickup, eligibility and scatter radii; drop mesh, icon and rolls curve parts |
| `UCKLootSubsystem` (`CKLootSubsystem.h`) | world subsystem: `DropLoot`, `DropItems` (server), the server's random stream (`SetRandomSeed`), round-robin turns, `TryPickUp` and `TryChoose` (the server checks; bots call them directly), `GetEligiblePlayers`, `GetVisibleDrops`; events `OnLootDropped`, `OnLootPickedUp`, `OnRollClosed` (server) |
| `ACKLootDrop` (`CKLootDrop.h`) | one item on the floor, replicated: item, phase (Free, Assigned, Rolling, Taken), assigned player, personal flag, Need / Greed roll with its server-time deadline; a personal drop is relevant only to its owner's connection; `OnDropChanged` on every machine; Blueprint subclass for your own look (`SetDropClass`) |
| `UCKLootPlayerComponent` (`CKLootPlayerComponent.h`) | on the player controller (the subsystem adds it): `PickUp`, `Roll` to the server (reliable RPCs), answers `OnLootReceived` and `OnRejected` with a reason tag |
| `ICKLootReceiver` (`CKLootReceiver.h`) | interface of your inventory: `ReceiveLoot(Player, Item, Drop)` returns the units taken |
| `CKLoot::Rules` (`CKLootRules.h`) | pure functions: weighted pick, roll count, guarantees, Need / Greed resolution, round-robin turn |
| `CKLoot::Tags` (`CKLootGameplayTags.h`) | `CK.Loot.Reject.*`: Invalid, Taken, Rolling, NotYours, OutOfRange, Full, RollClosed, NotEligible, AlreadyChosen |
| `FCKLootEntry`, `FCKLootItem`, `FCKLootRollState` (`CKLootTypes.h`) | a table entry, a dropped item, a roll |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `UCKLootSubsystem::DropLoot` / `DropItems` | server | drop loot when your game decides |
| `ICKLootReceiver::ReceiveLoot` | server | put the item into your inventory; return the units taken |
| `UCKLootSubsystem::OnLootPickedUp` | server | quests, statistics, a sound for everyone |
| `UCKLootSubsystem::OnRollClosed` | server | announce the winner |
| `UCKLootPlayerComponent::OnLootReceived` / `OnRejected` | server and the owning player | "you got X", "not yours" in the UI |
| `ACKLootDrop::OnDropChanged` | every machine that has the drop | refresh a label, a roll window |
| `ACKLootDrop::UpdateVisuals` (Blueprint Native Event) | every machine | your own look of a drop |

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

- Anything under `Plugins/CoopKitLoot/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Loot.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitLootEditor/Private/`: `CKLootWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKLootDataValidation.cpp`, `SCKLootCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitLootTests/Private/` show how to make its data in code and test it.
