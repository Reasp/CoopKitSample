# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. `UCKLootTableData` is `final`: the examples extend the plugin with components of the project, never with a subclass of the table.

## 1. Put picked-up loot into the project's inventory (a receiver component)

`Source/Sleepwalker/Public/SleepwalkerBagComponent.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKLootReceiver.h"
#include "Components/ActorComponent.h"
#include "SleepwalkerBagComponent.generated.h"

/** The player's bag of this project. Put it on the player state: CoopKit Loot gives it every item the player picks up. */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerBagComponent : public UActorComponent, public ICKLootReceiver
{
	GENERATED_BODY()

public:
	/** Units of an item in the bag. */
	UFUNCTION(BlueprintPure, Category = "Bag")
	int32 GetUnits(FName ItemId) const
	{
		const int32* Units = Items.Find(ItemId);
		return Units != nullptr ? *Units : 0;
	}

	/** Server: takes as many units as the bag has room for; the rest stays on the floor. */
	virtual int32 ReceiveLoot_Implementation(APlayerState* Player, const FCKLootItem& Item, ACKLootDrop* Drop) override;

	/** Most units the bag holds, all items together. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag", meta = (ClampMin = "1"))
	int32 Capacity = 20;

private:
	TMap<FName, int32> Items;
};
```

`Source/Sleepwalker/Private/SleepwalkerBagComponent.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerBagComponent.h"

int32 USleepwalkerBagComponent::ReceiveLoot_Implementation(APlayerState* Player, const FCKLootItem& Item, ACKLootDrop* Drop)
{
	int32 Used = 0;
	for (const TPair<FName, int32>& Entry : Items)
	{
		Used += Entry.Value;
	}
	// 0 refuses the pickup: the player hears CK.Loot.Reject.Full and the drop stays.
	const int32 Taken = FMath::Clamp(Capacity - Used, 0, Item.Count);
	if (Taken > 0)
	{
		Items.FindOrAdd(Item.ItemId) += Taken;
	}
	return Taken;
}
```

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitLoot");` (the class implements a plugin interface in its public header).

How the user uses it: open the player state Blueprint, **Add > Sleepwalker Bag**. Nothing else: the plugin finds the receiver on the pawn, the controller or the player state. For replication of the bag to the owning player, make `Items` an array of structs with `UPROPERTY(Replicated)` and `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)`.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerBagComponentTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "CKLootTypes.h"
#include "Misc/AutomationTest.h"
#include "SleepwalkerBagComponent.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerBagTest, "Sleepwalker.Loot.Bag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerBagTest::RunTest(const FString& Parameters)
{
	USleepwalkerBagComponent* Bag = NewObject<USleepwalkerBagComponent>(GetTransientPackage());
	Bag->Capacity = 3;
	FCKLootItem Coins;
	Coins.ItemId = TEXT("Coin");
	Coins.Count = 5;

	// The interface call the server makes on a pickup.
	TestEqual(TEXT("A bag of 3 takes 3 of 5 coins"), ICKLootReceiver::Execute_ReceiveLoot(Bag, nullptr, Coins, nullptr), 3);
	TestEqual(TEXT("The coins are in the bag"), Bag->GetUnits(TEXT("Coin")), 3);
	TestEqual(TEXT("A full bag takes nothing (the pickup is refused)"), ICKLootReceiver::Execute_ReceiveLoot(Bag, nullptr, Coins, nullptr), 0);
	return true;
}

#endif
```

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

## 3. Drop loot when the boss dies (server code of the project)

`Source/Sleepwalker/Public/SleepwalkerBossLootComponent.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SleepwalkerBossLootComponent.generated.h"

class UCKLootTableData;

/** Drops the boss's loot table where the boss stands. Call Drop Boss Loot from the boss's death on the server. */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerBossLootComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Server: rolls the table and lays the drops out around the owner. Does nothing on a client. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loot")
	void DropBossLoot();

	/** The boss's loot table (its sharing mode decides who gets what). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UCKLootTableData> Table;
};
```

`Source/Sleepwalker/Private/SleepwalkerBossLootComponent.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerBossLootComponent.h"
#include "CKLootSubsystem.h"
#include "GameFramework/Actor.h"

void USleepwalkerBossLootComponent::DropBossLoot()
{
	UCKLootSubsystem* Loot = UCKLootSubsystem::Get(this);
	if (Loot != nullptr && Table != nullptr && GetOwner()->HasAuthority())
	{
		Loot->DropLoot(Table, GetOwner()->GetActorLocation());
	}
}
```

A test for it runs a listen server session (see the plugin's `CoopKit.Loot.Replication` tests): call `DropBossLoot` on the server and check `UCKLootSubsystem::GetDrops` on the server and the clients.

## 4. A roll window and a drop list (purely visual)

The UI belongs to the project. A UMG widget of the local player reads `UCKLootSubsystem::GetVisibleDrops(PlayerState)` (only what this player may see), shows each drop's `GetItem()`, `GetPhase()` and `GetAssignedTo()`, and for a drop in `Rolling` the choices of `GetRollState()` and `GetRollSecondsLeft()` (counted from the server clock). Buttons call `UCKLootPlayerComponent::PickUp` and `Roll(Drop, Need/Greed/Pass)`; `OnLootReceived` and `OnRejected` give the answer. Refresh on a timer or on `ACKLootDrop::OnDropChanged`. The plugin's demo panel (`CKLootDemoPanelWidget.cpp`) is a complete example built in C++.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
