# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's classes are `final`: the examples extend it with a data asset, a component and an interface of the project.

## 1. A number the plugin does not have, in a data asset of the project ("speed boost after a revive")

`Source/Sleepwalker/Public/SleepwalkerReviveBoostData.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SleepwalkerReviveBoostData.generated.h"

/** What a revived player gets for a short while: a speed boost. One asset per kind of character, set on its boost component. */
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerReviveBoostData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	float GetSpeedMultiplier() const
	{
		return SpeedMultiplier;
	}

	float GetBoostSeconds() const
	{
		return BoostSeconds;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Walk speed multiplier right after a revive. 1 means no boost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "1", AllowPrivateAccess = "true"))
	float SpeedMultiplier = 1.5f;

	/** How long the boost lasts, in seconds. 0 turns the boost off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0", Units = "s", AllowPrivateAccess = "true"))
	float BoostSeconds = 3.0f;
};
```

`Source/Sleepwalker/Private/SleepwalkerReviveBoostData.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerReviveBoostData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SleepwalkerReviveBoostData"

#if WITH_EDITOR
EDataValidationResult USleepwalkerReviveBoostData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// Shown in the Data Asset editor and by Data Validation.
	if (SpeedMultiplier < 1.0f)
	{
		Context.AddError(LOCTEXT("SlowBoost", "Speed Multiplier is below 1: a revive boost must not slow the player down."));
		Result = EDataValidationResult::Invalid;
	}
	if (BoostSeconds < 0.0f)
	{
		Context.AddError(LOCTEXT("NegativeBoost", "Boost Seconds is negative: it must be zero or more."));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PrivateDependencyModuleNames.Add("CoopKitRevive");` (the component of example 3 includes the plugin's headers).

How the user uses it: in the Content Browser, **Add > Miscellaneous > Data Asset**, pick *Sleepwalker Revive Boost Data*, set the two numbers, and set the asset on the boost component of the character Blueprint (example 3).

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerReviveBoostDataTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SleepwalkerReviveBoostData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerReviveBoostDataTest, "Sleepwalker.Revive.Boost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerReviveBoostDataTest::RunTest(const FString& Parameters)
{
	USleepwalkerReviveBoostData* Data = NewObject<USleepwalkerReviveBoostData>(GetTransientPackage());
	FDataValidationContext Good;
	TestEqual(TEXT("The defaults are valid"), Data->IsDataValid(Good), EDataValidationResult::Valid);

	// Bad data, set like a designer would in the Details panel (the property is private, so go through reflection).
	FFloatProperty* Multiplier = FindFProperty<FFloatProperty>(Data->GetClass(), TEXT("SpeedMultiplier"));
	if (TestNotNull(TEXT("The property exists"), Multiplier))
	{
		Multiplier->SetPropertyValue_InContainer(Data, 0.5f);
		FDataValidationContext Context;
		Data->IsDataValid(Context);
		TArray<FText> Warnings;
		TArray<FText> Errors;
		Context.SplitIssues(Warnings, Errors);
		TestTrue(TEXT("A boost that slows down is reported"), Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Speed Multiplier")); }));
	}
	return true;
}

#endif
```

## 3. Using the value on the server (a component of the project)

Gameplay changes happen on the server. Put this component on the character Blueprint together with a **CKRevive Body Component** (the match adds a body component only to pawns without one, and later than BeginPlay; with both on the Blueprint the body exists when this component starts). It reacts to the body's revive event and applies the boost.

```cpp
// SleepwalkerReviveBoostComponent.h (excerpt)
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerReviveBoostComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleRevived(UCKReviveBodyComponent* Body, AActor* Reviver);

	void EndBoost();

	/** The boost this character gets after a revive. Empty: no boost. */
	UPROPERTY(EditAnywhere, Category = "Revive")
	TObjectPtr<USleepwalkerReviveBoostData> BoostData;

	float NormalSpeed = 0.0f;
	FTimerHandle BoostTimer;
};

// SleepwalkerReviveBoostComponent.cpp (excerpt)
void USleepwalkerReviveBoostComponent::BeginPlay()
{
	Super::BeginPlay();
	// On Revived fires on every machine; the speed is changed on the server and replicates with the movement.
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (UCKReviveBodyComponent* Body = UCKReviveBodyComponent::FindBodyComponent(GetOwner()))
	{
		Body->OnRevived.AddDynamic(this, &USleepwalkerReviveBoostComponent::HandleRevived);
	}
}

void USleepwalkerReviveBoostComponent::HandleRevived(UCKReviveBodyComponent* Body, AActor* Reviver)
{
	UCharacterMovementComponent* Movement = GetOwner()->FindComponentByClass<UCharacterMovementComponent>();
	if (BoostData == nullptr || BoostData->GetBoostSeconds() <= 0.0f || Movement == nullptr)
	{
		return;
	}
	// The plugin restores the walk speed it slowed for crawling before On Revived: read the speed now.
	NormalSpeed = Movement->MaxWalkSpeed;
	Movement->MaxWalkSpeed = NormalSpeed * BoostData->GetSpeedMultiplier();
	GetWorld()->GetTimerManager().SetTimer(BoostTimer, this, &USleepwalkerReviveBoostComponent::EndBoost, BoostData->GetBoostSeconds());
}
```

`EndBoost` sets `MaxWalkSpeed` back to `NormalSpeed`. Health back after a revive goes the same way: `Body->GetBodyData()->GetRevivedHealthFraction()` times your maximum health, on the server. For a speed seen smoothly on clients, change the speed on the owning client too (Character Movement predicts it).

## 4. Pawns that never go down ("bosses are killed, not downed")

Implement `ICKRevivable` on the pawn class in C++ (or in a Blueprint: *Class Settings > Interfaces > Add > CK Revivable*). The plugin asks it on the server before every down and every revive.

```cpp
// SleepwalkerBoss.h (excerpt)
#include "CKRevivable.h"

UCLASS()
class SLEEPWALKER_API ASleepwalkerBoss : public ACharacter, public ICKRevivable
{
	GENERATED_BODY()

public:
	virtual bool CanBeDowned_Implementation(AActor* DamageInstigator) const override
	{
		// A lethal hit kills a boss at once: Down returns Dead.
		return false;
	}

	virtual bool CanBeRevivedBy_Implementation(APawn* Reviver) const override
	{
		return false;
	}
};
```

For a while only (a cutscene, a burning player), add the loose Gameplay Tag `CK.Revive.Block.Down` or `CK.Revive.Block.Revive` to the pawn's Ability System Component, or grant it with a Gameplay Effect; remove it to allow again. Without an Ability System Component, implement the interface and answer from your own state.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
