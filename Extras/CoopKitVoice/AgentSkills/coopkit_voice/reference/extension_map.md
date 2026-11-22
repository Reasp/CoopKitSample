# CoopKitVoice: extension map

Plugin module names: `CoopKitVoice` (runtime), `CoopKitVoiceEditor` (editor only). Add `"CoopKitVoice"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` only if your code uses tags. Headers are included by plain name: `#include "CKVoiceProfileData.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (an icon, a curve, a sound) that the card shows with Open and Replace.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Voice Profile | `UCKVoiceProfileData` | `CKVoiceProfileData.h` | `VoiceProfiles` | `EntityId` | TalkInput (`TalkInput`, Create button); RadioInput (`RadioInput`, Create button); RadioClickSound (`RadioClickSound`); AttenuationOverride (`AttenuationOverride`); RadioEffectChain (`RadioEffectChain`) | no (`final`) | A Voice Profile: who hears whom and how in a world that uses it. |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

## Other classes

| Class | Role |
|---|---|
| `UCKVoicePlayerComponent` (`CKVoicePlayerComponent.h`) | On every player state: team, voice tags, the channel the player talks on; Start Talking, Stop Talking, Request Transmit, Start Test Tone. |
| `UCKVoiceSubsystem` (`CKVoiceSubsystem.h`) | The voice in a world: the active profile, the server's routing (Get Route), the client's mix (Get Mix), a player's own volume and mute. |
| `CKVoice::Rules` (`CKVoiceRules.h`) | The rules as pure functions: `ResolvePath`, `ProximityGain`, `ValidateTransmit`. |
| `FCKVoiceProfile`, `FCKVoiceChannel`, `FCKVoiceRule` (`CKVoiceTypes.h`) | The content of a Voice Profile. |
| `CKVoice::Tags` (`CKVoiceGameplayTags.h`) | `CK.Voice.Channel.*`, `CK.Voice.Item.Radio`, `CK.Voice.State.*`, `CK.Voice.Reject.*`. |
| `CKVoice::ProjectCheck` (`CKVoiceProjectCheck.h`) | The project's voice settings and their fix. |
| `UCKVoiceAgentOps` (editor) | Presets, route preview, the project check for agents. |

| Hook | Runs | Use for |
|---|---|---|
| `IGameplayTagAssetInterface::GetOwnedGameplayTags` on the player state, the pawn or their components | server, every routing tick (5 times a second) | the game state the rules read |
| `IGenericTeamAgentInterface::GetGenericTeamId` on the pawn or controller | server and clients | teams of team channels and Same Team Only rules |
| `UCKVoicePlayerComponent::OnTransmitResult` | server and owning client | a refused radio (reason tag) |
| `UCKVoicePlayerComponent::OnVoiceStateChanged` | every machine | the team, the tags or the channel changed |
| `ICKVoiceOutput` (`CKVoiceOutput.h`) | client | another voice backend for the mix (1.1) |

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

- Anything under `Plugins/CoopKitVoice/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Voice.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for other platforms (`PlatformAllowList` in `CoopKitVoice.uplugin`).

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitVoiceEditor/Private/`: `CKVoiceWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKVoiceDataValidation.cpp`, `SCKVoiceCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards), the `*Summary.cpp` files (Summary text). The plugin's own tests in `Source/CoopKitVoiceTests/Private/` show how to make its data in code and test it.
