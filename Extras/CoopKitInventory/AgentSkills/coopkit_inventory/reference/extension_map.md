# CoopKitInventory: extension map

Plugin module names: `CoopKitInventory` (runtime), `CoopKitInventoryEditor` (editor only). Add `"CoopKitInventory"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKInventoryComponent.h"`. Code that reads what equipment grants (abilities, effects) also adds `"GameplayAbilities"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace. Inventory rules are ordinary plugin data (Add > CoopKit > Inventory Rules, their own data editor), not an entity.

Parts of an inventory item: **Icon** and **World Mesh** (`CoopKitPart` metadata), and one slot per **granted ability** and **granted effect** (the classes in `GrantedAbilities` and `GrantedEffects`, shown as their Blueprints) plus an empty slot that adds one. The card's Create on a grant slot makes an own Blueprint `GA_<Name>_Ability<N>` or `GE_<Name>_Effect<N>` (an infinite effect) in the item's folder; Replace lists only Blueprints of the right parent class; None removes the grant. Granted tags are plain data, not parts.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Inventory Item | `UCKInventoryItemData` | `CKInventoryItemData.h` | `InventoryItems` | none | Icon (`Icon`); World Mesh (`WorldMesh`) | yes | An item of CoopKit Inventory: what players see, how many stack, which equipment slot it fits and what it grants while equipped (Gameplay Ability System abilities, effects and tags; ignored when the owner has no Ability System Component). |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKInventoryComponent` (*CoopKit Inventory*, `CKInventoryComponent.h`) | the replicated inventory, on the player state (or any actor): reading (`GetEntries`, `FindEntry`, `FindEquippedEntry`, `GetItemCount`, `GetRoomFor`, `GetRules`), client requests (`RequestPickUp`, `RequestDrop`, `RequestGive`, `RequestEquip`, `RequestUnequip`, `RequestUse`), the server API (`AddItem`, `AddStack`, `RemoveItem`, `RemoveEntry`, `Equip`, `Unequip`, `TransferTo`, `DropEntry`, `PickUp`, `UseEntry`, `SetEntryStat`, `ClearInventory`, `SetRules`), events; `FindInventoryComponent(Actor)`, `FindLocalInventoryComponent(WorldContext)` |
| `UCKInventoryItemData` (`CKInventoryItemData.h`) | an item (not `final`): display, stacking, item tags, default stats, equipment slot, granted abilities, effects and tags, use |
| `UCKInventoryRulesData` (`CKInventoryRulesData.h`) | the rules of an inventory (not `final`): bag size, starting items, equipment slots, pick-up and give range, request rate, drop distance, pick-up class |
| `ACKInventoryPickup` (*CoopKit Inventory Pickup*, `CKInventoryPickup.h`, `Blueprintable`) | an item lying in the world: `GetStack`, `SetStack` (server); make a Blueprint child for your own look |
| `FCKInventoryEntry`, `FCKInventoryItemStack`, `FCKInventoryStat`, `FCKInventorySlotDefinition`, `FCKInventoryAbilityGrant`, `FCKInventoryEffectGrant`, `ECKInventoryResult` (`CKInventoryTypes.h`) | an entry of the inventory (id, item, count, slot, stats), a stack to add, a stat, an equipment slot, a grant, the answer to a request |
| `UCKInventorySettings` (`CKInventorySettings.h`, *Project Settings > Plugins > CoopKit Inventory*) | the default rules of every component without its own |
| `CKInventory::Tags` (`CKInventoryGameplayTags.h`) | `CK.Inventory.Slot.Weapon`, `.Armor`, `.Utility`; events `CK.Inventory.Event.Added`, `.Removed`, `.Equipped`, `.Unequipped`, `.Used`; blocks `CK.Inventory.Block.Pickup`, `.Drop`, `.Give`, `.Equip`; state `CK.Inventory.State.Equipped` |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnEntryAdded`, `OnEntryChanged`, `OnEntryRemoved` (`Inventory`, `Entry`) | the server and every client | an inventory UI, a weight or value total, a sound on pick-up |
| `OnEquipmentChanged(Inventory, Slot, OldItem, NewItem)` | the server and every client | an equipment UI, attaching a visible mesh to the pawn |
| `OnRequestResult(Inventory, RequestId, Result)` | the machine that made the request | tell the player why a request was refused (`ECKInventoryResult`) |
| `OnItemUsed(Inventory, Entry)` and the Gameplay Event `CK.Inventory.Event.Used` | the server | what using an item does (heal, open a map); the inventory already removed the unit if the item says so |
| `OnInventoryRestored(Inventory)` | the server, after a SaveGame load | rebuild your own state from the restored list |
| the tags `CK.Inventory.Block.*` on the owner, its pawn or its player state (`IGameplayTagAssetInterface` or a Gameplay Effect) | read by the server on every request | forbid pick-up, drop, give or equip while a condition holds |
| `UCKInventoryItemData::IsDataValid` in a subclass | the editor (card, Check Project, save) | report bad values of your own item fields |

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** Only for a *reference to another asset* (icon, curve, sound): a `TSoftObjectPtr` property of an item subclass marked with `meta = (CoopKitPart = "Label")`, and `CoopKitPartCreate = "true"` to offer a Create button. A hard reference or a class is not a part the card can show (the grant slots are the plugin's own list).
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true; the inventory itself changes only through its server API. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitInventory/` (see "Editing the plugin" in `rules.md`).
- The fields of `FCKInventoryEntry` and the item properties: saved inventories (SaveGame) and existing item assets read them.
- The gameplay tags `CK.Inventory.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitInventoryEditor/Private/`: `CKInventoryWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKInventoryDataValidation.cpp`, `SCKInventoryCard.cpp` (card tabs), `CKInventoryItemDataFactory.cpp` (the New Inventory Item wizard), `CKInventoryRulesDataFactory.cpp`, `CKInventoryDataSummary.cpp` (Summary text). The runtime code is `Source/CoopKitInventory/Private/CKInventoryComponent.cpp` (server checks, grants, replication). The plugin's own tests in `Source/CoopKitInventoryTests/Private/` show how to make its data in code and test it.
