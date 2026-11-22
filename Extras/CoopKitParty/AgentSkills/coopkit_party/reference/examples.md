# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. `Build.cs` of the project module needs `"CoopKitParty"` in its dependencies.

## 1. A party panel that shows who is in and who is ready

No plugin class to subclass: a subsystem of the project listens to the party and keeps a text the UI shows.

`Source/Sleepwalker/Public/SleepwalkerPartyWatcher.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKPartyTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SleepwalkerPartyWatcher.generated.h"

/** Keeps a one-line summary of the local player's party for the menu. */
UCLASS()
class SLEEPWALKER_API USleepwalkerPartyWatcher : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** "3 in party, 2 ready" or "Not in a party". */
	UFUNCTION(BlueprintPure, Category = "Party")
	FString GetSummary() const
	{
		return Summary;
	}

	/** Builds the summary from a roster; pure, so the test can call it. */
	static FString MakeSummary(const FCKPartyRoster& Roster);

private:
	UFUNCTION()
	void HandleRoster(const FCKPartyRoster& Roster);

	FString Summary = TEXT("Not in a party");
};
```

`Source/Sleepwalker/Private/SleepwalkerPartyWatcher.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerPartyWatcher.h"
#include "CKPartySubsystem.h"
#include "Engine/GameInstance.h"

void USleepwalkerPartyWatcher::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The party subsystem must exist before this one binds to it.
	if (UCKPartySubsystem* Party = Collection.InitializeDependency<UCKPartySubsystem>())
	{
		Party->OnRosterChanged.AddDynamic(this, &USleepwalkerPartyWatcher::HandleRoster);
	}
}

FString USleepwalkerPartyWatcher::MakeSummary(const FCKPartyRoster& Roster)
{
	if (Roster.Members.Num() == 0)
	{
		return TEXT("Not in a party");
	}
	int32 Ready = 0;
	for (const FCKPartyMember& Member : Roster.Members)
	{
		Ready += Member.bReady ? 1 : 0;
	}
	return FString::Printf(TEXT("%d in party, %d ready"), Roster.Members.Num(), Ready);
}

void USleepwalkerPartyWatcher::HandleRoster(const FCKPartyRoster& Roster)
{
	Summary = MakeSummary(Roster);
}
```

The Ready button of the menu calls `GetGameInstance()->GetSubsystem<UCKPartySubsystem>()->SetReady(true)`; a phrase button calls `SendChatPhrase(Index)` with the index of the profile's *Chat Phrases*.

## 2. "Reconnecting..." while the party finds its leader

Bind `OnStateChanged` and `OnLeaderChanged` of `UCKPartySubsystem` the same way as in example 1: show the message when the state becomes `ECKPartyState::Reconnecting`, hide it when it is `InParty` again. `OnLeaderChanged` gives `FCKPartyLeaderChangeReport`: `NewLeaderKey` (compare with `GetLocalPlayerKey()` for "You lead the party now") and `BackInPartyMs`. `OnLeft` gives the reason: `ECKPartyLeaveReason::Kicked`, `Banned`, `LeaderLost` and others.

## 3. The party's choice of game mode reaches the game

The leader sets the value once; every member, and the next leader after a leader change, has it.

```cpp
// On the leader, when the player picks a mode in the menu.
Party->SetPartyAttribute(TEXT("Mode"), TEXT("Survival"));

// On any member, when the game starts.
bool bFound = false;
const FString Mode = Party->GetPartyAttribute(TEXT("Mode"), bFound);
```

Each member's own value (a platform id the leader needs for a group matchmaking ticket, a chosen hero) is a member attribute. Every member sets its own, the leader included; the leader checks the key (1 to 64 characters of `A-Z a-z 0-9 _ . -`) and the size (256 characters a value, 16 attributes a member) and sends it with the roster.

