# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's data classes and components are `final`: the examples extend it with a rule subclass (the rule bases are not `final`), a game state component and a widget of the project.

## 1. A rule the plugin does not have ("heroes the player has not unlocked cannot be picked")

`Source/Sleepwalker/Public/SleepwalkerLockedHeroRule.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKDraftRules.h"
#include "SleepwalkerLockedHeroRule.generated.h"

/** Draft rule: the listed heroes cannot be picked in this mode (heroes not finished yet, a boss). Add it to Selection Rules of a mode. */
UCLASS(meta = (DisplayName = "Locked Heroes"))
class SLEEPWALKER_API USleepwalkerLockedHeroRule : public UCKDraftSelectionRule
{
	GENERATED_BODY()

public:
	virtual FGameplayTag CheckAction_Implementation(const FCKDraftState& State, const FCKDraftActionRequest& Request) const override;
	virtual FText GetDescription_Implementation() const override;
	virtual void ValidateRule(const UCKDraftModeData& Mode, TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const override;

private:
	/** Ids of the heroes nobody may pick in this mode. Bans still work on them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locked Heroes", meta = (AllowPrivateAccess = "true"))
	TArray<FName> LockedHeroes;
};
```

`Source/Sleepwalker/Private/SleepwalkerLockedHeroRule.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerLockedHeroRule.h"
#include "CKDraftHeroPoolData.h"
#include "CKDraftModeData.h"
#include "NativeGameplayTags.h"

#define LOCTEXT_NAMESPACE "SleepwalkerLockedHeroRule"

namespace
{
	// The reason the player receives; a project tag next to the plugin's CK.Draft.Reject.* ones.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SleepwalkerRejectLocked, "Sleepwalker.Draft.Reject.Locked");
}

FGameplayTag USleepwalkerLockedHeroRule::CheckAction_Implementation(const FCKDraftState& State, const FCKDraftActionRequest& Request) const
{
	// Only picks are refused; an empty tag allows the request.
	if (Request.Action == ECKDraftAction::Pick && LockedHeroes.Contains(Request.Hero))
	{
		return TAG_SleepwalkerRejectLocked;
	}
	return FGameplayTag();
}

FText USleepwalkerLockedHeroRule::GetDescription_Implementation() const
{
	// One line for the Summary tab of the mode's card.
	return FText::Format(LOCTEXT("Description", "{0} heroes cannot be picked"), FText::AsNumber(LockedHeroes.Num()));
}

void USleepwalkerLockedHeroRule::ValidateRule(const UCKDraftModeData& Mode, TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	// Shown in the card's Validation tab and by the CoopKit Hub's Check Project.
	const UCKDraftHeroPoolData* Pool = Mode.GetHeroPool();
	for (const FName Hero : LockedHeroes)
	{
		FCKDraftHero Unused;
		if (Pool != nullptr && !Pool->FindHero(Hero, Unused))
		{
			OutWarnings.Add(FText::Format(LOCTEXT("UnknownHero", "Locked hero {0} is not in the mode's hero pool."), FText::FromName(Hero)));
		}
	}
}

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.AddRange(new string[] { "CoopKitDraft", "GameplayTags" });` (the class derives from a plugin class, so the dependency is public).

How the user uses it: open a draft mode in the CoopKit Hub, press **+** in *Selection Rules* and pick **Locked Heroes**, then fill its **Locked Heroes** list. A player who picks a locked hero receives `Sleepwalker.Draft.Reject.Locked`; show its text from `OnActionRejected` (example 4).

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerLockedHeroRuleTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CKDraftTypes.h"
#include "SleepwalkerLockedHeroRule.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerLockedHeroRuleTest, "Sleepwalker.Draft.LockedHeroRule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerLockedHeroRuleTest::RunTest(const FString& Parameters)
{
	USleepwalkerLockedHeroRule* Rule = NewObject<USleepwalkerLockedHeroRule>(GetTransientPackage());

	// The list is private: set it like a designer would in the Details panel, through reflection.
	FArrayProperty* Locked = FindFProperty<FArrayProperty>(Rule->GetClass(), TEXT("LockedHeroes"));
	if (!TestNotNull(TEXT("The property exists"), Locked))
	{
		return false;
	}
	TArray<FName> Heroes = { FName(TEXT("Boss")) };
	Locked->CopyCompleteValue(Locked->ContainerPtrToValuePtr<void>(Rule), &Heroes);

	// The rule reads the request only, so an empty state is enough.
	const FCKDraftState State;
	FCKDraftActionRequest Request;
	Request.Participant = 0;
	Request.Action = ECKDraftAction::Pick;

	Request.Hero = TEXT("Boss");
	TestTrue(TEXT("A locked hero is refused"), Rule->CheckAction(State, Request).IsValid());
	Request.Hero = TEXT("Knight");
	TestFalse(TEXT("Any other hero is allowed"), Rule->CheckAction(State, Request).IsValid());
	Request.Hero = TEXT("Boss");
	Request.Action = ECKDraftAction::Ban;
	TestFalse(TEXT("A ban of a locked hero is allowed"), Rule->CheckAction(State, Request).IsValid());
	return true;
}

