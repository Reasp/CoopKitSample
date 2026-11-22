# CoopKitSupport: extension map

Plugin module names: `CoopKitSupport` (runtime), `CoopKitSupportEditor` (editor only). Add `"CoopKitSupport"` to the dependency lists of your project module's `Build.cs`, and `"GameplayAbilities"` and `"GameplayTags"` when your code uses attributes, effects or tags. Headers are included by plain name: `#include "CKSupportComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Support Ability | `UCKSupportAbilityData` | `CKSupportAbilityData.h` | `SupportAbilities` | `EntityId` | Icon (`Icon`); Animation (`Animation`); Input (`Input`, Create button) | no (`final`) | One support ability, whole, in one asset: what it does (heal, shield, buff, aura), who it reaches (self, the ally the player points at, every ally around), how strong and how long, what it costs and how long it cools down, how it feeds the team's shared ultimate charge, which synergy mark it puts or consumes, and its look and input: the Gameplay Cue, the animation, the icon and the input action. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Classes an agent meets

| Class | Role |
|---|---|
| `UCKSupportAbilityData` (`CKSupportAbilityData.h`) | one ability: effect (`Heal`, `Shield`, `Buff`, `Aura`), targeting (`Self`, `Ally`, `AlliesAround`), range, soft lock angle, radius, amount, duration, cost attribute and amount, cooldown, ultimate gain and cost, prime and synergy tags, cue tag, icon, animation, input action; `GetDataProblems` |
| `UCKSupportComponent` (`CKSupportComponent.h`) | on the player state, a pawn or an NPC ally: the ability set, `RequestCast` (predicted on the owner, checked on the server), `CastOnServer`, `FindSoftLockTarget`, cooldowns, statuses (shield, buff, mark, aura) published as Gameplay Tags, `AbsorbDamage`, `BindAbilityInputs`, team id |
| `ACKSupportTeamState` (`CKSupportTeamState.h`) | one replicated actor per world: the shared ultimate charge of each team and who added how much; `GetCharge`, `GetTeamCharge`, server `AddCharge`, `SpendCharge`, `SetCharge` |
| `UCKSupportAttributeSet` (`CKSupportAttributeSet.h`) | simple attributes for a project without its own: Health, MaxHealth, Energy, MaxEnergy, IncomingDamage |
| `UCKSupportGameplayAbility` (`CKSupportGameplayAbility.h`) | a Gameplay Ability that casts one support ability through the support component (for input through the Ability System Component) |
| `UCKSupportSubsystem` (`CKSupportSubsystem.h`) | every support component of a world (the soft lock and area abilities look here) |
| `ACKSupportTrainingDummy` (`CKSupportTrainingDummy.h`) | a replicated ally post with health and a support component, for demos and tests |
| `CKSupport::Rules` (`CKSupportRules.h`) | pure functions: result tags, player keys, range with lag tolerance, soft lock score, marks a caster may consume, synergy amount, shield merge and absorption, charge, heal cap, the request limiter |
| `CKSupport::Tags` (`CKSupportGameplayTags.h`) | `CK.Support.Reject.*` (one per `ECKSupportResult`), `CK.Support.Status.Shielded`, `.Aura`, `CK.Support.SetByCaller.Amount`, `.Duration`, demo tags `CK.Support.Buff.Rally`, `CK.Support.Prime.Warded` |

## Hooks

| Hook | Runs | Use for |
|---|---|---|
| `UCKSupportComponent::OnCastResult` | server and owning client | the server's answer with the reason tag of a refusal |
| `UCKSupportComponent::OnCastPredicted` | owning client | instant feedback before the server answers |
| `UCKSupportComponent::OnCast` | every machine (unreliable) | sounds, numbers, other players' feedback |
| `UCKSupportComponent::OnStatusesChanged` | every machine | shield and buff icons |
| `UCKSupportComponent::OnSynergy` | every machine | "combo" feedback, achievements |
| `ACKSupportTeamState::OnChargeChanged` | every machine | the team charge meter |
| Gameplay Cue (*Cue Tag* of the ability) | every machine, on the target's Ability System Component | particles and sounds on the target |
| *Buff Effect* (a Gameplay Effect class) | server, on each buffed target | what a buff does to the project's attributes |

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

- Anything under `Plugins/CoopKitSupport/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Support.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitSupport.uplugin`).

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitSupportEditor/Private/`: `CKSupportWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKSupportDataValidation.cpp`, `SCKSupportCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitSupportTests/Private/` show how to make its data in code and test it.
