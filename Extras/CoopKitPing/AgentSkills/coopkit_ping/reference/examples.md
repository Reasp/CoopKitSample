# Examples

Replace `Sleepwalker`/`SLEEPWALKER_API` with the project's module name and API macro (module `Sleepwalker` gives `SLEEPWALKER_API`) and the copyright line with the project's. The plugin's data classes and components are `final` (only the match component is not): the examples extend it with a data asset and a component of the project, and with a subclass of the match component.

## 1. A number the plugin does not have, in a data asset of the project ("pinged enemies stay marked for five seconds")

`Source/Sleepwalker/Public/SleepwalkerPingMarkData.h`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SleepwalkerPingMarkData.generated.h"

/** How long an enemy stays marked after a teammate pings it. One asset for the game mode, set on its mark component. */
UCLASS(BlueprintType)
class SLEEPWALKER_API USleepwalkerPingMarkData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	float GetMarkSeconds() const
	{
		return MarkSeconds;
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** Seconds a pinged enemy stays marked (outlined for every player). 0 turns the mark off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mark", meta = (ClampMin = "0", Units = "s", AllowPrivateAccess = "true"))
	float MarkSeconds = 5.0f;
};
```

`Source/Sleepwalker/Private/SleepwalkerPingMarkData.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "SleepwalkerPingMarkData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SleepwalkerPingMarkData"

