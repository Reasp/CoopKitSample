# CoopKit Heroes: extension map

Plugin module names: `CoopKitHeroes` (runtime), `CoopKitHeroesEditor` (editor only). Add `"CoopKitHeroes"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"`/`"GameplayAbilities"` only if your code uses them. Headers are included by plain name: `#include "CKHeroModule.h"`.

## Entity type (what the CoopKit Hub and the hero card know)

Heroes are the one entity type of this plugin. `UCKHeroRosterData` (the heroes players may choose, the uniqueness rule, the default hero) is a plain data asset with its own editor window, not a hub entity.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Hero | `UCKHeroData` | `CKHeroData.h` | `Heroes` | `HeroId` | none | yes | Everything about one hero in one asset: name and role, look and skins, Gameplay Ability System content and modules for parts of your game the plugin does not know. |
<!-- coopkit:entities:end -->

`UCKHeroData` is a `UPrimaryDataAsset` with the primary asset type `CKHero`: id, name, role, pawn class, mesh, animation, materials, skins, abilities, effects, granted tags, **modules**.

Other classes you will meet:

| Class | Role |
|---|---|
| `UCKHeroModule` (abstract, Blueprintable, instanced) | extension point: a part of a hero for something the plugin does not know |
| `UCKHeroComponent` | on the player state: choice, roster, lock; applies the hero to each pawn of the player on the server; replicated |
| `ECKHeroUniqueness`, `FCKHeroSkin`, `FCKHeroAbility`, `FCKHeroEffect` | types in `CKHeroesTypes.h` |
| `CKHeroes::` functions of `CKHeroesRules.h` | the server's selection rules (pure functions) |
| `ICKHeroSelectionGate` | optional interface on game mode or game state to refuse a choice (`CK.Heroes.Reject.Denied`) |

## The hero module hooks (`CKHeroModule.h`)

| Hook | Runs | Use for |
|---|---|---|
| `ApplyToPawn(Pawn, Player)` | server, when the player starts playing the hero (after abilities, effects and tags) | gameplay parameters: set a value on a component, apply a Gameplay Effect, give items |
| `RemoveFromPawn(Pawn, Player)` | server, when the player stops playing the hero | undo what `ApplyToPawn` did; find it again on the pawn, a module holds no per-player state |
| `ApplyAppearance(Pawn, Skin)` | every machine, after the look was applied | props, voice sets, trails |
| `GetSummary()` | editor | the line shown on the hero card Summary tab, e.g. "Regeneration: 2.5 per second" |
| `CollectErrors(OutErrors)` | editor | text shown in the Validation tab and by the hub's Check Project |

A module is shared by every player of that hero: it keeps no state, and anything it changes it must be able to find again on the pawn.

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a module or a subclass in your project module.
2. **Validation.** `CollectErrors` of a module, or `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass. Report data the code cannot use (negative where it must not be, empty required reference). Warnings for suspicious values: `Context.AddWarning`.
3. **Card Summary.** `GetSummary` of the module returns a short readable line with the value. The card's Summary tab lists the summaries of all modules.
4. **Card part.** Only for a *reference to another asset* (icon, curve, sound): mark the soft pointer with `meta = (CoopKitPart = "Label")`, and `CoopKitPartCreate = "true"` to offer a Create button. The hero card lists parts of `UCKHeroData` only; a part inside a module is edited in the module's own Details row.
5. **CSV.** The plugin's CSV export and import cover the properties of `UCKHeroData` itself (editable, not instanced). A module's fields are instanced, so they are not in the CSV; say so in your report when the user asked for CSV.
6. **Runtime.** Server: apply in `ApplyToPawn` and undo in `RemoveFromPawn`. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics: `ApplyAppearance`.
7. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic; modules can also be subclassed in Blueprint (the base class is Blueprintable).
8. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitHeroes/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Heroes.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- `UCKHeroComponent` internals and the selection rules: extend through `ICKHeroSelectionGate`, not by patching.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitHeroesEditor/Private/`: `CKHeroesWorkspace.cpp` (parts of a hero, rename, duplicate, delete), `CKHeroesDataSummaries.cpp` (Summary text), `CKHeroesDataValidation.cpp`, `CKHeroesCsv.cpp`, `SCKHeroCard.cpp` (card tabs), `CKHeroDataFactory.cpp` (the New Hero wizard). The plugin's own tests in `Source/CoopKitHeroesTests/Private/` show how to make heroes in code and test them.
