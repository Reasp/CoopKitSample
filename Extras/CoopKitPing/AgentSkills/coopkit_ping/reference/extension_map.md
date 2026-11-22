# CoopKitPing: extension map

Plugin module names: `CoopKitPing` (runtime), `CoopKitPingEditor` (editor only). Add `"CoopKitPing"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Add `"GameplayTags"` when your code uses the block tag. Headers are included by plain name: `#include "CKPingMatchComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (here: the ping types of the rules) that the card shows with Open, Replace and Create.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Ping Rules | `UCKPingRulesData` | `CKPingRulesData.h` | `PingRules` | none | none | no (`final`) | Rules of CoopKit Ping for a match: the command wheel, the quick ping, how far players can ping, the server rate limit, who sees pings. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

The parts of ping rules are listed by the plugin, not by `CoopKitPart` metadata: **Quick Ping** (`QuickPingType`, the type of a short press) and one slot per wheel sector **Wheel 1**, **Wheel 2**, ... (`WheelTypes`, clockwise from the top), plus an empty slot after the last sector while the wheel has room (at most 8). Replace with none removes a sector (the ones behind it move up) or empties the quick ping; Replace in the empty slot adds a sector. The **New Ping Rules** wizard starts from the project's default rules or other rules and, with *Create Types*, makes own copies of their ping types in the rules' folder (`DA_PingType_<Name>_Wheel<N>`; the quick ping points at the copy of the sector it shared); otherwise the rules use the types where they are (shared parts). The card's **Create** makes a new type (`DA_PingType_<Name>_Quick`, `DA_PingType_<Name>_Wheel<N>`), named after its slot.

The ping type `UCKPingTypeData` (`CKPingTypeData.h`) is a data asset with the same data window but is not a hub entity: create it in *Add > CoopKit > Ping Type*, or let the wizard or the card make it. It is `final` too.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKPingMatchComponent` (`CKPingMatchComponent.h`) | on the game state; **not `final`**: the rules (`GetRules`, `SetRules`), the server's checks and the live pings (`CreatePing`, `RemovePing`, `ClearPings`, `GetLivePings`), who sees what (`CanPlayerSeePing`, `RefreshVisibility`); `FindMatchComponent` |
| `UCKPingControllerComponent` (`CKPingControllerComponent.h`) | on the player controller: what a client may ask the server (`PingAtCrosshair`, `RequestPing`, `RequestRemovePing`), the pings the player sees (`GetVisiblePings`, `SeesPing`), events; `FindControllerComponent`, `FindLocalControllerComponent` |
| `UCKPingInputComponent` (`CKPingInputComponent.h`) | on a local controller: ping key, wheel, pointer and stick turned into pings (`BeginPingInput`, `EndPingInput`, `OpenWheel`, `CloseWheel`, `AddWheelPointerInput`, `SetWheelStick`), wheel state (`IsWheelOpen`, `GetSelectedSector`) |
| `UCKPingRulesData`, `UCKPingTypeData` | the data (both `final`): the rules of a match and one kind of ping |
| `UCKPingSettings` (`CKPingSettings.h`, *Project Settings > CoopKit Ping*) | default rules, default input assets, HUD layer and widget classes |
| `UCKPingHudWidget`, `UCKPingMarkerWidget`, `UCKPingWheelWidget` (`UI/`) | the HUD layer, one marker and the wheel; Blueprintable, replaceable in the settings |
| `FCKPingEntry` (`CKPingTypes.h`) | one ping: `PingId`, `Author`, `Type`, `Location`, `Target`, `CreatedServerTime`, `ExpireServerTime`; `ECKPingRejectReason` says why a ping was refused |
| `CKPing::Tags` (`CKPingGameplayTags.h`) | the plugin's Gameplay Tag `CK.Ping.Block.Ping` |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnPingCreated(Ping)`, `OnPingEnded(PingId)` of the match component | server | effects on pinged actors, score, a voice line, saving who pinged what |
| `OnPingAdded(Ping)`, `OnPingRemoved(PingId)`, `OnPingRejected(Reason)` of the controller component | the owning client (and the listen-server host) | a sound or a shake for a new ping, a message when a ping is refused |
| `OnWheelOpenChanged(bOpen)`, `OnWheelSectorChanged(Sector)` of the input component | the local machine | dim the screen while the wheel is open, a click on each sector |
| `UCKPingMatchComponent::CanPlayerSeePing` (`BlueprintNativeEvent`) | server, per viewer | squads, spectators, a stealth mode |
| `CK.Ping.Block.Ping` on the controller, pawn or player state, loose or from an effect | server | forbid a player's pings for a while |
| `UCKPingMarkerWidget` and `UCKPingWheelWidget` Blueprint events (`OnPingAssigned`, `OnMarkerUpdated`, `OnTypesSet`, `OnSectorSelected`) | the local machine | another look of a marker or the wheel without C++ |

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** The ping rules card lists their ping types by itself. A data asset of your own gets the Data Asset editor, not a card: refer from it to the plugin's data where needed (`TObjectPtr<UCKPingTypeData>`).
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitPing/` (see "Editing the plugin" in `rules.md`).
- The gameplay tag `CK.Ping.Block.Ping` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- The replicated list of pings on the controller component (`FCKPingList`): clients derive the events from it.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitPingEditor/Private/`: `CKPingWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKPingDataValidation.cpp`, `SCKPingCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards; `CKPingRulesDataFactory.cpp` is New Ping Rules), `CKPingDataSummary.cpp` (Summary text). The plugin's own tests in `Source/CoopKitPingTests/Private/` show how to make its data in code and test it.
