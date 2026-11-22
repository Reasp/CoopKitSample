# CoopKitSave: extension map

Plugin module names: `CoopKitSave` (runtime), `CoopKitSaveEditor` (editor only). Add `"CoopKitSave"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKSaveSubsystem.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Properties, Summary and Validation tabs. A part is a reference to another asset that the card shows with Open and Replace; a save profile refers to no other asset, so its Parts tab is empty.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Save Profile | `UCKSaveProfileData` | `CKSaveProfileData.h` | `SaveProfiles` | none | none | no (`final`) | Save profile of CoopKit Save: where saves go (slot prefix, limit), what is saved, when the game autosaves, how players are matched to their records and which Game Data Version the game writes. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it. The **New Save Profile** wizard (*Add > CoopKit > Save Profile*, or the hub) starts from the plugin's defaults or from another profile and takes a *Slot Prefix*; the game picks the profile with `SetProfile` or in *Project Settings > Plugins > CoopKit Save* (*Default Profile*).

## Classes you will meet

| Class | Role |
|---|---|
| `UCKSaveSubsystem` (`CKSaveSubsystem.h`) | game instance subsystem, the one door: `SaveToSlot`, `LoadFromSlot`, `DeleteSlot`, `GetSlots`, `DoesSlotExist`, `SetProfile`, `GetProfile`, `IsBusy`, `GetSessionSave`, `GetPlayerKey`, `RememberPlayer`, `RestorePlayer`, events; `GetSaveSubsystem(WorldContext)` |
| `UCKSaveableComponent` (*CoopKit Saveable*, `CKSaveableComponent.h`) | marks an actor saveable and says what is saved: transform, `SaveGame` properties of the actor and its components; *Save Id* for placed actors; `IsActorSaveable`, `FindSaveableComponent` |
| `ICKSaveable` (`CKSaveable.h`) | optional interface of an actor or component (also saveable without the component): `OnBeforeSave`, `OnAfterLoad(SavedGameDataVersion)` |
| `ICKSavePlayerIdentity` (`CKSavePlayerIdentity.h`) | optional interface of the player state: `GetSavePlayerKey` |
| `UCKSaveProfileData` | the profile (`final`): slot prefix, limits, autosave, what is saved, player matching, Game Data Version |
| `UCKSaveSettings` (`CKSaveSettings.h`, *Project Settings > Plugins > CoopKit Save*) | the default profile |
| `UCKSaveGame` (`CKSaveGame.h`) | the save file object: records of the game state, actors and players; `OnMigrateSave` hands it over |
| `UCKSaveAsyncAction` | Blueprint nodes *Save Game To Slot (CoopKit)* and *Load Game From Slot (CoopKit)* |
| `UCKSaveWorldSubsystem` | the world side (server): captures and applies actors; games rarely call it |
| `CKSave::Tags` (`CKSaveGameplayTags.h`) | `CK.Save.Block.Save`, `CK.Save.Block.Load` |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `ICKSaveable::OnBeforeSave()` | server, right before the state is written | copy runtime state into `SaveGame` properties |
| `ICKSaveable::OnAfterLoad(SavedGameDataVersion)` | server, right after saved values were applied | rebuild runtime state, convert old values by the version (RepNotify does not run on the server) |
| `OnSaveCompleted(SlotName, Result)`, `OnLoadCompleted(SlotName, Result)` of the subsystem | server, also for autosaves | a "Saved" message, error text from the result, reopen a menu |
| `OnPlayerRestored(PlayerState)` | server, a player got their saved player state (join, reconnect, load) | give gear, refresh the player's UI (the pawn follows when it spawns) |
| `OnMigrateSave(SaveGame, FromVersion, ToVersion)` | server, an older Game Data Version is loaded, before anything is applied | rename or remove records of the save |
| `ICKSavePlayerIdentity::GetSavePlayerKey()`, `ResolvePlayerKey` (C++ delegate) | server, when a record is looked up | key by an account id or a character slot |
| tags `CK.Save.Block.Save`, `CK.Save.Block.Load` on the game state, loose or from an effect | server | forbid a save or a load for a while (`Blocked`) |

## Old saves: add, rename, change meaning

Properties are saved by name, in a tagged format. A game that already has players' saves must follow this table before it changes a `SaveGame` property.

| Change | What happens to old saves | What to do |
|---|---|---|
| add a `SaveGame` property | the old save has no value: the property keeps its default | nothing; set a sensible default |
| remove a property | the old value is dropped | nothing |
| rename a property | the old value is not found | a property redirect in the project's `Config/DefaultEngine.ini`: `[CoreRedirects]` `+PropertyRedirects=(OldName="/Script/MyGame.MyActor.OldName",NewName="/Script/MyGame.MyActor.NewName")`, or keep the old name |
| rename a class or move an actor to another sub-level | the actor is not found | a class redirect, or set *Save Id* on the actor before the change |
| change what a value *means* (units, scale, enum order) | the old value is read as the new meaning | raise *Game Data Version* of the profile by one, convert in `OnAfterLoad(SavedGameDataVersion)` when `SavedGameDataVersion` is lower, or edit the records in `OnMigrateSave` |
| a save written by a higher version | refused: *Newer Game Data* / *Newer Format* | nothing: an older game does not read newer saves |

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab; a `SaveGame` property of an actor shows in the Details panel of that actor, under *Advanced*. Tell the user which asset to open and which field to set.
4. **Card part.** The save profile card has no parts. A data asset of your own gets the Data Asset editor, not a card.
5. **Runtime.** Saving and loading run on the server. A value that must be saved is a `UPROPERTY(SaveGame)`; if clients need to see it, it is also `Replicated` (replication reaches clients after a load). Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitSave/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Save.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- The save file format and the slot list: they are the plugin's; edit saves only through `OnMigrateSave` and the `UCKSaveGame` functions.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitSaveEditor/Private/`: `CKSaveWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKSaveDataValidation.cpp`, `SCKSaveCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitSaveTests/Private/` show how to make its data in code and test it.
