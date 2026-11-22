# CoopKitRevive: extension map

Plugin module names: `CoopKitRevive` (runtime), `CoopKitReviveEditor` (editor only). Add `"CoopKitRevive"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Add `"GameplayAbilities"` and `"GameplayTags"` when your code uses Gameplay Events, effects or tags. Headers are included by plain name: `#include "CKReviveBodyComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (here: the body data of the rules) that the card shows with Open, Replace and Create.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Revive Rules | `UCKReviveRulesData` | `CKReviveRulesData.h` | `ReviveRules` | none | none | no (`final`) | Match rules of CoopKit Revive: what happens after a death (death camera, spectating, respawn), what a team wipe does and which components the match adds by itself. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

The part of revive rules is listed by the plugin, not by `CoopKitPart` metadata: **Body Data** (`BodyData`, the body data of the bodies the match adds; empty: *Body Data* of *Project Settings > CoopKit Revive*). The **New Revive Rules** wizard starts from the plugin's defaults or other rules and, with *Create Body Data*, makes an own body data `DA_ReviveBody_<Name>` in the rules' folder; otherwise the rules use the body data picked where it is (a shared part). The card's **Create** makes `DA_ReviveBody_<Name>` too.

The body data `UCKReviveBodyData` (`CKReviveBodyData.h`) is a data asset with the same data window but is not a hub entity: create it in *Add > CoopKit > Revive Body Data*, or let the wizard make it. A pawn's own body data goes on its **CKRevive Body Component** (*Body Data*).

## Classes you will meet

| Class | Role |
|---|---|
| `UCKReviveBodyComponent` (`CKReviveBodyComponent.h`) | on every pawn that can go down (players and AI): state (`GetState`, `IsDowned`, `GetReviveProgress`, `GetBleedOutSecondsLeft`, `GetReviver`), server API (`Down`, `Kill`, `BeginRevive`, `StopRevive`, `ReviveNow`, `ResetToAlive`, `RestoreDowned`, `SetBodyData`), events; `FindBodyComponent(Actor)` |
| `UCKRevivePlayerComponent` (`CKRevivePlayerComponent.h`) | on the player state: the player's life between bodies (`GetLifeState`, `GetRespawnsLeft`, `GetSpectatedPawn`), events; `FindPlayerComponent`, `FindLocalPlayerComponent` |
| `UCKReviveMatchComponent` (`CKReviveMatchComponent.h`) | on the game state: the rules (`GetRules`, `SetRules`), respawn (`RespawnPlayer`, `RespawnAllPlayers`), checkpoints, team wipe; adds the other components to players when the rules say so; `FindMatchComponent` |
| `UCKReviveControllerComponent` (`CKReviveControllerComponent.h`) | on the player controller: what a client may ask the server (`RequestRevive`, `StopRevive`, `GiveUp`, `SpectateNext`, `SpectatePrevious`); `FindLocalControllerComponent` |
| `ICKRevivable` (`CKRevivable.h`) | optional interface of a pawn: `CanBeDowned(DamageInstigator)`, `CanBeRevivedBy(Reviver)`, asked on the server |
| `ACKReviveCheckpoint` (`CKReviveCheckpoint.h`) | a respawn point with an order; a higher order activates when a player overlaps it, or with `Activate` (server) |
| `UCKReviveBodyData`, `UCKReviveRulesData` | the data (both `final`): body tuning and match rules |
| `UCKReviveSettings` (`CKReviveSettings.h`, *Project Settings > CoopKit Revive*) | default body data and default rules |
| `CKRevive::Tags` (`CKReviveGameplayTags.h`) | the plugin's Gameplay Tags: `CK.Revive.State.*`, `CK.Revive.Event.*`, `CK.Revive.Block.*` |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnDowned(Body, Instigator)`, `OnRevived(Body, Reviver)`, `OnDied(Body, Killer)` of the body component | every machine; gameplay changes on the server only | health back after a revive (`GetBodyData()->GetRevivedHealthFraction()`), a boost, a penalty, a score |
| `OnReviveStarted`, `OnReviveStopped`, `OnStateChanged` of the body component | every machine | revive UI, voice lines, animations |
| `OnLifeStateChanged`, `OnRespawned`, `OnSpectatedPawnChanged` of the player component | every machine | death screen, spectator UI, gear on respawn (server) |
| `OnCheckpointActivated`, `OnTeamWipedChanged` of the match component | every machine | checkpoint messages, a game over or a level restart |
| Gameplay Events `CK.Revive.Event.Downed`, `.Revived` (magnitude: health fraction), `.Died`, `.Respawned`, `.ReviveStarted`, `.ReviveStopped` | server, to the pawn's Ability System Component | abilities and effects of your game triggered by the revive loop |
| `ICKRevivable::CanBeDowned`, `CanBeRevivedBy` | server, before a down or a revive | bosses, pits, finishing moves, team rules |
| `CK.Revive.Block.Down`, `.Revive` (pawn), `.Respawn` (player state), loose or from an effect | server | forbid a down, a revive or a respawn for a while |

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** The revive rules card lists their body data by itself. A data asset of your own gets the Data Asset editor, not a card: refer from it to the plugin's data where needed (`TObjectPtr<UCKReviveBodyData>`).
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitRevive/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Revive.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- The replicated state of the components (`FCKReviveBodyStatus` and the player status): clients derive the events from it.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitReviveEditor/Private/`: `CKReviveWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKReviveDataValidation.cpp`, `SCKReviveCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards; `CKReviveRulesDataFactory.cpp` is New Revive Rules), `CKReviveDataSummary.cpp` (Summary text). The plugin's own tests in `Source/CoopKitReviveTests/Private/` show how to make its data in code and test it.
