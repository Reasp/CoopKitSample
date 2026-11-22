# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The item class `UCKInventoryItemData` is not `final`: example 1 adds a field in a subclass; the other examples use the inventory component's events, its block tags and its requests.

## 1. "Items have a weight" (a field in an item subclass)

`Source/Sleepwalker/Public/SleepwalkerItemData.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKInventoryItemData.h"
#include "SleepwalkerItemData.generated.h"

/** A CoopKit Inventory item with the weight of this project. Create items of this class for everything players carry. */
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerItemData : public UCKInventoryItemData
{
	GENERATED_BODY()

public:
	/** Weight of one unit, in kg. */
	float GetWeightKg() const
	{
		return WeightKg;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Weight of one unit. Zero: the item weighs nothing (a key, a note). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight", meta = (ClampMin = "0", Units = "kg", AllowPrivateAccess = "true"))
	float WeightKg = 1.0f;
};
```

`Source/Sleepwalker/Private/SleepwalkerItemData.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerItemData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SleepwalkerItemData"

#if WITH_EDITOR
EDataValidationResult USleepwalkerItemData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// Shown in the card's Validation tab and by the CoopKit Hub's Check Project.
	if (WeightKg < 0.0f)
	{
		Context.AddError(LOCTEXT("NegativeWeight", "Weight Kg is negative: it must be zero or more."));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.AddRange(new string[] { "CoopKitInventory", "GameplayTags" });` (the class derives from a plugin class, so the dependency is public).

How the user uses it: create the item in the Content Browser (**Add > Miscellaneous > Data Asset**, pick *Sleepwalker Item Data*); it opens in the same card as the plugin's items, the Properties tab shows **Weight Kg**, and the CoopKit Hub lists it with the inventory items. The hub's *New Inventory Item* wizard creates the plugin's own class only; tell the user. Existing items keep working: an inventory holds any subclass.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerItemDataTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SleepwalkerItemData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerItemDataTest, "Sleepwalker.Inventory.Weight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerItemDataTest::RunTest(const FString& Parameters)
{
	USleepwalkerItemData* Item = NewObject<USleepwalkerItemData>(GetTransientPackage());
	TestEqual(TEXT("A new item weighs 1 kg"), Item->GetWeightKg(), 1.0f);

	// Bad data, set like a designer would in the Details panel (the property is private, so go through reflection).
	FFloatProperty* Weight = FindFProperty<FFloatProperty>(Item->GetClass(), TEXT("WeightKg"));
	if (TestNotNull(TEXT("The property exists"), Weight))
	{
		Weight->SetPropertyValue_InContainer(Item, -1.0f);
		FDataValidationContext Context;
		Item->IsDataValid(Context);
		TArray<FText> Warnings;
		TArray<FText> Errors;
		Context.SplitIssues(Warnings, Errors);
		TestTrue(TEXT("A negative weight is reported"), Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Weight")); }));
	}
	return true;
}

#endif
```

A fresh item also gets the plugin's warnings (no name, no world mesh); the test looks for its own message only.

## 3. "A player cannot pick anything up when overloaded" (a block tag)

The server checks the tags `CK.Inventory.Block.*` of the inventory's owner, its pawn and its player state (their own tags and those of their components with `IGameplayTagAssetInterface`) before every request and answers `Blocked`. The project adds a component to its player state, next to the inventory, that counts the weight and publishes `CK.Inventory.Block.Pickup` while it is too high.

`Source/Sleepwalker/Public/SleepwalkerCarryLimitComponent.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "GameplayTagAssetInterface.h"
#include "SleepwalkerCarryLimitComponent.generated.h"

class UCKInventoryComponent;
struct FCKInventoryEntry;

/** Forbids picking items up while the inventory of the owner weighs more than the limit (tag CK.Inventory.Block.Pickup). */
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerCarryLimitComponent : public UActorComponent, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	USleepwalkerCarryLimitComponent();

	/** Weight the player carries, in kg; counted on the server, replicated for the UI. */
	float GetCarriedKg() const
	{
		return CarriedKg;
	}

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleEntry(UCKInventoryComponent* Inventory, const FCKInventoryEntry& Entry);

	void Recount();

	/** Kilograms a player may carry; a heavier player cannot pick anything up until they drop or give items. */
	UPROPERTY(EditAnywhere, Category = "Carry", meta = (ClampMin = "0", Units = "kg", AllowPrivateAccess = "true"))
	float LimitKg = 30.0f;

	UPROPERTY(Replicated)
	float CarriedKg = 0.0f;
};
```

`Source/Sleepwalker/Private/SleepwalkerCarryLimitComponent.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerCarryLimitComponent.h"
#include "CKInventoryComponent.h"
#include "CKInventoryGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "SleepwalkerItemData.h"
#include "TimerManager.h"

