# CoopKitDraft: extension map

Plugin module names: `CoopKitDraft` (runtime), `CoopKitDraftEditor` (editor only). Add `"CoopKitDraft"` to the dependency lists of your project module's `Build.cs`, and `"GameplayTags"` when your code uses tags (a rule that refuses with a tag of its own does). Headers are included by plain name: `#include "CKDraftComponent.h"`.

## Entity types (what the CoopKit Hub and the cards know)

An entity is a data asset the CoopKit Hub lists (*Tools > CoopKit Hub*): it has a "New ..." wizard and a card with Summary, Parts and Validation tabs. A part is a reference to another asset (here: the hero pool of the mode) that the card shows with Open, Replace and Create.

<!-- coopkit:entities:begin -->
| Entity | Class | Header | Asset folder | Id property | Parts (`CoopKitPart`) | Subclass in project | Purpose |
|---|---|---|---|---|---|---|---|
| Draft Mode | `UCKDraftModeData` | `CKDraftModeData.h` | `DraftMode` | none | none | no (`final`) | A hero draft mode: the hero pool, the teams and one rule object per axis of the draft (order, pool, validity, timeout). |
<!-- coopkit:entities:end -->

A class marked `final` cannot be subclassed: extend it with a component or with a data asset of your own that refers to it.

The part of a draft mode is listed by the plugin, not by `CoopKitPart` metadata: **Hero Pool** (`HeroPool`, the heroes of the draft; a draft started with its own pool, `StartDraft(Mode, HeroPoolOverride)`, does not need one). The **New Draft Mode** wizard starts from a plain mode or another mode (its rules are copied, not shared) and, with *Create Hero Pool*, makes an own hero pool `DA_DraftHeroPool_<Name>` in the mode's folder, with the heroes of the pool you pick, of the mode you start from or of the plugin's demo pool; otherwise the mode uses the pool picked where it is (a shared part). The card's **Create** makes `DA_DraftHeroPool_<Name>` too.

The hero pool `UCKDraftHeroPoolData` (`CKDraftHeroPoolData.h`) is a data asset with the same data window but is not a hub entity: create it in *Add > CoopKit > Draft Hero Pool*, or let the wizard make it. A hero (`FCKDraftHero`) has a unique `Id`, a `DisplayName`, a `Role` tag, an `Icon` and a `PawnClass`; the plugin never spawns the pawn.

## Classes you will meet

| Class | Role |
|---|---|
| `UCKDraftComponent` (`CKDraftComponent.h`) | on the game state: runs the draft on the server (`StartDraft`, `StartDraftWithTeams`, `CancelDraft`, `SubmitAction`, `OfferSwap`, `SetRandomSeed`) and replicates the state; readable everywhere (`GetState`, `GetPhase`, `GetCurrentTurn`, `GetPickedHero`, `GetOfferedHeroes`, `GetTurnSecondsLeft`, `PreviewAction`), events; `FindDraftComponent(WorldContext)` |
| `UCKDraftPlayerComponent` (`CKDraftPlayerComponent.h`) | on every player controller, added by the draft on the server: what a player asks (`RequestPick`, `RequestBan`, `HoverHero`, `RequestSwap`, `AnswerSwap`), the player's private view of hidden turns, `OnActionRejected`; `FindLocalPlayerComponent(WorldContext)` |
| `UCKDraftModeData`, `UCKDraftHeroPoolData` | the data (both `final`): the mode with its rules and the hero pool |
| `UCKDraftRule` and its bases `UCKDraftOrderRule`, `UCKDraftPoolRule`, `UCKDraftSelectionRule`, `UCKDraftAutoPickRule`, `UCKDraftVisibilityRule`, `UCKDraftReserveRule`, `UCKDraftSwapRule`, `UCKDraftBanVoteRule` (`CKDraftRules.h`) | one instanced rule object per axis of a mode; subclass a base in C++ or Blueprint for your own; the ready ones (`UCKDraftTurnOrderRule`, `UCKDraftUniqueHeroRule`, `UCKDraftRoleLimitRule`, ...) are `final` |
| `UCKDraftLibrary` (`CKDraftLibrary.h`) | Blueprint helpers for a draft screen: `RequestLocalPick`, `RequestLocalBan`, `HoverLocalHero`, `GetHeroDisplayName`, `GetRejectReasonText` |
| `FCKDraftState`, `FCKDraftParticipant`, `FCKDraftTurn`, `FCKDraftActionRequest` (`CKDraftTypes.h`) | the replicated state and what the rules read |
| `CKDraft::Tags` (`CKDraftGameplayTags.h`) | the plugin's Gameplay Tags: `CK.Draft.State.*` (on the game state's tag interface), `CK.Draft.Reject.*` (the reasons of a refusal) |

