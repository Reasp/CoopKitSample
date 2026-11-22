# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The data classes `UCKCraftRecipeData` and `UCKCraftStationData` are `final`: game rules go into components of the project, as below.

## 1. Connect the game's inventory ("the workbench should take items from our backpack")

`Source/Sleepwalker/Public/SleepwalkerCraftBackpack.h`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#pragma once

#include "CKCraftInventory.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "SleepwalkerCraftBackpack.generated.h"

/**
 * The player's backpack as CoopKit Craft sees it. Put it on the player state: the station takes the inputs of a job from
 * it and gives the results to it. Items are found by Item Id; here the backpack is a simple map of counts.
 */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerCraftBackpack : public UActorComponent, public ICKCraftInventory
{
	GENERATED_BODY()

public:
	virtual int32 GetCraftItemCount_Implementation(APlayerState* Player, const FCKCraftItem& Item) const override;
	virtual int32 TakeCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, int32 Count) override;
	virtual int32 GiveCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, int32 Count) override;

	/** Units per item id. Server only; replicate it in your own way if clients show it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sleepwalker|Backpack")
	TMap<FName, int32> Counts;

	/** Most units of one item the backpack holds; what does not fit stays in the station for the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sleepwalker|Backpack", meta = (ClampMin = "1"))
	int32 MaxPerItem = 99;
};
```

`Source/Sleepwalker/Private/SleepwalkerCraftBackpack.cpp`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#include "SleepwalkerCraftBackpack.h"

int32 USleepwalkerCraftBackpack::GetCraftItemCount_Implementation(APlayerState* Player, const FCKCraftItem& Item) const
{
	const int32* Found = Counts.Find(Item.ItemId);
	return Found != nullptr ? *Found : 0;
}

int32 USleepwalkerCraftBackpack::TakeCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, const int32 Count)
{
	int32& Have = Counts.FindOrAdd(Item.ItemId);
	const int32 Taken = FMath::Clamp(Count, 0, Have);
	Have -= Taken;
	return Taken;
}

int32 USleepwalkerCraftBackpack::GiveCraftItems_Implementation(APlayerState* Player, const FCKCraftItem& Item, const int32 Count)
{
	int32& Have = Counts.FindOrAdd(Item.ItemId);
	const int32 Given = FMath::Clamp(Count, 0, MaxPerItem - Have);
	Have += Given;
	return Given;
}
```

Tell the user: add **Sleepwalker Craft Backpack** to the player state Blueprint (or create it in C++); the stations find it by themselves.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerCraftBackpackTest.cpp`

```cpp
// Copyright 2026 Sleepwalker Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SleepwalkerCraftBackpack.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerCraftBackpackTest, "Sleepwalker.Craft.Backpack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerCraftBackpackTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<USleepwalkerCraftBackpack> Backpack(NewObject<USleepwalkerCraftBackpack>());
	Backpack->Counts.Add(TEXT("Scrap"), 5);
	Backpack->MaxPerItem = 10;
	FCKCraftItem Scrap;
	Scrap.ItemId = TEXT("Scrap");

	TestEqual(TEXT("Counts what it has"), ICKCraftInventory::Execute_GetCraftItemCount(Backpack.Get(), nullptr, Scrap), 5);
	TestEqual(TEXT("Takes no more than it has"), ICKCraftInventory::Execute_TakeCraftItems(Backpack.Get(), nullptr, Scrap, 8), 5);
	TestEqual(TEXT("Gives no more than fits"), ICKCraftInventory::Execute_GiveCraftItems(Backpack.Get(), nullptr, Scrap, 15), 10);
	return true;
}

#endif
```

## 3. Do something when a job finishes (a component of the project)

`USleepwalkerWorkbenchSounds : UActorComponent` on the station actor: in `BeginPlay`, find the station with `UCKCraftStationComponent::FindStationComponent(GetOwner())` and bind `OnJobFinished` (a `UFUNCTION` with `(UCKCraftStationComponent* Station, int32 JobId, UCKCraftRecipeData* Recipe, bool bCompleted)`). Play the sound on every machine; give quest progress only where `GetOwner()->HasAuthority()`. A cancelled job comes with `bCompleted` false.

## 4. A station panel (purely visual)

A UMG widget of the project: on Construct, get the station (**Find Station Component** of the workbench actor) and the local player component (**Find Local Player Component**); bind **On Queue Changed** of the station and **On Request Result** of the player component. Build one row per entry of **Get Jobs**: the recipe's *Display Name*, *Delivered* against the recipe's *Inputs*, the *Contributions* (player name and units) and **Get Job Progress** for the job in work. Buttons call **Request Queue**, **Request Contribute**, **Request Cancel** and **Request Collect**. Show the *Reason* tag of a refused answer. No C++ is needed.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