```cpp
// On every member, once it is in the party.
Party->SetMemberAttribute(TEXT("PlayFab.EntityId"), EntityId);

// On the leader, when it builds the ticket: every member's id.
for (const FCKPartyMember& Member : Party->GetMembers())
{
	bool bHasId = false;
	const FString MemberId = Party->GetMemberAttribute(Member.PlayerKey, TEXT("PlayFab.EntityId"), bHasId);
}
```

`OnMemberAttributeChanged(PlayerKey, Key, Value)` fires on every member when a roster brings a new, changed or removed (empty `Value`) member attribute.

## 4. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerPartyWatcherTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SleepwalkerPartyWatcher.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerPartyWatcherTest, "Sleepwalker.Party.Summary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerPartyWatcherTest::RunTest(const FString& Parameters)
{
	FCKPartyRoster Roster;
	TestEqual(TEXT("An empty roster"), USleepwalkerPartyWatcher::MakeSummary(Roster), FString(TEXT("Not in a party")));
	FCKPartyMember& Leader = Roster.Members.AddDefaulted_GetRef();
	Leader.bReady = true;
	Roster.Members.AddDefaulted();
	TestEqual(TEXT("Two members, one ready"), USleepwalkerPartyWatcher::MakeSummary(Roster), FString(TEXT("2 in party, 1 ready")));
	return true;
}

#endif
```

### Testing a validation

The plugin's `UCKPartyProfileData` is `final`; a validation rule of the project lives in its own data asset (a `UPrimaryDataAsset` subclass with `IsDataValid`). Its test, inside `#if WITH_EDITOR`, because `IsDataValid` exists only in the editor: `USleepwalkerSquadData` and its `SquadSize` stand for your own class and property.

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

A test of the party itself over the network needs several Play In Editor windows; the plugin's own tests in `Source/CoopKitPartyTests/Private/CKPartyNetTest.cpp` show how.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.

## 6. A "Play" button: the whole party into a match and back

Only the leader starts it; the members follow by themselves. A game without a place for every member of the party is refused at once, before anyone travels.

```cpp
// On the leader: host a game of 4 places on the arena map.
UCKPartySubsystem* Party = GetGameInstance()->GetSubsystem<UCKPartySubsystem>();
FCKPartyGameHostParams Params;
Params.MapPackageName = TEXT("/Game/Maps/Arena");
Params.MaxPlayers = 4;
Params.Attributes.Add(TEXT("Mode"), TEXT("Survival"));
if (!Party->HostGame(Params) && Party->GetLastGameFailure() == ECKPartyGameFailure::NotEnoughSeats)
{
	// Tell the player: the game has no place for the whole party. Nobody left the menu.
}

// Or join someone else's game: FindGames, then on OnGameSearchComplete pick one of GetGameSearchResults().
FCKPartyGameSearchParams Search;
Search.RequiredAttributes.Add(TEXT("Mode"), TEXT("Survival"));
Party->FindGames(Search);
// ... later: Party->JoinGame(Party->GetGameSearchResults()[0]); refused the same way when OpenSlots is too small.

// Back to the menu after the match (any member); the party comes together again by itself.
Party->LeaveGame(TEXT("/Game/Maps/MainMenu"));
```

Every member hears the result through `OnGameTravel`; bind it like `OnRosterChanged` in example 1:

```cpp
// In the header: UFUNCTION() void HandleGameTravel(bool bSuccess, ECKPartyGameFailure Failure);
Party->OnGameTravel.AddDynamic(this, &USleepwalkerPartyWatcher::HandleGameTravel);

void USleepwalkerPartyWatcher::HandleGameTravel(const bool bSuccess, const ECKPartyGameFailure Failure)
{
	// FollowFailed: this player could not get into the leader's game; it stays in the party and rejoins it after the match.
	Summary = bSuccess ? TEXT("In the game") : FString::Printf(TEXT("Could not join the game (%s)"), *UEnum::GetValueAsString(Failure));
}
```