#endif
```

For a rule that needs the state (the picks so far), build an `FCKDraftState` in the test: add `FCKDraftParticipant` entries (`Team`, `Hero`) to `State.Participants` and set `State.Pool`. The plugin's own tests (`Source/CoopKitDraftTests/Private/CKDraftCoreAxesTest.cpp`) show it.

### Testing a validation

A rule is checked through `ValidateRule(Mode, OutErrors, OutWarnings)`. In its test, build a mode in code, call the method with bad settings and check that `OutErrors` or `OutWarnings` is not empty. A data asset of your own (a `UPrimaryDataAsset` subclass with `IsDataValid`) is tested like this. Keep the test inside `#if WITH_EDITOR`, because `IsDataValid` exists only in the editor. `USleepwalkerSquadData` and its `SquadSize` stand for your own class and property.

```cpp
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"

// Bad data: the asset must report it.
USleepwalkerSquadData* Data = NewObject<USleepwalkerSquadData>(GetTransientPackage());
FIntProperty* Size = FindFProperty<FIntProperty>(Data->GetClass(), TEXT("SquadSize"));
if (!TestNotNull(TEXT("The property exists"), Size))
{
	return false;
}
Size->SetPropertyValue_InContainer(Data, 0);
FDataValidationContext Bad;
TestEqual(TEXT("A squad of 0 is invalid"), Data->IsDataValid(Bad), EDataValidationResult::Invalid);
TestTrue(TEXT("The error is reported"), Bad.GetNumErrors() > 0);

// Good data: no errors.
Size->SetPropertyValue_InContainer(Data, 4);
FDataValidationContext Good;
Data->IsDataValid(Good);
TestTrue(TEXT("A squad of 4 has no errors"), Good.GetNumErrors() == 0);
```

## 3. Behavior after the draft (spawn the picked heroes)

Put gameplay behavior in a component of the project module on the game state (`UActorComponent`). It finds the draft component, binds `OnDraftCompleted` on the server only and reads the final picks from `GetState()`:

```cpp
// In BeginPlay of the project's component, on the game state.
if (GetOwner()->HasAuthority())
{
	if (UCKDraftComponent* Draft = GetOwner()->FindComponentByClass<UCKDraftComponent>())
	{
		Draft->OnDraftCompleted.AddDynamic(this, &USleepwalkerDraftSpawner::HandleDraftCompleted);
	}
}

void USleepwalkerDraftSpawner::HandleDraftCompleted()
{
	const UCKDraftComponent* Draft = GetOwner()->FindComponentByClass<UCKDraftComponent>();
	const FCKDraftState& State = Draft->GetState();
	for (const FCKDraftParticipant& Participant : State.Participants)
	{
		FCKDraftHero Hero;
		if (Participant.Player != nullptr && State.HeroPool != nullptr && State.HeroPool->FindHero(Participant.Hero, Hero))
		{
			// Hero.PawnClass is a soft class: load it, then spawn and possess for Participant.Player's controller.
		}
	}
}
```

`OnDraftCompleted` is a dynamic delegate: the handler is a `UFUNCTION()`. Start the draft where the match starts, on the server: `Draft->StartDraft(Mode)` (every player, teams round-robin) or `StartDraftWithTeams(Mode, Players)`; a `false` result means the mode cannot start, and `GetLastStartError()` says why in plain words. The plugin never spawns pawns itself.

## 4. A draft screen (cosmetic, every machine)

A widget or component of the project reads the replicated draft and shows it; it never changes the draft. Bind the events to refresh the view and ask through the local player component:

```cpp
UCKDraftComponent* Draft = UCKDraftComponent::FindDraftComponent(this);
UCKDraftPlayerComponent* Player = UCKDraftPlayerComponent::FindLocalPlayerComponent(this);
if (Draft != nullptr && Player != nullptr)
{
	Draft->OnTurnChanged.AddDynamic(this, &UMyDraftScreen::HandleTurnChanged);      // refresh whose turn it is
	Draft->OnHeroPicked.AddDynamic(this, &UMyDraftScreen::HandleHeroAction);        // mark the hero as taken
	Player->OnActionRejected.AddDynamic(this, &UMyDraftScreen::HandleRejected);     // show the reason with GetRejectReasonText
}
// A hero button: Player->RequestPick(HeroId); a ban turn: Player->RequestBan(HeroId); hovering a button: Player->HoverHero(HeroId).
```

Read what to show from `Draft->GetState()` (it already hides what this player may not see), `GetOfferedHeroes(PlayerState)`, `GetTurnSecondsLeft()`, `IsPlayerToAct(PlayerState)` and `PreviewAction(...)` (greys out a hero the server would refuse). In Blueprint the same nodes and `Draft Library` helpers exist. The ready demo (`CKDraftDemoPanelWidget` of the plugin) is a full example to read, not to edit. The widget runs on clients that may be late: the draft component can be missing at first, so retry when the game state replicates.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
