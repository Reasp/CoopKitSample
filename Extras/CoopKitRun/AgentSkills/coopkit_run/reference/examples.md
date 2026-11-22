# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's data classes (`UCKRunData`, `UCKRunUpgradePoolData`, `UCKRunModifierData`, `UCKRunMetaData`) are `final`: a new behavior goes into a rule, a component, an event handler or a store of the project, as below.

## 1. The project's own path rule ("a shop before every boss")

`Source/Sleepwalker/Public/SleepwalkerShopPathRule.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKRunPathRule.h"
#include "CoreMinimal.h"
#include "SleepwalkerShopPathRule.generated.h"

/** Doors after a room; the room before a stage's final room is always the shop. */
UCLASS(meta = (DisplayName = "Doors with a Shop before the Boss"))
class SLEEPWALKER_API USleepwalkerShopPathRule : public UCKRunPathRule
{
	GENERATED_BODY()

public:
	virtual void BuildOptions_Implementation(const FCKRunPublicState& State, const UCKRunData* Run, int64 Seed, TArray<FCKRunPathOption>& OutOptions) const override;
	virtual void ValidateRule(const UCKRunData& Run, TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const override;

	/** Room type of the shop: one of the run's room types. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FName ShopRoomType = TEXT("Shop");

	/** Doors after the other rooms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1", UIMax = "4"))
	int32 DoorCount = 2;
};
```

`Source/Sleepwalker/Private/SleepwalkerShopPathRule.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerShopPathRule.h"
#include "CKRunData.h"
#include "CKRunRandom.h"

#define LOCTEXT_NAMESPACE "SleepwalkerShopPathRule"

void USleepwalkerShopPathRule::BuildOptions_Implementation(const FCKRunPublicState& State, const UCKRunData* Run, const int64 Seed, TArray<FCKRunPathOption>& OutOptions) const
{
	OutOptions.Reset();
	if (Run == nullptr)
	{
		return;
	}
	// The first room of a stage and its final room are fixed by the stage.
	FCKRunPathOption Fixed;
	if (GetFixedOption(State, *Run, Fixed))
	{
		OutOptions.Add(Fixed);
		return;
	}
	int32 Stage = INDEX_NONE;
	int32 RoomInStage = INDEX_NONE;
	bool bFinal = false;
	if (!GetNextPosition(State, Run, Stage, RoomInStage, bFinal))
	{
		return;
	}
	if (RoomInStage + 2 == Run->GetStages()[Stage].RoomCount && Run->FindRoomType(ShopRoomType) != nullptr)
	{
		OutOptions.Add(MakeOption(Run, ShopRoomType, Stage, false));
		return;
	}
	// Randomness only from Seed: every machine and every replay of the run seed gets the same doors.
	TArray<FName> Candidates = GetCandidateRoomTypes(State, *Run, Stage, RoomInStage);
	Candidates.Remove(ShopRoomType);
	FCKRunRandom Random(Seed);
	Random.Shuffle(Candidates);
	for (int32 Index = 0; Index < Candidates.Num() && Index < DoorCount; ++Index)
	{
		OutOptions.Add(MakeOption(Run, Candidates[Index], Stage, false));
	}
}

void USleepwalkerShopPathRule::ValidateRule(const UCKRunData& Run, TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	Super::ValidateRule(Run, OutErrors, OutWarnings);
	// Shown in the run's Validation tab and by the CoopKit Hub's Check Project.
	if (Run.FindRoomType(ShopRoomType) == nullptr)
	{
		OutErrors.Add(FText::Format(LOCTEXT("NoShop", "Path Rule: Shop Room Type {0} is not a room type of the run."), FText::FromName(ShopRoomType)));
	}
}

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitRun");` (the class derives from a plugin class, so the dependency is public).