#if WITH_EDITOR
EDataValidationResult USleepwalkerPingMarkData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	// Shown in the Data Asset editor and by Data Validation.
	if (MarkSeconds < 0.0f)
	{
		Context.AddError(LOCTEXT("NegativeMark", "Mark Seconds is negative: it must be zero or more."));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
```

`Build.cs` of the project module: `PrivateDependencyModuleNames.Add("CoopKitPing");` (the component of example 3 includes the plugin's headers).

How the user uses it: in the Content Browser, **Add > Miscellaneous > Data Asset**, pick *Sleepwalker Ping Mark Data*, set the number, and set the asset on the mark component of the game state (example 3).

## 2. The automation test of example 1

`Source/Sleepwalker/Private/SleepwalkerPingMarkDataTest.cpp`

```cpp
// Copyright 2026 Example Games. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "SleepwalkerPingMarkData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSleepwalkerPingMarkDataTest, "Sleepwalker.Ping.Mark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSleepwalkerPingMarkDataTest::RunTest(const FString& Parameters)
{
	USleepwalkerPingMarkData* Data = NewObject<USleepwalkerPingMarkData>(GetTransientPackage());
	FDataValidationContext Good;
	TestEqual(TEXT("The defaults are valid"), Data->IsDataValid(Good), EDataValidationResult::Valid);

	// Bad data, set like a designer would in the Details panel (the property is private, so go through reflection).
	FFloatProperty* Seconds = FindFProperty<FFloatProperty>(Data->GetClass(), TEXT("MarkSeconds"));
	if (TestNotNull(TEXT("The property exists"), Seconds))
	{
		Seconds->SetPropertyValue_InContainer(Data, -1.0f);
		FDataValidationContext Context;
		Data->IsDataValid(Context);
		TArray<FText> Warnings;
		TArray<FText> Errors;
		Context.SplitIssues(Warnings, Errors);
		TestTrue(TEXT("A negative mark time is reported"), Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Mark Seconds")); }));
	}
	return true;
}

#endif
```

## 3. Using the value on the server (a component of the project)

Gameplay changes happen on the server. Put this component on the **game state** Blueprint next to the **CKPing Match Component** (both are components of the game state, so the match component exists when this one starts). It reacts to the match component's server event and marks the pinged actor.

```cpp
// SleepwalkerPingMarkComponent.h (excerpt)
UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerPingMarkComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandlePingCreated(const FCKPingEntry& Ping);

	/** How long a pinged enemy stays marked. Empty: no mark. */
	UPROPERTY(EditAnywhere, Category = "Ping")
	TObjectPtr<USleepwalkerPingMarkData> MarkData;
};

// SleepwalkerPingMarkComponent.cpp (excerpt)
void USleepwalkerPingMarkComponent::BeginPlay()
{
	Super::BeginPlay();
	// On Ping Created fires on the server only: nothing to bind on a client.
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (UCKPingMatchComponent* Match = UCKPingMatchComponent::FindMatchComponent(this))
	{
		Match->OnPingCreated.AddDynamic(this, &USleepwalkerPingMarkComponent::HandlePingCreated);
	}
}

void USleepwalkerPingMarkComponent::HandlePingCreated(const FCKPingEntry& Ping)
{
	// The target is set only when the ping type follows targets and the actor is at the ping point.
	AActor* Target = Ping.Target.Get();
	if (MarkData == nullptr || MarkData->GetMarkSeconds() <= 0.0f || Target == nullptr)
	{
		return;
	}
	if (USleepwalkerMarkedComponent* Marked = Target->FindComponentByClass<USleepwalkerMarkedComponent>())
	{
		Marked->MarkFor(MarkData->GetMarkSeconds());
	}
}
```

`USleepwalkerMarkedComponent::MarkFor` (a replicated component of the enemy Blueprint, not shown) sets a replicated flag on the server and clears it after the time; the enemy's material reads the flag on every machine. The ping itself needs no change: its type decides the icon, color and lifetime.

## 4. Who sees a ping ("only my squad sees my pings")

`UCKPingMatchComponent` is not `final` and asks `CanPlayerSeePing` on the server for every viewer of a new ping. Subclass it in the project, or override the event in a Blueprint subclass, and use the subclass as the game state's match component.

```cpp
// SleepwalkerPingMatchComponent.h (excerpt)
#include "CKPingMatchComponent.h"

UCLASS(ClassGroup = (Sleepwalker), meta = (BlueprintSpawnableComponent))
class SLEEPWALKER_API USleepwalkerPingMatchComponent : public UCKPingMatchComponent
{
	GENERATED_BODY()

public:
	virtual bool CanPlayerSeePing_Implementation(const APlayerController* Viewer, const APlayerState* Author) const override;
};

// SleepwalkerPingMatchComponent.cpp (excerpt)
bool USleepwalkerPingMatchComponent::CanPlayerSeePing_Implementation(const APlayerController* Viewer, const APlayerState* Author) const
{
	const ASleepwalkerPlayerState* ViewerState = Viewer != nullptr ? Viewer->GetPlayerState<ASleepwalkerPlayerState>() : nullptr;
	const ASleepwalkerPlayerState* AuthorState = Cast<ASleepwalkerPlayerState>(Author);
	if (ViewerState == nullptr || AuthorState == nullptr)
	{
		// Without squads the plugin's rule (the rules' Visibility) decides.
		return Super::CanPlayerSeePing_Implementation(Viewer, Author);
	}
	return ViewerState == AuthorState || ViewerState->GetSquadId() == AuthorState->GetSquadId();
}
```

When a player changes squad, call `RefreshVisibility()` on the match component (server) so live pings are shown or hidden again. Squad teams that the engine already knows (`IGenericTeamAgentInterface`) need no code: set the rules' *Visibility* to *Allies*. To forbid a player's pings instead (a stun, a cutscene), put the loose tag `CK.Ping.Block.Ping` on the player's controller, pawn or player state: the server refuses their pings with *Blocked*.

## 5. Add a C++ module to a Blueprint-only project

The build script reports "the project has no C++ target". Fix: in the editor, Tools > New C++ Class, pick **None** as the parent, name it, and finish. The editor creates `Source/<Project>/` and target files and offers to restart. Then run the build script again.

Without the editor (what an agent can do): create `Source/<Project>.Target.cs` and `Source/<Project>Editor.Target.cs` (`TargetRules` with `Type = TargetType.Game` or `TargetType.Editor`, `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, `ExtraModuleNames.Add("<Project>")`), `Source/<Project>/<Project>.Build.cs`, `Source/<Project>/<Project>.cpp` with `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, <Project>, "<Project>");`, and add a `Modules` entry (`Name`, `Type: Runtime`, `LoadingPhase: Default`) to the `.uproject`. Ask the user to close the editor first.
