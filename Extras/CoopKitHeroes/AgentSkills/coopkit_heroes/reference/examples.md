# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`), and the copyright line with the project's.

## 1. A numeric hero parameter as a hero module ("regeneration rate")

`Source/Sleepwalker/Public/SleepwalkerHeroRegenModule.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CKHeroModule.h"
#include "CoreMinimal.h"
#include "SleepwalkerHeroRegenModule.generated.h"

/**
 * Hero module: how fast the hero regenerates health. Add it to a hero's Modules list. The server hands the rate to the
 * pawn's regeneration component when the player plays the hero and takes it back when the player stops.
 */
UCLASS(meta = (DisplayName = "Regeneration"))
class SLEEPWALKER_API USleepwalkerHeroRegenModule : public UCKHeroModule
{
	GENERATED_BODY()

public:
	virtual void ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual void RemoveFromPawn_Implementation(APawn* Pawn, APlayerState* Player) const override;
	virtual FText GetSummary_Implementation() const override;
	virtual void CollectErrors_Implementation(TArray<FText>& OutErrors) const override;

	float GetRegenerationRate() const
	{
		return RegenerationRate;
	}

private:
	/** Health regenerated per second while the hero lives. Zero turns regeneration off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regeneration", meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	float RegenerationRate = 1.0f;
};
```

`Source/Sleepwalker/Private/SleepwalkerHeroRegenModule.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerHeroRegenModule.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "SleepwalkerHeroRegenModule"

void USleepwalkerHeroRegenModule::ApplyToPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	// Server only: put RegenerationRate where the game's health logic reads it (a component of the pawn, an attribute,
	// a Gameplay Effect). Look the target up on the pawn; this object is shared by every player of the hero.
}

void USleepwalkerHeroRegenModule::RemoveFromPawn_Implementation(APawn* Pawn, APlayerState* Player) const
{
	// Undo exactly what ApplyToPawn did. Pawn can be null when the pawn is already gone.
}

FText USleepwalkerHeroRegenModule::GetSummary_Implementation() const
{
	// Shown on the hero card, Summary tab.
	return FText::Format(LOCTEXT("Summary", "Regeneration: {0} per second"), FText::AsNumber(RegenerationRate));
}

void USleepwalkerHeroRegenModule::CollectErrors_Implementation(TArray<FText>& OutErrors) const
{
	// Shown in the hero card's Validation tab and by the CoopKit Hub's Check Project.
	if (RegenerationRate < 0.0f)
	{
		OutErrors.Add(LOCTEXT("NegativeRate", "Regeneration Rate is negative: the hero would lose health."));
	}
}

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitHeroes");` (the class derives from a plugin class, so the dependency is public).

How the user uses it: open a hero, in **Modules** click **+**, pick **Regeneration**, set the rate. The hero card's Summary tab then shows "Regeneration: ... per second".

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerHeroRegenModuleTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SleepwalkerHeroRegenModule.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerHeroRegenModuleTest, "Sleepwalker.Heroes.Regeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerHeroRegenModuleTest::RunTest(const FString& Parameters)
{
	USleepwalkerHeroRegenModule* Module = NewObject<USleepwalkerHeroRegenModule>(GetTransientPackage());

	TArray<FText> Errors;
	Module->CollectErrors(Errors);
	TestEqual(TEXT("The default rate is valid"), Errors.Num(), 0);
	TestTrue(TEXT("The summary shows the rate"), Module->GetSummary().ToString().Contains(TEXT("Regeneration")));

	// Bad data, set like a designer would in the Details panel (the property is private, so go through reflection).
	FFloatProperty* Rate = FindFProperty<FFloatProperty>(Module->GetClass(), TEXT("RegenerationRate"));
	if (TestNotNull(TEXT("The property exists"), Rate))
	{
		Rate->SetPropertyValue_InContainer(Module, -1.0f);
		Errors.Reset();
		Module->CollectErrors(Errors);
		TestEqual(TEXT("A negative rate is reported"), Errors.Num(), 1);
	}
	return true;
}

#endif
```

## 3. A parameter that must be a top-level field of every hero (data subclass)

Use only when the user wants the field next to Hero Id and Display Name in the Details panel.

```cpp
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerHeroData : public UCKHeroData
{
	GENERATED_BODY()

public:
	float GetRegenerationRate() const
	{
		return RegenerationRate;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Health regenerated per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regeneration", meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	float RegenerationRate = 1.0f;
};
```

`IsDataValid` calls `Super::IsDataValid(Context)` first and adds its own errors with `Context.AddError(...)` (it needs `#include "Misc/DataValidation.h"` inside `#if WITH_EDITOR`). Create heroes of this class in the Content Browser: **Add > Miscellaneous > Data Asset**, pick the class. The hero wizard and the CSV tools create and read the plugin's base class only; tell the user.

## 4. A purely visual parameter (a prop on the hero)

Override `ApplyAppearance(Pawn, Skin)` in a module: it runs on every machine after the look was applied. Attach the prop with `UGameplayStatics::SpawnEmitterAttached` or an actor you spawn locally (not replicated), and remove it when the hero changes: the next `ApplyAppearance` call replaces the look, so look for your previous prop on the pawn first (by a component tag) and destroy it.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