USleepwalkerCarryLimitComponent::USleepwalkerCarryLimitComponent()
{
	SetIsReplicatedByDefault(true);
}

void USleepwalkerCarryLimitComponent::BeginPlay()
{
	Super::BeginPlay();
	// The weight decides what the server allows: only the server counts it.
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (UCKInventoryComponent* Inventory = UCKInventoryComponent::FindInventoryComponent(GetOwner()))
	{
		Inventory->OnEntryAdded.AddDynamic(this, &USleepwalkerCarryLimitComponent::HandleEntry);
		Inventory->OnEntryChanged.AddDynamic(this, &USleepwalkerCarryLimitComponent::HandleEntry);
		Inventory->OnEntryRemoved.AddDynamic(this, &USleepwalkerCarryLimitComponent::HandleEntry);
	}
}

void USleepwalkerCarryLimitComponent::HandleEntry(UCKInventoryComponent* Inventory, const FCKInventoryEntry& Entry)
{
	// On Entry Removed fires before the entry leaves the list: count once the change is done.
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &USleepwalkerCarryLimitComponent::Recount);
}

void USleepwalkerCarryLimitComponent::Recount()
{
	float Total = 0.0f;
	if (const UCKInventoryComponent* Inventory = UCKInventoryComponent::FindInventoryComponent(GetOwner()))
	{
		for (const FCKInventoryEntry& Entry : Inventory->GetEntries())
		{
			if (const USleepwalkerItemData* Item = Cast<USleepwalkerItemData>(Entry.Item))
			{
				Total += Item->GetWeightKg() * Entry.Count;
			}
		}
	}
	CarriedKg = Total;
}

void USleepwalkerCarryLimitComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (CarriedKg > LimitKg)
	{
		TagContainer.AddTag(CKInventory::Tags::Block_Pickup);
	}
}

void USleepwalkerCarryLimitComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USleepwalkerCarryLimitComponent, CarriedKg);
}
```

How the user uses it: add *Sleepwalker Carry Limit* to the player state Blueprint next to *CoopKit Inventory* and set **Limit Kg**. A pick-up over the limit is refused with `Blocked` (example 4 shows it to the player). Test it in a server-side automation test: add heavy items with `AddItem`, tick once, and check that `PickUp` answers `ECKInventoryResult::Blocked`.

## 4. "Tell the player why a request was refused" (an inventory UI)

An inventory UI reads the component on every machine and asks the server with the `Request*` functions. Every request returns an id; the answer comes back on the machine that asked through `OnRequestResult`. Find the local player's inventory with `UCKInventoryComponent::FindLocalInventoryComponent(this)` (it is null until the player state replicated: try again on the next tick), bind `OnEntryAdded`, `OnEntryChanged`, `OnEntryRemoved` and `OnEquipmentChanged` to refresh the lists, and bind `OnRequestResult` to a function that turns the result into a line of text:

```cpp
FText USleepwalkerInventoryWidget::DescribeResult(const ECKInventoryResult Result)
{
	switch (Result)
	{
	case ECKInventoryResult::InventoryFull:
		return NSLOCTEXT("Sleepwalker", "Full", "The bag is full.");
	case ECKInventoryResult::TooFar:
		return NSLOCTEXT("Sleepwalker", "TooFar", "Too far away.");
	case ECKInventoryResult::Blocked:
		return NSLOCTEXT("Sleepwalker", "Blocked", "You cannot do that now (too heavy?).");
	case ECKInventoryResult::SlotRejectsItem:
		return NSLOCTEXT("Sleepwalker", "WrongSlot", "That does not go there.");
	default:
		return FText::GetEmpty();
	}
}
```

A widget never changes the list itself: it calls `RequestEquip(Entry.EntryId, Slot)`, `RequestUse(Entry.EntryId)`, `RequestDrop(Entry.EntryId, Count)` and waits for the replicated change. Items show their **Icon** (load the soft reference before use) and **Display Name**; the stats of a copy are `Entry.GetStat(Tag)`.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
