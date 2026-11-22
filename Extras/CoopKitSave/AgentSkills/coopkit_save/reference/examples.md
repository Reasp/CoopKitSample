# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's classes are `final`: the examples extend it with `SaveGame` properties, an interface of the plugin, an event of its subsystem and a component of the project.

## 1. A saved value that changes meaning ("the chest remembers its coins")

A chest placed on the map keeps `Coins`. In the first release the number counted gold coins; the next release counts silver coins (one gold is 100 silver), so old saves must be converted. The project raises *Game Data Version* of its save profile from 1 to 2 (open the profile from the CoopKit Hub card and set **Game Data Version**).

`Source/Sleepwalker/Public/SleepwalkerChest.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKSaveable.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SleepwalkerChest.generated.h"

class UCKSaveableComponent;

namespace Sleepwalker::Save
{
	/** Coins of a chest saved with Game Data Version SavedVersion, as silver coins: version 1 counted gold (1 gold = 100 silver). */
	SLEEPWALKER_API int32 ConvertSavedCoins(int32 SavedVersion, int32 SavedCoins);
}

/** A chest that keeps its coins in saves. Place it on a map; the Saveable component makes the plugin save it. */
UCLASS()
class SLEEPWALKER_API ASleepwalkerChest : public AActor, public ICKSaveable
{
	GENERATED_BODY()

public:
	ASleepwalkerChest();

	int32 GetCoins() const
	{
		return Coins;
	}

	/** Server: the plugin calls this right after the saved values were applied. */
	virtual void OnAfterLoad_Implementation(int32 SavedGameDataVersion) override;

private:
	/** Makes the plugin save this actor: its transform and its SaveGame properties. */
	UPROPERTY(VisibleAnywhere, Category = "Save")
	TObjectPtr<UCKSaveableComponent> Saveable;

	/** Silver coins in the chest. Saved; version 1 saves counted gold and are converted on load. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Loot", meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	int32 Coins = 0;
};
```

`Source/Sleepwalker/Private/SleepwalkerChest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerChest.h"
#include "CKSaveableComponent.h"

namespace Sleepwalker::Save
{
	int32 ConvertSavedCoins(const int32 SavedVersion, const int32 SavedCoins)
	{
		return SavedVersion < 2 ? SavedCoins * 100 : SavedCoins;
	}
}

ASleepwalkerChest::ASleepwalkerChest()
{
	Saveable = CreateDefaultSubobject<UCKSaveableComponent>(TEXT("Saveable"));
}

void ASleepwalkerChest::OnAfterLoad_Implementation(const int32 SavedGameDataVersion)
{
	// The saved value is already applied; convert it when the save is older than the meaning change.
	Coins = Sleepwalker::Save::ConvertSavedCoins(SavedGameDataVersion, Coins);
}
```

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitSave");` (the chest implements a plugin interface, so the dependency is public).

How the user uses it: place the chest on a map and set **Coins** in the Details panel. It is saved by `SaveToSlot` without any further setup. A placed actor that may be renamed or moved to another sub-level also gets a unique **Save Id** in its CoopKit Saveable component, so older saves still find it. In Blueprint the same is a variable with **Advanced > SaveGame** checked, the **CoopKit Saveable** component, and the **CK Saveable** interface with its **On After Load** event.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerChestTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SleepwalkerChest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerChestSaveTest, "Sleepwalker.Save.ChestCoins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerChestSaveTest::RunTest(const FString& Parameters)
{
	// The conversion: version 1 counted gold, version 2 counts silver.
	TestEqual(TEXT("A version 1 save is converted to silver"), Sleepwalker::Save::ConvertSavedCoins(1, 3), 300);
	TestEqual(TEXT("A version 2 save is left as it is"), Sleepwalker::Save::ConvertSavedCoins(2, 3), 3);

	// The property is marked SaveGame, or the plugin would not save it (the property is private: go through reflection).
	const FIntProperty* Coins = FindFProperty<FIntProperty>(ASleepwalkerChest::StaticClass(), TEXT("Coins"));
	if (TestNotNull(TEXT("The property exists"), Coins))
	{
		TestTrue(TEXT("Coins is marked SaveGame"), Coins->HasAnyPropertyFlags(CPF_SaveGame));
	}
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

## 3. Match players by the account id of the project's backend

The plugin matches a joining player to their record by unique net id, by name or by `ICKSavePlayerIdentity` on the player state, whichever the profile's **Player Key Source** says. A project with its own accounts implements the interface.

`Source/Sleepwalker/Public/SleepwalkerPlayerState.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKSavePlayerIdentity.h"
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "SleepwalkerPlayerState.generated.h"

/** Player state of the project: its account id is the key of the player's save record. */
UCLASS()
class SLEEPWALKER_API ASleepwalkerPlayerState : public APlayerState, public ICKSavePlayerIdentity
{
	GENERATED_BODY()

public:
	/** Server: the plugin asks when a player joins; an empty key falls back to the profile's Player Key Source. */
	virtual FString GetSavePlayerKey_Implementation() const override
	{
		return AccountId;
	}

	/** Account id of this player in the project's backend. Set on the server at login. */
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Account")
	FString AccountId;
};
```

C++ without a player state of your own: `UCKSaveSubsystem::ResolvePlayerKey.BindLambda([](const APlayerState& State) { return LookUpAccount(State); });` before players join. A key must stay the same across sessions and never contain a player's secret.

## 4. Block autosave in a boss fight and show "Saved"

Behavior stays in a component of the project on the game state. It grants the plugin's block tag to the game state while a fight runs and listens to the subsystem's event. The game state must answer `IGameplayTagAssetInterface` (for example with an Ability System Component that grants loose tags); the plugin reads `CK.Save.Block.Save` from it.

```cpp
// In the project's component, on the server:
UCKSaveSubsystem* SaveSubsystem = UCKSaveSubsystem::GetSaveSubsystem(this);
if (SaveSubsystem != nullptr)
{
	SaveSubsystem->OnSaveCompleted.AddDynamic(this, &USleepwalkerSaveFeedback::HandleSaveCompleted);
}

void USleepwalkerSaveFeedback::HandleSaveCompleted(const FString& SlotName, const ECKSaveResult Result)
{
	// Result is Success or the reason: Blocked (a block tag), Busy, TooManySlots, WriteFailed ...
	ShowMessage(Result == ECKSaveResult::Success ? FText::FromString(TEXT("Saved")) : FText::FromString(TEXT("Could not save")));
}
```

The block while the fight runs: add a Gameplay Effect that grants `CK.Save.Block.Save` (and `CK.Save.Block.Load`) to the game state's Ability System Component at the start of the fight and remove it at the end. A save requested meanwhile returns `Blocked`; the timed autosave simply skips that round.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
