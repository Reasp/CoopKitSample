# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's.

## 1. A number per attack in a move set subclass ("poise damage")

The attacks of a combo are `FCKCombatAttackStep` structs owned by the plugin. A value of your own per attack goes into an array of the subclass, one entry per step, in combo order.

`Source/Sleepwalker/Public/SleepwalkerMoveSetData.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CKCombatMoveSetData.h"
#include "SleepwalkerMoveSetData.generated.h"

/** A CoopKit Combat move set with the poise damage of each attack. Create move sets of this class where poise matters. */
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerMoveSetData : public UCKCombatMoveSetData
{
	GENERATED_BODY()

public:
	/** Poise damage of the attack at Step (0-based); 0 for a step without a value. */
	float GetPoiseDamage(const int32 Step) const
	{
		return PoiseDamage.IsValidIndex(Step) ? PoiseDamage[Step] : 0.0f;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Poise damage of each attack, in combo order: one value per step of Steps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "0", AllowPrivateAccess = "true"))
	TArray<float> PoiseDamage;
};
```

`Source/Sleepwalker/Private/SleepwalkerMoveSetData.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerMoveSetData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SleepwalkerMoveSetData"

#if WITH_EDITOR
EDataValidationResult USleepwalkerMoveSetData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// Shown in the card's Validation tab and by the CoopKit Hub's Check Project.
	if (PoiseDamage.Num() != GetStepCount())
	{
		Context.AddError(FText::Format(LOCTEXT("PoiseCount", "Poise Damage has {0} values for {1} steps: give every attack one value."),
			FText::AsNumber(PoiseDamage.Num()), FText::AsNumber(GetStepCount())));
		Result = EDataValidationResult::Invalid;
	}
	for (const float Value : PoiseDamage)
	{
		if (Value < 0.0f)
		{
			Context.AddError(LOCTEXT("NegativePoise", "Poise Damage has a negative value: it must be zero or more."));
			Result = EDataValidationResult::Invalid;
			break;
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PublicDependencyModuleNames.Add("CoopKitCombat");` (the class derives from a plugin class, so the dependency is public).

How the user uses it: in the Content Browser, **Add > Miscellaneous > Data Asset**, pick *Sleepwalker Move Set Data*; it opens in the CoopKit Combat card (Properties, Preview, Parts, Summary, Validation). The hub's **New Move Set** wizard creates the plugin's own class; tell the user. Give the move set to a profile or a weapon's attack ability as usual.

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerMoveSetDataTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SleepwalkerMoveSetData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerMoveSetDataTest, "Sleepwalker.Combat.PoiseDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerMoveSetDataTest::RunTest(const FString& Parameters)
{
	USleepwalkerMoveSetData* Data = NewObject<USleepwalkerMoveSetData>(GetTransientPackage());

	// A new move set has the built-in three steps and no poise values: the validation names the count.
	FDataValidationContext Context;
	Data->IsDataValid(Context);
	TArray<FText> Warnings;
	TArray<FText> Errors;
	Context.SplitIssues(Warnings, Errors);
	TestTrue(TEXT("A missing value per step is reported"), Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Poise Damage")); }));

	// One value per step, set like a designer would in the Details panel (the property is private: go through reflection).
	FArrayProperty* Poise = FindFProperty<FArrayProperty>(Data->GetClass(), TEXT("PoiseDamage"));
	if (TestNotNull(TEXT("The property exists"), Poise))
	{
		TArray<float>& Values = *Poise->ContainerPtrToValuePtr<TArray<float>>(Data);
		Values = { 5.0f, 5.0f, 20.0f };
		TestEqual(TEXT("The finisher's poise damage"), Data->GetPoiseDamage(2), 20.0f);
		TestEqual(TEXT("A step without a value gives 0"), Data->GetPoiseDamage(7), 0.0f);
	}
	return true;
}

#endif
```

## 3. Using the value on the server (a component of the project)

Gameplay changes happen on the server. Put a component on the fighter's pawn next to the CoopKit Combat component; it binds to the server event of a landed hit and reads the value of the attack that landed.

```cpp
// SleepwalkerPoiseComponent.h (excerpt)
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerPoiseComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleDamageDealt(UCKCombatComponent* Combat, AActor* Target, float Damage, ECKCombatHitOutcome Outcome);
};

