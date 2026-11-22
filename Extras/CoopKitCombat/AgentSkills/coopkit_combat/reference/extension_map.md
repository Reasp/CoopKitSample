# CoopKitCombat: extension map

Plugin module names: `CoopKitCombat` (runtime), `CoopKitCombatEditor` (editor only). Add `"CoopKitCombat"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Add `"GameplayAbilities"` and `"GameplayTags"` when your code uses Gameplay Effects, events or tags. Headers are included by plain name: `#include "CKCombatMoveSetData.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (a montage, a Gameplay Cue notify) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Move Set | `UCKCombatMoveSetData` | `CKCombatMoveSetData.h` | `MoveSets` | none | none | yes | A combo of CoopKit Combat: the attacks in order with their timings, reach, damage and cost. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

The parts of a move set are listed per step of its combo, not by `CoopKitPart` metadata: **Step N Montage** (the step's `Montage`) and **Step N Hit Cue** (the Gameplay Cue notify asset that plays the step's `HitCue` tag; empty tag: `GameplayCue.CK.Combat.Hit`). A cue notify is never an own part of a move set: it is found by its tag, so renaming, duplicating or deleting a move set leaves it where it is. The **New Move Set** wizard starts from the built-in three-hit combo or another move set and makes an own montage `AM_<Name>_Step<N>` from each animation picked.

The fighter profile `UCKCombatProfileData` (`CKCombatProfileData.h`) is a data asset with the same card window but is not a hub entity: create it in *Add > CoopKit > Combat Profile*.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKCombatComponent` (`CKCombatComponent.h`) | on the fighter's pawn: actions (`Attack`, `StartBlock`, `StopBlock`, `Dodge`), server API (`ApplyDamage`, `RestoreHealth`, `ResetAttributes`, `SetProfile`), getters (`GetProfile`, `GetComboStep`, `GetAbilitySystem`, `GetActiveAttackAbility`, `ResolveMoveSet`), events; `FindCombatComponent(Actor)` works for a pawn, controller or player state |
| `UCKCombatMoveSetData` (`CKCombatMoveSetData.h`) | a combo: `GetSteps()`, `FindStep(Index)`, input buffer; subclass it for per-attack values of your own |
| `FCKCombatAttackStep` (`CKCombatTypes.h`) | one attack: duration, hit and combo windows, reach, arc, max targets, damage, stamina and energy cost, energy per hit, `Montage`, `HitCue`; owned by the plugin, do not add fields |
| `UCKCombatProfileData` (`CKCombatProfileData.h`) | a fighter: attributes, block, parry, dodge, its move set, granted abilities; subclass it for per-fighter values |
| `UCKCombatAttackAbility`, `UCKCombatBlockAbility`, `UCKCombatDodgeAbility` | the predicted abilities; a Blueprint child of the attack ability with its own *Move Set* is a weapon's combo |
| `UCKCombatSettings` (`CKCombatSettings.h`, *Project Settings > CoopKit Combat*) | attribute map, *Damage Effect*, *Resource Effect*, friendly fire, hit check slack, default profile |
| `UCKCombatDamageEffect`, `UCKCombatResourceEffect` (`CKCombatEffects.h`) | the default effects: damage reads `CK.Combat.SetByCaller.Damage`; costs read `.Stamina` and `.Energy` |
| `UCKCombatDebugCue` (`CKCombatDebugCue.h`) | placeholder Gameplay Cue notify (draws a shape); the demo cues `/CoopKitCombat/Cues/GC_CombatDemo*` derive from it |
| `CKCombat::Tags` (`CKCombatGameplayTags.h`) | the plugin's Gameplay Tags (`CK.Combat.*`, `GameplayCue.CK.Combat.*`) |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `OnDamageDealt(Combat, Target, Damage, Outcome)` | server, on the attacker's component, after block and armor | per-attack effects: knockback, an extra damage type, poise; the step is `Combat->GetComboStep()`, the combo `Combat->ResolveMoveSet(Combat->GetActiveAttackAbility())` |
| `OnDamageTaken(Combat, Attacker, Damage, Outcome)` | server, on the target's component | reactions of the target: flinch, a resistance that heals back |
| `OnHitRejected(Combat, Target, Result)` | server | logs and anti-cheat statistics |
| `OnOutOfHealth(Combat)` | server | down, death or respawn of your game |
| `OnActionStarted(Combat, Action, Step)` | every machine (the owner at prediction) | cosmetics: trails, voice lines |
| Gameplay Events `CK.Combat.Event.Hit`, `.Damaged`, `.Blocked`, `.Parried`, `.Dodged`, `.OutOfHealth` | server, to the attacker's or target's Ability System Component | abilities of your game triggered by combat |
| *Damage Effect* in the settings | server, every hit | your damage formula or damage type (an execution calculation, resistances) |
| Gameplay Cue tags (`HitCue` of a step) | the attacker at prediction, the others from the server | hit visuals and sounds per attack |
| `CK.Combat.Block.Attack`, `.Block`, `.Dodge`, `.Damage` (loose tags you add) | server and owner | forbid actions or hits (downed players, cutscenes) |

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** The move set card lists the montage and the hit cue of every step by itself. A *reference to another asset* you add in a subclass (an icon, a sound) shows as a part when it is a `TSoftObjectPtr` property marked with `meta = (CoopKitPart = "Label")`; `CoopKitPartCreate = "true"` offers a Create button.
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitCombat/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Combat.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- `FCKCombatAttackStep`: the server checks every hit and combo step against it; adding a field there means editing the plugin.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitCombatEditor/Private/`: `CKCombatWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKCombatDataValidation.cpp`, `SCKCombatCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards; `CKCombatMoveSetDataFactory.cpp` is New Move Set), `CKCombatDataSummary.cpp` (Summary text). The plugin's own tests in `Source/CoopKitCombatTests/Private/` show how to make its data in code and test it.
