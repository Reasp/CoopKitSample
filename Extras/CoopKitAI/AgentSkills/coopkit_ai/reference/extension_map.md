# CoopKit AI: extension map

Plugin module names: `CoopKitAI` (runtime), `CoopKitAIEditor` (editor only). Add `"CoopKitAI"` to the dependency lists of your project module's `Build.cs`, `"GameplayTags"` if your code uses tags, and `"AIModule"` if it uses AI controllers or teams. Headers are included by plain name: `#include "CKAIThreatComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Director Rules | `UCKAIDirectorData` | `CKAIDirectorData.h` | `DirectorRules` | none | none | no (`final`) | Rules of the AI director for a match: how often enemies re-evaluate threat, how long attack tokens last, and the difficulty steps by player count (attackers per player, overall attackers, multipliers for your damage, health and spawn code). |
| Pacing Profile | `UCKAIPacingData` | `CKAIPacingData.h` | `PacingProfiles` | none | none | no (`final`) | Pacing profile of the horde director: what makes the players tense (damage, enemies after them, contract tags such as downed, your Tension Source code) and how the pacing phases follow the tension - enemies come during Build Up until the peak, the pressure holds, fades, and the players get a break. |
| Threat Profile | `UCKAIProfileData` | `CKAIProfileData.h` | `ThreatProfiles` | none | none | no (`final`) | Threat profile of an enemy type: what makes the enemy angry with a player, how fast it calms down and forgets, how readily it switches targets, who it may target. |
| Wave Set | `UCKAIWaveData` | `CKAIWaveData.h` | `WaveSets` | none | Pacing (`Pacing`, Create button) | no (`final`) | A set of waves and horde events: which enemies come, how many for the budget and the player count, from which spawn points, in groups of how many, with pauses between waves; named horde events that your code (or the pacing during Build Up) starts on top of the waves; where enemies may appear (out of the players' sight, on the navigation mesh). |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it. The **New Threat Profile**, **New Director Rules**, **New Pacing Profile** and **New Wave Set** wizards (*Add > CoopKit > AI Threat Profile* / *AI Director Data* / *AI Pacing Profile* / *AI Wave Set*, or the hub) start from the plugin's defaults or from another asset of the type. The game picks a profile per enemy with `SetProfile` of the threat component (or its *Profile* field), the rules with `SetData` of the director (or its *Data* field); without them *Project Settings > Plugins > CoopKit AI* gives *Default Profile* and *Default Director Data*.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKAIThreatComponent` (*CKAI Threat Component*, `CKAIThreatComponent.h`) | on the enemy pawn: threat table (server), target choice, attack token requests; replicates the current target and the token. `Blueprintable`, override `IsValidTarget`. `FindThreatComponent(Actor)` |
| `UCKAIDirectorComponent` (*CKAI Director Component*, `CKAIDirectorComponent.h`) | on the game state, server only: attack tokens, difficulty step by player count, stimuli. `FindDirector(WorldContext)`, `GetScaling`, `ReportStimulus`, `SetPlayerCountOverride`, `SetData` |
| `UCKAIProfileData` | threat profile of an enemy type (`final`): target filter, untargetable tags, switch threshold, threat sources, forgetting, whether it needs a token |
| `UCKAIDirectorData` | director rules (`final`): evaluation interval, token hold time and cooldown, difficulty steps `FCKAIScalingStep` (`CKAITypes.h`: `Players`, `MaxAttackersPerTarget`, `MaxAttackersTotal`, `DamageMultiplier`, `HealthMultiplier`, `SpawnBudgetMultiplier`) |
| `UCKAIHordeComponent` (*CKAI Horde Component*, `CKAIHordeComponent.h`) | on the game state next to the director (or on an arena actor with *Area Radius*): players' tension, pacing phase, waves and horde events of a wave set, spawning out of sight on the navigation mesh. `FindHorde(WorldContext)`, `StartWaves`, `StopWaves`, `StartNextWaveNow`, `StartEvent`, `StopEvent`, `ReportPlayerDamage`, `AddTension`, `ReportEnemyDefeated`, `GetHordeState` (replicated) |
| `UCKAIPacingData` | pacing profile (`final`): tension sources (damage, enemies after the player, contract tags by name), decay, Build Up / Peak / Peak Fade / Relax thresholds and times, spawn scale per phase |
| `UCKAIWaveData` | wave set (`final`): waves `FCKAIWave` and named events (`CKAIHordeTypes.h`: budget, `FCKAIWaveEnemy` kinds with cost, fixed count, fill weight, *Min Players*, *Outside Budget*; spawn group, group size and interval, pause, repeat), *Max Alive*, `FCKAISpawnRules`, part *Pacing* |
| `ACKAISpawnPoint` (*CK AI Spawn Point*) | a place where a wave's enemies may appear; its *Group* is the wave's *Spawn Group* |
| `ICKAITensionSource` (`CKAITensionSource.h`) | your own tension per second (ammo, health) on the player's pawn, player state or their components |
| `UCKAISettings` (`CKAISettings.h`, *Project Settings > Plugins > CoopKit AI*) | default profile, default director rules, *Contract Untargetable Tag Names* (by default the downed and dead states of CoopKit Revive), *Contract Defeated Tag Names* (an enemy with the tag leaves the horde's count; by default CoopKit Combat's out of health) |
| StateTree tasks and conditions (`StateTree/CKAIStateTreeTasks.h`, `StateTree/CKAIStateTreeConditions.h`) | *CK AI Threat Target*, *CK AI Hold Attack Token*, *CK AI Move To Threat Target*, *CK AI Wait*; *CK AI Has Threat Target*, *CK AI Attack Token Available*, *CK AI Actor Has Tag* |
| EQS tests (`EQS/CKAIEnvQueryTests.h`) | *CK AI Threat*, *CK AI Attack Slots*, *CK AI Actor Tags*; context *CK AI Threat Target* |
| `CKAI::Tags` (`CKAIGameplayTags.h`) | `CK.AI.State.Attacking`, `CK.AI.Block.Target`, `CK.AI.Block.Attack` |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `UCKAIThreatComponent::IsValidTarget(Candidate)` (`BlueprintNativeEvent`, override `IsValidTarget_Implementation` in C++) | server, at every evaluation and before threat is taken | your own "who is a foe" rule: distance from home, a quest state, a faction field; call the parent first to keep alive, filter and team checks |
| `OnTargetChanged(NewTarget, OldTarget)` of the threat component | server and every client | a roar, a look-at, an aggro marker, a speed change (gameplay on the server only) |
| `OnAttackTokenChanged(bHoldsToken)` of the threat component; tag `CK.AI.State.Attacking` | server and every client | an attack telegraph, an animation; start the attack when the token comes |
| `OnScalingChanged(Step, PlayerCount)` of the director; `GetScaling()` | server | apply health, damage and spawn budget multipliers in your own code |
| `ReportStimulus(Location, Instigator, Radius, Threat)`, `AddThreat`, `ReportDamage`, `ForceTarget` | server, called by your code | noise, alarms, healing aggro, taunts |
| tags `CK.AI.Block.Target` (on a possible target), `CK.AI.Block.Attack` (on an enemy), loose or from a Gameplay Effect | server, at every evaluation | no target / no attack for a while |
| `OnEnemySpawned(Enemy, Source)` of the horde | server | apply the difficulty's health and damage multipliers to a new enemy, give it a team or loot |
| `OnWaveStarted`, `OnWaveCompleted`, `OnAllWavesCompleted`, `OnPacingPhaseChanged`, `OnHordeEventStarted` of the horde | server | open the next door, a music change, a mission phase |
| `OnHordeStateChanged` of the horde, `GetHordeState()` | server and every client | a HUD line "Wave 2 of 4", a pause countdown |
| `ICKAITensionSource::GetTensionPerSecond` (`BlueprintNativeEvent`) | server, every horde update | low ammo, low health, darkness raise the player's tension |

## Pacing and waves

The horde component (`UCKAIHordeComponent`) runs pacing and waves from data; the project adds code only where its own game knows more:

- **Data, not code:** waves, events, budgets, enemy kinds and spawn rules are a **Wave Set** (`UCKAIWaveData`), the tension sources and phases a **Pacing Profile** (`UCKAIPacingData`); both are edited in the CoopKit Hub. Do not add pacing fields to `UCKAIDirectorData` (it is `final`).
- **Add the component** next to the director on the game state, set its *Wave Data* (and *Auto Start*, or call `StartWaves` from the mission code). Put *CK AI Spawn Point* actors in the level with the *Group* of the waves; a wave without a group spawns around the players on the navigation mesh.
- **Scale:** the budget of a wave is multiplied by the director's `SpawnBudgetMultiplier` for the counted players when the wave starts; enemy kinds with *Min Players* come only with enough players. Health and damage multipliers stay the project's: apply them on `OnEnemySpawned`.
- **Tension:** engine damage of player pawns, enemies whose target is the player and the contract tags of the profile count by themselves. Damage that bypasses the engine (GAS, CoopKit Combat): call `ReportPlayerDamage`. Anything else (ammunition, health): implement `ICKAITensionSource` on the pawn, the player state or a component.
- **Defeat:** an enemy leaves the count when its pawn is destroyed, when it publishes a tag of *Contract Defeated Tag Names*, or when the project calls `ReportEnemyDefeated` (a death animation that keeps the pawn).
- **Mission flow:** `OnWaveCompleted` / `OnAllWavesCompleted` open doors and move phases; `StartEvent(Name)` runs a named horde event (an alarm, a timer's final rush), `StopEvent` ends a repeating one; `StartNextWaveNow` skips a pause.

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). In a subclass, a component or a data asset in your project module.
2. **Validation.** `IsDataValid` (editor-only, `#if WITH_EDITOR`) of a data subclass: call `Super::IsDataValid(Context)` first, then report data the code cannot use (negative where it must not be, empty required reference) with `Context.AddError`. Warnings for suspicious values: `Context.AddWarning`. The card's Validation tab and the hub's Check Project show them.
3. **Where the user sees it.** A property of a data asset shows in the card's Properties tab. Tell the user which asset to open and which field to set.
4. **Card part.** Only for a *reference to another asset* (icon, curve, sound) in a data asset of the project: a `TSoftObjectPtr` property marked with `meta = (CoopKitPart = "Label")`, and `CoopKitPartCreate = "true"` to offer a Create button. The plugin's own two types have no parts.
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the pawn or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitAI/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.AI.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitAIEditor/Private/`: `CKAIWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKAIDataValidation.cpp`, `SCKAICard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitAITests/Private/` show how to make its data in code and test it.