How the user uses it: open the run in the CoopKit Hub, add a room type with the id `Shop` (Properties tab, Room Types), then set **Path Rule** to **Doors with a Shop before the Boss**. The Simulate tab of the card shows the new doors for any seed at once.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerShopPathRuleTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKRunCore.h"
#include "CKRunData.h"
#include "CKRunDemo.h"
#include "CKRunUpgradePoolData.h"
#include "Misc/AutomationTest.h"
#include "SleepwalkerShopPathRule.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerShopPathRuleTest, "Sleepwalker.Run.ShopBeforeBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerShopPathRuleTest::RunTest(const FString& Parameters)
{
	// The plugin's demo run (2 stages of 5 rooms) with a Shop room type and the project's rule.
	UCKRunUpgradePoolData* Pool = NewObject<UCKRunUpgradePoolData>(GetTransientPackage());
	CKRun::Demo::FillDemoPool(*Pool);
	UCKRunData* Run = NewObject<UCKRunData>(GetTransientPackage());
	CKRun::Demo::FillDemoRun(*Run, Pool);
	TArray<FCKRunRoomType> Types = Run->GetRoomTypes();
	Types.Add(CKRun::Demo::MakeRoomType(TEXT("Shop"), TEXT("Shop"), FGameplayTag(), 0.0f, false));
	Run->SetRoomTypes(Types);
	Run->SetPathRule(NewObject<USleepwalkerShopPathRule>(Run));

	// A whole run on the plugin's pure functions: every room cleared, the first card picked, the first door taken.
	TArray<CKRun::Core::FJoiningPlayer> Players;
	Players.AddDefaulted_GetRef().PlayerKey = TEXT("A");
	FCKRunState State;
	FText Error;
	if (!TestTrue(TEXT("The run starts"), CKRun::Core::BuildRun(*Run, Run->GetPathName(), 12345, 1, Players, TArray<FName>(), 0.0, State, Error)))
	{
		return false;
	}
	State.Public.Players[0].bPresent = true;
	TArray<FName> BeforeBoss;
	double Now = 0.0;
	for (int32 Step = 0; Step < 200 && State.Public.IsRunning(); ++Step)
	{
		Now += 1.0;
		if (State.Public.Phase == ECKRunPhase::InRoom)
		{
			if (State.Public.Room.RoomInStage == 3)
			{
				BeforeBoss.Add(State.Public.Room.RoomTypeId);
			}
			CKRun::Core::CompleteRoom(State, *Run, true, Now);
		}
		else if (State.Public.Phase == ECKRunPhase::Choosing)
		{
			CKRun::Core::ApplyPick(State, *Run, TEXT("A"), 0);
		}
		else if (State.Public.Phase == ECKRunPhase::PathVote)
		{
			CKRun::Core::ApplyVote(State, TEXT("A"), 0);
		}
		else if (State.Public.Phase == ECKRunPhase::Travel)
		{
			CKRun::Core::AdvanceToNextRoom(State, *Run, Now);
		}
		CKRun::Core::ResolveProgress(State, *Run, Now);
	}
	TestTrue(TEXT("The run is won"), State.Public.EndReason == ECKRunEndReason::Victory);
	TestTrue(TEXT("Both rooms before a boss are the shop"), BeforeBoss == TArray<FName>({ TEXT("Shop"), TEXT("Shop") }));
	return true;
}

#endif
```

## 3. Build the room from the room seed (the level of the project)

Every machine gets the room seed with the room and builds the same level from it: no level data travels over the network. Bind to the director's `OnRoomEntered` on every machine and draw only from the seed:

```cpp
// In BeginPlay of the project's level builder actor (every machine). The director may arrive after this actor on a client:
// try again on a timer until FindRunDirector returns it.
if (UCKRunComponent* Director = UCKRunComponent::FindRunDirector(this))
{
	Director->OnRoomEntered.AddUniqueDynamic(this, &ASleepwalkerRoomBuilder::HandleRoomEntered);
}

// UFUNCTION() void HandleRoomEntered(const FCKRunRoom& Room);
void ASleepwalkerRoomBuilder::HandleRoomEntered(const FCKRunRoom& Room)
{
	FCKRunRandom Random(Room.RoomSeed);
	const int32 Enemies = Random.RandRange(3, 6) + (Room.RoomTag.MatchesTagExact(CKRun::Tags::Room_Elite) ? 2 : 0);
	// Spawn gameplay actors (enemies, loot) on the server only; cosmetics (props, lights) on every machine.
}
```

Blueprint: the event **On Room Entered** of the run director, and **Random Integer From Seed** of `CKRunRandomLibrary` with the room seed and an index per draw. The server ends a room with **Complete Room** (Success false: the team was wiped, the run ends in defeat).

## 4. Keep meta progress in the project's backend

The meta track keeps each player's currency and nodes; by default in a save game slot on the player's machine. A project with its own profile service implements the store and names it in the meta track's **Store Class**:

```cpp
UCLASS()
class SLEEPWALKER_API USleepwalkerMetaStore : public UObject, public ICKRunMetaStore
{
	GENERATED_BODY()

public:
	virtual bool LoadMeta_Implementation(FName MetaId, const FString& SlotKey, FCKRunMetaProgress& OutProgress) override;
	virtual bool SaveMeta_Implementation(FName MetaId, const FString& SlotKey, const FCKRunMetaProgress& Progress) override;
};
```

With **Storage: Local Profile** the owning player's machine calls it (Slot Key: the local player) and the server cleans what arrives (unknown nodes, a balance over the maximum); with **Storage: Server** the server calls it with the player key. Return false from `LoadMeta` for a new player. Test it like the plugin's `CoopKit.Run.Meta.LocalStore`: save, load, compare.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