// SleepwalkerPoiseComponent.cpp (excerpt)
void USleepwalkerPoiseComponent::BeginPlay()
{
	Super::BeginPlay();
	// The events fire on the server only; nothing to bind elsewhere.
	if (GetOwner()->HasAuthority())
	{
		if (UCKCombatComponent* Combat = UCKCombatComponent::FindCombatComponent(GetOwner()))
		{
			Combat->OnDamageDealt.AddDynamic(this, &USleepwalkerPoiseComponent::HandleDamageDealt);
		}
	}
}

void USleepwalkerPoiseComponent::HandleDamageDealt(UCKCombatComponent* Combat, AActor* Target, const float Damage, const ECKCombatHitOutcome Outcome)
{
	// The combo of the running attack: the weapon ability's move set, else the profile's.
	const USleepwalkerMoveSetData* MoveSet = Cast<USleepwalkerMoveSetData>(Combat->ResolveMoveSet(Combat->GetActiveAttackAbility()));
	if (MoveSet == nullptr || Outcome == ECKCombatHitOutcome::Parried)
	{
		return;
	}
	const float Poise = MoveSet->GetPoiseDamage(Combat->GetComboStep());
	// ... lower the target's poise (an attribute through a Gameplay Effect, or a replicated value on your own component).
}
```

## 4. A damage type of your own ("fire damage")

For every hit: make a Gameplay Effect (a Blueprint child of *Gameplay Effect*, or C++) that reads the SetByCaller `CK.Combat.SetByCaller.Damage` the way `UCKCombatDamageEffect` does, add what your damage type needs (an execution calculation with resistances, an asset tag `Damage.Fire`), and set it as **Damage Effect** in *Project Settings > CoopKit Combat*. With the plugin's attribute set, add the damage to *Incoming Damage* so armor still applies.

For some attacks only: keep the plugin's damage effect, add a per-step array like example 1 (for example `TArray<float> FireDamage`), and on the server in `On Damage Dealt` apply a second effect to the target's Ability System Component with that amount:

```cpp
UAbilitySystemComponent* Source = Combat->GetAbilitySystem();
UAbilitySystemComponent* TargetSystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
if (Source != nullptr && TargetSystem != nullptr && FireDamage > 0.0f)
{
	FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(FireDamageEffectClass, 1.0f, Source->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(SleepwalkerTags::SetByCaller_FireDamage, FireDamage);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetSystem);
}
```

The project module then also needs `"GameplayAbilities"`, `"GameplayTags"` and `"GameplayTasks"` in its `Build.cs`.

## 5. A hit effect of your own (no C++)

1. *Project Settings > Gameplay Tags*: add `GameplayCue.Sleepwalker.Hit.Axe`.
2. Content Browser: **Add > Blueprint Class**, parent *Gameplay Cue Notify Static* (or *Gameplay Cue Notify Actor* for a lasting effect), named `GC_Sleepwalker_Hit_Axe`; set its *Gameplay Cue Tag* to the new tag and spawn the spark and the sound in *On Execute*. Keep it in a folder the project's Gameplay Cue paths include: `GameplayCueNotifyPaths` in `[/Script/GameplayAbilities.AbilitySystemGlobals]` of `Config/DefaultGame.ini`, or all of `/Game` when the project lists none. Elsewhere the engine never plays it and warns "Invalid GameplayCue Path".
3. Open the move set in the CoopKit Hub (or double-click it): in the card's **Parts** tab, **Replace** on *Step N Hit Cue* and pick the new notify. The step now plays the new tag on a hit; the Parts tab shows the tag under the notify's name.

## 6. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
