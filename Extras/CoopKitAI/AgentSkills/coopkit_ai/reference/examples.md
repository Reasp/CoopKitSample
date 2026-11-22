# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's data classes are `final`: the examples extend it with a subclass of its threat component, a component and a data asset of the project, and the director's difficulty step.

## 1. An own targeting rule in a threat component subclass ("enemies do not chase too far from home")

`Source/Sleepwalker/Public/SleepwalkerLeashThreatComponent.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKAIThreatComponent.h"
#include "SleepwalkerLeashThreatComponent.generated.h"

/**
 * Threat component of an enemy that stays near home: players farther than Leash Radius from where the enemy started are
 * no foes, so the enemy loses them and goes back to the next target or idles. Put it on the enemy pawn instead of the
 * plugin's CKAI Threat Component.
 */
UCLASS(Blueprintable, ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerLeashThreatComponent : public UCKAIThreatComponent
{
	GENERATED_BODY()

public:
	/** True when Location lies within Radius of Home; a radius of 0 means no leash. */
	static bool IsWithinLeash(const FVector& Home, const FVector& Location, float Radius);

	virtual bool IsValidTarget_Implementation(AActor* Candidate) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Players farther than this from the enemy's start are no targets. 0: no leash. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leash", meta = (ClampMin = "0", Units = "cm", AllowPrivateAccess = "true"))
	float LeashRadius = 3000.0f;

	FVector Home = FVector::ZeroVector;
};
```

`Source/Sleepwalker/Private/SleepwalkerLeashThreatComponent.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerLeashThreatComponent.h"
#include "GameFramework/Actor.h"

bool USleepwalkerLeashThreatComponent::IsWithinLeash(const FVector& Home, const FVector& Location, const float Radius)
{
	return Radius <= 0.0f || FVector::DistSquared(Home, Location) <= FMath::Square(Radius);
}

bool USleepwalkerLeashThreatComponent::IsValidTarget_Implementation(AActor* Candidate) const
{
	// The plugin's own checks first (alive, target filter, teams), then the project's rule.
	return Super::IsValidTarget_Implementation(Candidate) && IsWithinLeash(Home, Candidate->GetActorLocation(), LeashRadius);
}

void USleepwalkerLeashThreatComponent::BeginPlay()
{
	Super::BeginPlay();
	Home = GetOwner()->GetActorLocation();
}
```

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitAI");` (the class derives from a plugin class, so the dependency is public).

How the user uses it: in the enemy Blueprint, remove **CKAI Threat Component** and add **Sleepwalker Leash Threat Component**; its Details show **Leash Radius** next to the plugin's *Profile*. The rule runs on the server at every evaluation; clients see the target change as before.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerLeashThreatComponentTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "SleepwalkerLeashThreatComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerLeashTest, "Sleepwalker.AI.Leash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerLeashTest::RunTest(const FString& Parameters)
{
	const FVector Home(100.0f, 0.0f, 0.0f);
	TestTrue(TEXT("A player inside the radius is a target"), USleepwalkerLeashThreatComponent::IsWithinLeash(Home, FVector(2000.0f, 0.0f, 0.0f), 3000.0f));
	TestFalse(TEXT("A player outside the radius is no target"), USleepwalkerLeashThreatComponent::IsWithinLeash(Home, FVector(4000.0f, 0.0f, 0.0f), 3000.0f));
	TestTrue(TEXT("Radius 0 turns the leash off"), USleepwalkerLeashThreatComponent::IsWithinLeash(Home, FVector(1.0e6f, 0.0f, 0.0f), 0.0f));
	return true;
}

#endif
```

Test the rule as a plain function; the plugin's own tests (`CoopKit.AI.Threat`) already cover how a target that stops being valid is dropped.

## 3. More enemy health with more players (the director's difficulty step)

The plugin reports the multipliers of the step for the current player count; the project's health code applies them. Put this component on the enemy pawn; it works on the server.

```cpp
// SleepwalkerScaledHealthComponent.h (excerpt)
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerScaledHealthComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleScalingChanged(const FCKAIScalingStep& Step, int32 PlayerCount);

	/** Health with one player; the director's Health Multiplier scales it. */
	UPROPERTY(EditAnywhere, Category = "Health", meta = (ClampMin = "1"))
	float BaseHealth = 100.0f;

	/** Current maximum health (read by the project's damage code and UI). */
	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	float MaxHealth = 100.0f;
};

// SleepwalkerScaledHealthComponent.cpp (excerpt)
#include "CKAIDirectorComponent.h"

void USleepwalkerScaledHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	// The director lives on the game state; without one the enemy keeps its base health.
	if (UCKAIDirectorComponent* Director = UCKAIDirectorComponent::FindDirector(this))
	{
		HandleScalingChanged(Director->GetScaling(), Director->GetPlayerCount());
		Director->OnScalingChanged.AddDynamic(this, &USleepwalkerScaledHealthComponent::HandleScalingChanged);
	}
}

void USleepwalkerScaledHealthComponent::HandleScalingChanged(const FCKAIScalingStep& Step, int32 PlayerCount)
{
	MaxHealth = BaseHealth * Step.HealthMultiplier;
}
```

The other multipliers go the same way: `DamageMultiplier` in the enemy's damage code, `SpawnBudgetMultiplier` is applied by the horde component to wave budgets by itself (`extension_map.md`, "Pacing and waves"). The steps themselves are data: the user edits them in **Director Rules** of the CoopKit Hub, no C++.

## 4. A roar when the enemy spots a player (data asset and event of the project)

The sound and its cooldown are numbers the threat profile does not have: they go into a data asset of the project, which a component on the enemy pawn reads. `OnTargetChanged` fires on the server and on every client, so each machine plays the sound itself; nothing is replicated.

```cpp
// SleepwalkerEnemyBarkData.h (excerpt)
/** What an enemy type plays when it picks a new target. */
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerEnemyBarkData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Played at the enemy when it picks a new target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark")
	TSoftObjectPtr<USoundBase> SpottedSound;

	/** Seconds before the enemy may roar again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bark", meta = (ClampMin = "0", Units = "s"))
	float CooldownSeconds = 5.0f;
};

// SleepwalkerEnemyBarkComponent.cpp (excerpt): BarkData is an EditAnywhere TObjectPtr<USleepwalkerEnemyBarkData>.
void USleepwalkerEnemyBarkComponent::BeginPlay()
{
	Super::BeginPlay();
	// On every machine: the sound is cosmetic.
	if (UCKAIThreatComponent* Threat = UCKAIThreatComponent::FindThreatComponent(GetOwner()))
	{
		Threat->OnTargetChanged.AddDynamic(this, &USleepwalkerEnemyBarkComponent::HandleTargetChanged);
	}
}

void USleepwalkerEnemyBarkComponent::HandleTargetChanged(AActor* NewTarget, AActor* OldTarget)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (NewTarget == nullptr || BarkData == nullptr || Now < NextBarkTime)
	{
		return;
	}
	NextBarkTime = Now + BarkData->CooldownSeconds;
	if (USoundBase* Sound = BarkData->SpottedSound.LoadSynchronous())
	{
		UGameplayStatics::SpawnSoundAttached(Sound, GetOwner()->GetRootComponent());
	}
}
```

Add an `IsDataValid` to the data asset when a value can be wrong (a negative cooldown), with a test as in example 2. An attack telegraph goes the same way with `OnAttackTokenChanged(bHoldsToken)`.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
