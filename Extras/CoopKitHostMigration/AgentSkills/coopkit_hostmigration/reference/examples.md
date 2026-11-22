# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's.

## 1. "Players keep their score after the host leaves"

No plugin class to subclass: a `SaveGame` property on the project's player state is enough. The light snapshot keeps every `SaveGame` property of player states, pawns, the game state and tracked actors, and the new host gives each player their own values back when they rejoin.

`Source/Sleepwalker/Public/SleepwalkerPlayerState.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SleepwalkerPlayerState.generated.h"

/** Player state of the project: the coins survive a host migration because the property is SaveGame. */
UCLASS()
class SLEEPWALKER_API ASleepwalkerPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Sleepwalker")
	int32 GetCoins() const
	{
		return Coins;
	}

	/** Server: adds coins. */
	UFUNCTION(BlueprintCallable, Category = "Sleepwalker")
	void AddCoins(int32 Amount);

private:
	/** Coins of the player. SaveGame: the host migration snapshot carries it to the new host. */
	UPROPERTY(Replicated, SaveGame)
	int32 Coins = 0;
};
```

For an actor of the level (a door, a chest): add the *CoopKit Host Migration Tracked* component to it and mark its state `SaveGame` the same way. Set the player state class in the game mode.

## 2. A "Reconnecting..." message while the group moves to the new host

`Source/Sleepwalker/Public/SleepwalkerMigrationNotice.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKHostMigrationTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SleepwalkerMigrationNotice.generated.h"

/** Shows a message on this machine from the moment the host is lost until the group plays again. */
UCLASS()
class SLEEPWALKER_API USleepwalkerMigrationNotice : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** The text to show; empty while nothing happens. */
	FText GetNotice() const
	{
		return Notice;
	}

private:
	UFUNCTION()
	void HandleHostLost(ECKHostMigrationLossReason Reason);

	UFUNCTION()
	void HandleMigrationFinished(const FCKHostMigrationReport& Report);

	FText Notice;
};
```

`Source/Sleepwalker/Private/SleepwalkerMigrationNotice.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerMigrationNotice.h"
#include "CKHostMigrationSubsystem.h"

void USleepwalkerMigrationNotice::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// The migration subsystem must exist before this one binds to it.
	UCKHostMigrationSubsystem* Migration = Collection.InitializeDependency<UCKHostMigrationSubsystem>();
	if (Migration != nullptr)
	{
		Migration->OnHostLost.AddDynamic(this, &USleepwalkerMigrationNotice::HandleHostLost);
		Migration->OnMigrationFinished.AddDynamic(this, &USleepwalkerMigrationNotice::HandleMigrationFinished);
	}
}

void USleepwalkerMigrationNotice::HandleHostLost(const ECKHostMigrationLossReason Reason)
{
	Notice = NSLOCTEXT("Sleepwalker", "HostLost", "The host left. Reconnecting...");
}

void USleepwalkerMigrationNotice::HandleMigrationFinished(const FCKHostMigrationReport& Report)
{
	const bool bPlaying = Report.Outcome == ECKHostMigrationOutcome::NewHost || Report.Outcome == ECKHostMigrationOutcome::Joined;
	Notice = bPlaying ? FText::GetEmpty() : NSLOCTEXT("Sleepwalker", "MigrationFailed", "The game could not continue without its host.");
}
```

A widget reads `GetNotice()` from `GetGameInstance()->GetSubsystem<USleepwalkerMigrationNotice>()`.

## 3. The project's own save system as the snapshot source

`Source/Sleepwalker/Public/SleepwalkerSnapshotSource.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKHostMigrationSnapshotSource.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SleepwalkerSnapshotSource.generated.h"

/** Carries the world through a host migration with the project's own save code. Named in the migration profile. */
UCLASS()
class SLEEPWALKER_API USleepwalkerSnapshotSource : public UObject, public ICKHostMigrationSnapshotSource
{
	GENERATED_BODY()

public:
	virtual bool CaptureSnapshot(UWorld& World, TArray<uint8>& OutBytes) override;
	virtual ECKHostMigrationApplyResult ApplySnapshot(UWorld& World, const TArray<uint8>& Bytes) override;
};
```

In `CaptureSnapshot` write the project's save into `OutBytes` (in memory, not to a slot file); in `ApplySnapshot` load the same bytes and return `Applied`, or `Pending` when the load takes several frames (then implement `PollApply`). Set *Snapshot Source Class* of the profile to `SleepwalkerSnapshotSource`. The plugin checks the length and the hash of the bytes before `ApplySnapshot`, so the method never sees a snapshot cut short.

## 4. The automation test of the examples

`Source/Sleepwalker/Private/Tests/SleepwalkerMigrationTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SleepwalkerPlayerState.h"
#include "SleepwalkerSnapshotSource.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerMigrationTest, "Sleepwalker.HostMigration.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerMigrationTest::RunTest(const FString& Parameters)
{
	// The coins are a SaveGame property: the plugin's light snapshot carries them.
	const FProperty* Coins = FindFProperty<FProperty>(ASleepwalkerPlayerState::StaticClass(), TEXT("Coins"));
	TestTrue(TEXT("Coins are kept by the migration snapshot"), Coins != nullptr && Coins->HasAnyPropertyFlags(CPF_SaveGame));

	// The project's snapshot source implements the plugin's interface, so the profile accepts it.
	TestTrue(TEXT("The snapshot source fits the profile"), USleepwalkerSnapshotSource::StaticClass()->ImplementsInterface(UCKHostMigrationSnapshotSource::StaticClass()));
	return true;
}

#endif
```

Run it with the command in `build_and_diagnose.md` and the filter `Sleepwalker.HostMigration`.

### Testing a validation

A validation rule of the project lives in its own data asset (a `UPrimaryDataAsset` subclass with `IsDataValid`). Its test, inside `#if WITH_EDITOR`, because `IsDataValid` exists only in the editor: `USleepwalkerSquadData` and its `SquadSize` stand for your own class and property.

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

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