## Hooks: where your code runs

| Hook | Runs | Use for |
|---|---|---|
| `UCKDraftSelectionRule::CheckAction(State, Request)` | server, after the plugin's own checks | refuse a pick or ban with a reason tag (an empty tag allows it): unlocked heroes, role order, own limits |
| `UCKDraftOrderRule::BuildTurns(State, OutTurns)` | server, once at the start | the order of turns, bans before picks, team captains |
| `UCKDraftPoolRule::BuildPools(State, AllHeroes, Stream, OutSharedPool, OutChoices)` | server, once at the start | which heroes are on offer: a random share, personal choices, a rotation |
| `UCKDraftAutoPickRule::ChooseHero(State, Participant, Candidates, Stream)` | server, when a pick turn runs out | the timeout pick: a preferred list, the first free hero |
| `UCKDraftVisibilityRule::GetTurnVisibility`, `UCKDraftReserveRule::GetReserveSeconds`, `UsesReserve`, `UCKDraftSwapRule::CheckSwap`, `UCKDraftBanVoteRule::IsVoteTurn`, `ChooseBan` | server | blind turns, reserve time, swaps, team ban votes of your own kind |
| `OnPhaseChanged`, `OnTurnChanged` of the draft component | every machine | the draft screen: whose turn, phase banners |
| `OnHeroPicked`, `OnHeroBanned`, `OnHeroHovered`, `OnBanVoteChanged`, `OnSwapOffered`, `OnHeroesSwapped` | every machine, for what the machine may see | the draft screen, sounds, a pick log |
| `OnDraftCompleted` of the draft component | every machine | spawn the picked heroes (server), a loadout, the start of the match |
| `OnActionRejected` of the player component | the player's machine | show why a pick was refused (`GetRejectReasonText(Reason)`) |

A rule runs on the replicated state too, so a client can preview what the server will allow (`PreviewAction`): keep a rule a pure function of its properties and the state it is given.

## Where a new parameter goes (checklist)

For a parameter named `X`, decide each line; the first three are always needed.

1. **Property.** `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "...", meta = (ClampMin = "0"))` with a comment line (it becomes the tooltip). On a rule subclass, a component or a data asset in your project module.
2. **Validation.** For a rule: override `ValidateRule(Mode, OutErrors, OutWarnings)` and report settings the draft cannot use; the mode's `IsDataValid` calls it and the card's Validation tab and the hub's Check Project show it. For a data asset of your own: `IsDataValid` (editor-only, `#if WITH_EDITOR`), `Super::IsDataValid(Context)` first, then `Context.AddError` / `Context.AddWarning`.
3. **Where the user sees it.** A property of a rule shows in the mode card's Properties tab, inside the rule's slot (*Order Rule*, *Pool Rule*, *Selection Rules*, ...); pick the new rule class there. Tell the user which mode asset to open and which slot to set.
4. **Card part.** The mode card lists its hero pool by itself. A data asset of your own gets the Data Asset editor, not a card: refer from it to the plugin's data where needed (`TObjectPtr<UCKDraftHeroPoolData>`).
5. **Runtime.** Server: gameplay values change only where `HasAuthority()` is true. If clients need the value (a UI), put it on a replicated property of a component you own on the game state or player state; set it on the server and use `DOREPLIFETIME`. Cosmetics run on every machine.
6. **Blueprint access.** `BlueprintReadOnly` on the property, plus `UFUNCTION(BlueprintPure)` getters for logic.
7. **Test.** An automation test for the logic and for the validation (see `examples.md`).

## What not to touch

- Anything under `Plugins/CoopKitDraft/` (see "Editing the plugin" in `rules.md`).
- The gameplay tags `CK.Draft.*` and the hub contract names (`CoopKitEntity` metadata, tab `CoopKitHub`): other CoopKit plugins read them.
- The replicated draft state (`FCKDraftState`) and the private views of the player components: hidden picks and team votes reach each player only through them. Do not read the server's full state (`GetFullState`) into anything a client can see.

## Limits to tell the user

- Windows (Win64) only: the plugin's modules do not build for Mac or Linux.

## Where the editor pieces live (for reading, not editing)

`Source/CoopKitDraftEditor/Private/`: `CKDraftWorkspace.cpp` (parts of an entity, rename, duplicate, delete), `CKDraftDataValidation.cpp`, `SCKDraftCard.cpp` (card tabs), the `*Factory.cpp` files (the "New ..." wizards; `CKDraftModeDataFactory.cpp` is New Draft Mode), `CKDraftDataSummaries.cpp` (Summary text). The plugin's own tests in `Source/CoopKitDraftTests/Private/` show how to make its data in code and test it.
