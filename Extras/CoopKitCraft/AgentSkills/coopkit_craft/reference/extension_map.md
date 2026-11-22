# CoopKitCraft: extension map

Plugin module names: `CoopKitCraft` (runtime), `CoopKitCraftEditor` (editor only). Add `"CoopKitCraft"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKCraftStationComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Building | `UCKCraftBuildingData` | `CKCraftBuildingData.h` | `Buildings` | `EntityId` | Icon (`Icon`); PreviewMesh (`PreviewMesh`) | no (`final`) | A building of CoopKit Craft (a shelter, a barricade, a turret base): what it costs, who pays, how long it takes, where it may stand and which actor appears when it is done. |
| Recipe | `UCKCraftRecipeData` | `CKCraftRecipeData.h` | `Recipes` | `EntityId` | Icon (`Icon`); ProgressCurve (`ProgressCurve`, Create button) | no (`final`) | A recipe of CoopKit Craft: what goes in, what comes out, how long the station works and on which stations it is made. |
| Station | `UCKCraftStationData` | `CKCraftStationData.h` | `Stations` | `EntityId` | Icon (`Icon`) | no (`final`) | A kind of crafting station of CoopKit Craft (a workbench, a forge, a chemistry table): the recipes it offers, how many jobs its queue holds and how near a player must stand to use it. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKCraftRecipeData` (`CKCraftRecipeData.h`) | one recipe: inputs and outputs (`FCKCraftItem`: item id, display name, optional item asset, count), craft time, station tags; `GetDataProblems` |
| `UCKCraftStationData` (`CKCraftStationData.h`) | one kind of station: recipes it offers, queue length, use range, station tags; `OffersRecipe` |
| `UCKCraftStationComponent` (`CKCraftStationComponent.h`) | on the station actor (it must replicate): the shared queue, contributions per player, the job in work on the server clock, held items; server API `QueueJob`, `Contribute`, `CancelJob`, `Collect`, `PrepareForSave`, `RestoreAfterLoad` |
| `UCKCraftPlayerComponent` (`CKCraftPlayerComponent.h`) | on the player controller: `RequestQueue`, `RequestContribute`, `RequestCancel`, `RequestCollect`, `RequestPlace` (server RPCs), `OnRequestResult`, request limit |
| `ICKCraftInventory` (`CKCraftInventory.h`) | the game's inventory as the station sees it: `GetCraftItemCount`, `TakeCraftItems`, `GiveCraftItems`; on the pawn, controller, player state or one of their components |
| `UCKCraftItemBagComponent` (`CKCraftItemBagComponent.h`) | a simple replicated inventory (item ids with counts) that implements `ICKCraftInventory` |
| `ACKCraftStation` (`CKCraftStation.h`) | ready station actor: base mesh, sign with the queue, the station component |
| `UCKCraftBuildingData` (`CKCraftBuildingData.h`) | one building: cost, funding (`Shared` or `PlacerPays`), build seconds, footprint, building class, placement rules (place and use range, slope, grid, rotation step, spacing, sites per player); `GetDataProblems` |
| `ACKCraftBuildSite` (`CKCraftBuildSite.h`) | a construction site the server spawns at a placement: its station component holds one job, the building (`IsBuildSite`, `GetSiteBuilding`); players use `RequestContribute`, `RequestCancel`, `RequestCollect` on it |
| `UCKCraftBuildLibrary` (`CKCraftBuildLibrary.h`) | `CheckPlacement` (any machine, for a preview), `CheckPlacementForPlayer` and `PlaceBuilding` (server), `GetBuildSites`, `CountOpenSites`, `SnapPlacement` |
| `UCKCraftBuildPreviewComponent` (`CKCraftBuildPreviewComponent.h`) | on the player controller, local only: the green or red ghost, `StartPreview`, `StepPreview`, `RotatePreview`, `SetPreviewLocation`, `ConfirmPlacement` (sends `RequestPlace`) |
| `ACKCraftBuilding` (`CKCraftBuilding.h`) | the ready block a site spawns when the building has no class of its own |
| `CKCraft::Rules` (`CKCraftRules.h`) | pure functions: result tags, station tags, player keys, missing units, the job to start, progress on the server clock, the request limiter, grid and rotation snapping, slope, footprint overlap, the order of the placement rules (`GetPlacementResult`) |
| `CKCraft::Tags` (`CKCraftGameplayTags.h`) | `CK.Craft.Reject.*`, one per `ECKCraftResult` value |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `ICKCraftInventory` on your inventory | server | connect the game's inventory; return the units really taken or given |
| `UCKCraftStationComponent::OnJobFinished` | every machine | the effect of a finished job (sound, effect, a quest step on the server); `bCompleted` tells done from cancelled |
| `UCKCraftStationComponent::OnQueueChanged`, `OnJobAdded`, `OnContributed`, `OnJobStarted` | every machine | refresh a station panel or a world-space sign |
| `UCKCraftPlayerComponent::OnRequestResult` | server and the owning player | tell the player why (`Reason` tag) |
| `QueueJob`, `Contribute`, `CancelJob`, `Collect` of the station | server | AI, scripted sequences, rewards that queue a job |
| `PrepareForSave`, `RestoreAfterLoad` | server | call them around your save system |
| `UCKCraftStationComponent::OnBuildingBuilt` | server | the building a site spawned: give it your components, register it in your save system, start a quest step |
| `UCKCraftBuildLibrary::PlaceBuilding` | server | AI and scripted sequences that place buildings; the same checks as a player's `RequestPlace` |
| `Building Class` and `Site Class` of the building data | server spawns them | your own building actor (origin at the bottom center) and your own site look (a subclass of `ACKCraftBuildSite`) |

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

- Anything under `Plugins/CoopKitCraft/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Craft.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitCraft.uplugin`).
- Buildings stand on World Static ground one by one: no sockets, supports, damage or demolition. Sites and buildings are spawned actors: the game's save system must spawn them again after a load.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitCraftEditor/Private/`: `CKCraftWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKCraftDataValidation.cpp`, `SCKCraftCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitCraftTests/Private/` show how to make its data in code and test it.
